/* FUnit config file parser.
 */
#include "funit.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

static char *fu_strndup(char *str, size_t len)
{
    char *copy = malloc(len + 1);
    strcpy(copy, str);
    return copy;
}

static char *fu_strdup(char *str)
{
    size_t len = strlen(str);
    return fu_strndup(str, len);
}

static int try_open_file(const char *path, struct ParseState *ps)
{
    struct stat statbuf;
    if (stat(path, &statbuf)) {
        if (errno == ENOENT) {
            return -1;
        } else {
            fprintf(stderr, "error stat()ing '%s': %s\n",
                    path, strerror(errno));
            abort();
        }
    }
    if (statbuf.st_size <= 0) {
        return -1;
    }

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
        size_t hlen = strlen(h);
        if (hlen > PATH_MAX - 7) {
            fprintf(stderr, "Warning: value for 'HOME' environment variable is too long\n");
        } else {
            strcpy(buf, h);
            if (buf[hlen-1] != '/') {
                buf[hlen] = '/';
                hlen++;
            }
            strcpy(buf+hlen, ".funit");
            if (!try_open_file(buf, ps))
                return 0;
        }
    } else {
        fprintf(stderr, "Warning: environment variable 'HOME' not set\n");
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
        if (c < 'A' || (c > 'Z' && c < 'a') || c > 'z' || c == '_')
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
    while (ps->next_pos < ps->next_line_pos) {
        if (*ps->next_pos == quote_char) {
            *len = ps->next_pos - ps->read_pos;
            return ps->read_pos;
        }
        ps->next_pos++;
    }
    parse_fail(ps, ps->next_pos, "expected close quote at end of string");
    return NULL;
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
    skip_next_ws(ps);

    // config value
    size_t valuelen;
    char *value = NULL;
    if (*ps->next_pos == '"' || *ps->next_pos == '\'') {
        value = next_quoted_string(ps, &valuelen);
    } else {
        value = next_thing(ps, &valuelen, value_end_finder);
    }
    if (!value)
        return -1;

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
    if (!strcmp("build", key)) {
        conf->build = value;
    } else if (!strcmp("fortran_ext", key)) {
        conf->fortran_ext = value;
    } else if (!strcmp("template_ext", key)) {
        conf->template_ext = value;
    } else {
        free(value);
        parse_vfail(ps, key, "unknown config key '%s'", key);
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

static void set_defaults(struct Config *conf)
{
    if (!conf->build) {
        // XXX set default build command
        fprintf(stderr, "no build command given and no default yet!\n");
        abort();
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
    char config_path[PATH_MAX + 1];
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
    free(conf->fortran_ext);
    free(conf->template_ext);
}
