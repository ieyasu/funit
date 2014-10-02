/* FUnit config file parser.
 */
#include "funit.h"
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

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
        if (c < 'A' || (c > 'Z' && c < 'a' &&  c != '_') || c > 'z')
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

static char *next_quoted_string(struct ParseState *ps, size_t *len,
                                int *escapes)
{
    char quote_char = *ps->read_pos;
    assert(quote_char == '"' || quote_char == '\'');
    ps->next_pos = ++ps->read_pos;
    while (ps->next_pos < ps->next_line_pos) {
        if (*ps->next_pos == '\\' && (ps->next_pos[1] == '"' ||
                                      ps->next_pos[1] == '\'')) {
            *escapes = TRUE;
            ps->next_pos++; // skip the '\', *and* skip the quote below
        } else if (*ps->next_pos == quote_char) {
            *len = ps->next_pos - ps->read_pos;
            ps->next_pos++;
            return ps->read_pos;
        }
        ps->next_pos++;
    }
    parse_fail(ps, ps->next_pos, "expected close quote at end of string");
    return NULL;
}

static char *copy_escaped_string(const char *s, size_t len)
{
    char *buf = malloc(len);
    size_t srci = 0, desti = 0;
    while (srci < len) {
        if (s[srci] == '\\' && (s[srci + 1] == '"' || s[srci + 1] == '\'')) {
            srci++; // skip past backslash to just copy quote
        }
        buf[desti++] = s[srci++];
    }
    buf[desti] = '\0';
    return buf;
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
    if (*ps->next_pos == '=') {
        ps->next_pos++;
        skip_next_ws(ps);
    } else {
        parse_fail(ps, ps->next_pos, "'=' expected after config key");
        return -1;
    }

    // config value
    size_t valuelen;
    char *value = NULL;
    int escapes = 0;
    ps ->read_pos = ps->next_pos;
    if (*ps->read_pos == '"' || *ps->read_pos == '\'') {
        value = next_quoted_string(ps, &valuelen, &escapes);
    } else {
        value = next_thing(ps, &valuelen, value_end_finder);
    }
    if (!value) return -1;

    // check for trailing text
    skip_next_ws(ps);
    switch (*ps->next_pos) {
    case '\r':
    case '\n':
    case '#':
        break;
    default:
        parse_fail(ps, ps->next_pos, "unexpected text after config value -- add a comment or surround it with quotes (\" or ')");
        return -1;
    }

    // store value
    if (escapes) {
        value = copy_escaped_string(value, valuelen);
    } else {
        value = fu_strndup(value, valuelen);
    }

    if (keylen == 5 && !strncmp("build", key, 5)) {
        conf->build = value;
        conf->build_len = valuelen;
    } else if (keylen == 11 && !strncmp("fortran_ext", key, 11)) {
        conf->fortran_ext = value;
        conf->fortran_ext_len = valuelen;
    } else if (keylen == 12 && !strncmp("template_ext", key, 12)) {
        conf->template_ext = value;
        conf->template_ext_len = valuelen;
    } else {
        free(value);

        fprintf(stderr, "%s:%li: Error: unknown config key '",
                ps->path, ps->lineno);
        fwrite(key, 1, keylen, stderr);
        fputs("' in\n", stderr);
        fwrite(ps->line_pos, 1, ps->next_line_pos - ps->line_pos, stderr);
        fputs("\n", stderr);

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
            // config keys always start with a letter
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

#define SELF_STRNDUP(var) var = fu_strndup(var, var ## _len)

static void set_defaults(struct Config *conf)
{
    if (!conf->build) {
        conf->build = fu_strdup("make {{EXE}}");
        conf->build_len = 12;
    } else {
        SELF_STRNDUP(conf->build);
    }
    conf->build_fragments = parse_build_rule(conf->build);

    if (!conf->fortran_ext) {
        conf->fortran_ext = fu_strdup(".F90");
        conf->fortran_ext_len = 4;
    } else {
        SELF_STRNDUP(conf->fortran_ext);
    }

    if (!conf->template_ext) {
        conf->template_ext = fu_strdup(".fun");
        conf->template_ext_len = 4;
    } else {
        SELF_STRNDUP(conf->template_ext);
    }
}

int read_config(struct Config *conf)
{
    char config_path[PATH_MAX + 1];
    struct ParseState ps;
    int r;

    memset(conf, 0, sizeof(struct Config));

    if (find_config_file(config_path, &ps)) {
        fprintf(stderr, "FUnit: warning: config file not found\n");
        r = 0;
    } else {
        r = parse_config(&ps, conf);
        close_parse_file(&ps);
    }

    if (r == 0) {
        set_defaults(conf);
    }

    return r;
}

void free_config(struct Config *conf)
{
    assert(conf != NULL);

    free(conf->build);
    free_build_fragments(conf->build_fragments);
    free(conf->fortran_ext);
    free(conf->template_ext);
}
