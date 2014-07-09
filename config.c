/* FUnit config file parser.
 */
#include "funit.h"
#include "util.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define PATH_BUF_LEN (PATH_MAX + 1)

static int try_open_file(const char *path, struct ParseState *ps)
{
    struct stat statbuf;
    if (fu_stat(path, &statbuf) || statbuf.st_size <= 0) return -1;

    return open_file_for_parsing(path, ps);
}

// Look in current dir, $HOME, /etc/funitrc, then give up.
static int find_config_file(char *buf, struct ParseState *ps)
{
    // try current directory
    strcpy(buf, ".funit");
    if (!try_open_file(buf, ps))
        return 0;

    // try $HOME
    char *h = getenv("HOME");
    if (h) {
        if (fu_pathcat(buf, PATH_BUF_LEN, h, ".funit")) {
            fputs("error concatenating 'HOME' env var with '.funit'", stderr);
            abort();
        }

        if (!try_open_file(buf, ps)) return 0;
    } else {
        fputs("Warning: environment variable 'HOME' not set\n", stderr);
    }

    // try /etc/funitrc
    strcpy(buf, "/etc/funitrc");
    if (!try_open_file(buf, ps))
        return 0;

    return -1;
}

static void key_end_finder(struct ParseState *ps)
{
    while (ps->next_pos < ps->next_line_pos) {
        char c = *ps->next_pos;
        if (c < 'A' || (c > 'Z'  && c < '_') || (c > '_' && c < 'a') || c > 'z')
            return;
        ps->next_pos++;
    }
}

static void value_end_finder(struct ParseState *ps)
{
    while (ps->next_pos < ps->next_line_pos) {
        switch (*ps->next_pos) {
        case ' ':
        case '\t':
        case '#':
        case '\r':
        case '\n':
            return;
        }
        ps->next_pos++;
    }
}

static char *next_quoted_string(struct ParseState *ps, size_t *len)
{
    char quote_char = *ps->next_pos;
    assert(quote_char == '"' || quote_char == '\'');
    ps->read_pos = ps->next_pos + 1;
    ps->next_pos = ps->read_pos;
    while (ps->next_pos < ps->next_line_pos) {
        if (*ps->next_pos == quote_char) {
            *len = ps->next_pos - ps->read_pos;
            ps->next_pos++;
            return ps->read_pos;
        }
        ps->next_pos++;
    }
    parse_fail(ps, ps->next_pos, "expected close quote at end of string");
    return END_OF_LINE;
}

static int parse_config_setting(struct ParseState *ps, struct Config *conf)
{
    // config key
    size_t keylen;
    char *key = next_thing(ps, &keylen, key_end_finder);
    if (key == END_OF_LINE) {
        parse_fail(ps, ps->next_pos, "config key or comment ('#') expected");
        return -1;
    }

    // around the '='
    skip_next_ws(ps);
    if (*ps->next_pos != '=') {
        parse_fail(ps, ps->next_pos, "'=' expected after config key");
        return -1;
    }
    ps->next_pos++;
    skip_next_ws(ps);

    // config value
    size_t valuelen;
    char *value = NULL;
    if (*ps->next_pos == '"' || *ps->next_pos == '\'') {
        value = next_quoted_string(ps, &valuelen);
    } else {
        ps->read_pos = ps->next_pos;
        value = next_thing(ps, &valuelen, value_end_finder);
    }
    if (value == END_OF_LINE || valuelen == 0) {
        parse_fail(ps, ps->next_pos, "missing value");
        return -1;
    }

    // check for trailing text
    skip_next_ws(ps);
    switch (*ps->next_pos) {
    case '\r':
    case '\n':
    case '#':
        break;
    default:
        parse_fail(ps, ps->next_pos, "unexpected text after config value");
        return -1;
    }

    // store value
    value = fu_strndup(value, valuelen);
    if (       !strncmp("build",        key, 5)) {
        conf->build = value;
    } else if (!strncmp("tempdir",      key, 7)) {
        conf->tempdir = value;
    } else if (!strncmp("fortran_ext",  key, 11)) {
        conf->fortran_ext = value;
    } else if (!strncmp("template_ext", key, 12)) {
        conf->template_ext = value;
    } else {
        free(value);
        struct Buffer msg;
        init_buffer(&msg, 32);
        buffer_cat(&msg, "unknown config key \"");
        buffer_ncat(&msg, key, keylen);
        buffer_cat(&msg, "\"");
        msg.s[msg.i] = '\0';
        parse_fail(ps, key, msg.s);
        free_buffer(&msg);
        return -1;
    }
    return 0;
}

/* Read config file line-by-line ignoring comments and empty lines, looking for
 * lines like:
 *     <var> = <value> [#...]
 * where <var> is one of the known config names, and <value> is a 
 */
static int parse_config(struct ParseState *ps, struct Config *conf)
{
    while (next_line(ps)) {
        skip_next_ws(ps);
        char c = *ps->next_pos;
        switch (c) {
        case '\r':
        case '\n':
        case '#':
            continue; // start of comment or end of line
        default:
            if (c < 'A' || (c > 'Z' && c < 'a') || c > 'z') {
                syntax_error(ps);
                return -1;
            }
            break;
        }
        if (parse_config_setting(ps, conf)) {
            return -1;
        }
    }
    return 0;
}

static char *find_tempdir(void)
{
    char *e = getenv("TMPDIR");
    if (e && *e != '\0') {
        return fu_strdup(e);
    }

    e = getenv("TEMP");
    if (e && *e != '\0') {
        return fu_strdup(e);
    }

    if (fu_isdir("/tmp")) {
        return fu_strdup("/tmp");
    }

    if (fu_isdir("/var/tmp")) {
        return fu_strdup("/var/tmp");
    }

    fputs("error: no temporary directory found, you'll need to specify one in the FUnit config file\n", stderr);
    abort();
    return NULL;
}

static void set_defaults(struct Config *conf)
{
    if (!conf->build) {
        conf->build = fu_strdup("make {{EXE}}");
    }

    if (!conf->tempdir) {
        conf->tempdir = find_tempdir();
    }

    if (!conf->fortran_ext) {
        conf->fortran_ext = fu_strdup(".F90");
    }

    if (!conf->template_ext) {
        conf->template_ext = fu_strdup(".fun");
    }
}

int read_config(struct Config *conf)
{
    memset(conf, 0, sizeof(struct Config));

    char config_path[PATH_BUF_LEN];
    struct ParseState ps;
    if (find_config_file(config_path, &ps)) {
        fprintf(stderr, "error: funit config file not found\n");
        return -1;
    }

    int r = parse_config(&ps, conf);
    close_parse_file(&ps);

    if (r == 0) {
        set_defaults(conf);
    }

    return r;
}

void free_config(struct Config *conf)
{
    assert(conf != NULL);

    free(conf->build);
    free(conf->tempdir);
    free(conf->fortran_ext);
    free(conf->template_ext);
}