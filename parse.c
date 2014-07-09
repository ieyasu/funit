/* parse.c - generic parsing routines.
 */
#include "funit.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#define MAX_PARSE_SIZE (8 * 1024 * 1024)

void parse_fail(struct ParseState *ps, const char *col, const char *message)
{
    assert(ps->line_pos != NULL);

    // file:line:
    assert(ps->path != NULL);
    fprintf(stderr, "%s:%li:\n\n", ps->path, ps->lineno);
    // code
    fwrite(ps->line_pos, ps->next_line_pos - ps->line_pos, 1, stderr);
    fputs("\n", stderr);
    // column indicator
    if (col) {
        int n = col - ps->line_pos;
        int spaces = n > 2 ? n - 2 : n;
        while (spaces > 0) {
            fputs(" ", stderr);
            spaces--;
        }
        if (n > 2)
            fputs("--", stderr);
        fputs("^", stderr);
        if (n <= 2)
            fputs("--", stderr);
        fputs("\n", stderr);
    }
    // error message
    fprintf(stderr, "Error: %s\n", message);
}

void parse_vfail(struct ParseState *ps, const char *col, const char *format, ...)
{
    char message[256];
    va_list ap;

    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);

    parse_fail(ps, col, message);
}

void syntax_error(struct ParseState *ps)
{
    parse_fail(ps, ps->read_pos, "syntax error");
}

/* Returns a pointer to the next line of data, or NULL if at last line.
 * Does not handle comments.
 */
char *next_line(struct ParseState *ps)
{
    char *prev_line;

    assert(ps->file_buf != NULL);

    if (ps->line_pos) {
        assert(ps->next_line_pos >= ps->line_pos ||
               ps->line_pos == ps->file_end);
        prev_line = ps->line_pos;
        ps->line_pos = ps->next_line_pos;
        // read past \r\n
        while (ps->line_pos < ps->file_end) {
            if (*ps->line_pos == '\n') {
                ps->line_pos++;
                ps->lineno++;
                break;
            } else if (*ps->line_pos == '\r') {
                if (ps->line_pos + 1 < ps->file_end &&
                    *(ps->line_pos + 1) == '\n')
                    ps->line_pos += 2;
                else
                    ps->line_pos++;
                ps->lineno++;
                break;
            }
            ps->line_pos++;
        }
        if (ps->line_pos >= ps->file_end) {
            ps->line_pos = prev_line;
            ps->lineno--;
            ps->read_pos = ps->file_end;
            return NULL;
        }
    } else { // very first line
        ps->line_pos = ps->file_buf;
        ps->lineno = 1;
    }
    ps->read_pos = ps->next_pos = ps->line_pos;

    // find next \r\n
    ps->next_line_pos = ps->line_pos;
    while (ps->next_line_pos < ps->file_end) {
        switch (*ps->next_line_pos) {
        case '\n':
        case '\r':
            goto eol;
        default:
            ps->next_line_pos++;
            break;
        }
    }
 eol:
    return ps->line_pos;
}

/* Skip whitespace starting from ps->next_pos.  ps->next_pos will be set to the
 * next non-whitespace character, and is returned.
 */
char *skip_next_ws(struct ParseState *ps)
{
    assert(ps->next_pos != NULL);

    while (ps->next_pos < ps->next_line_pos) {
        switch (*ps->next_pos) {
        case ' ':
        case '\t':
            ps->next_pos++;
            break;
        default:
            goto eol;
        }
    }
 eol:
    return ps->next_pos;
}

char *next_thing(struct ParseState *ps, size_t *len, end_finder_fun end_fun)
{
    skip_next_ws(ps);

    end_fun(ps);

    if (ps->next_pos > ps->read_pos) {
        if (len)
            *len = ps->next_pos - ps->read_pos;
        return ps->read_pos;
    }
    return END_OF_LINE;
}

/* Opens and mmap()s a file for parsing.  Returns a ParseState struct to keep
 * track of this open file.
 */
int open_file_for_parsing(const char *path, struct ParseState *ps)
{
    struct stat statbuf;

    ps->fd = open(path, O_RDONLY);
    if (ps->fd == -1) {
        fprintf(stderr, "Opening file %s: %s\n", path, strerror(errno));
        return -1;
    }
    ps->path = path;

    if (fstat(ps->fd, &statbuf)) {
        fprintf(stderr, "Stat file %s: %s\n", path, strerror(errno));
        goto close_it;
    }
    if (statbuf.st_size < 1) {
        fprintf(stderr, "File %s is empty!\n", path);
        goto close_it;
    } else if (statbuf.st_size > MAX_PARSE_SIZE) {
        fprintf(stderr, "File %s is too big (> %lu bytes)!\n", path,
                (unsigned long)MAX_PARSE_SIZE);
        goto close_it;
    }
    ps->bufsize = (size_t)statbuf.st_size;

    ps->file_buf = (char *)mmap(NULL, ps->bufsize, PROT_READ, MAP_SHARED,
                                ps->fd, 0);
    if (!ps->file_buf) {
        fprintf(stderr, "Mapping file %s: %s\n", path, strerror(errno));
        goto close_it;
    }
    ps->file_end = ps->file_buf + ps->bufsize;
    ps->line_pos = ps->next_line_pos = NULL;
    ps->read_pos = ps->next_pos = NULL;
    ps->lineno = 0;
    return 0;

 close_it:
    close_parse_file(ps);
    return -1;
}

void close_parse_file(struct ParseState *ps)
{
    assert(ps != NULL);

    if (ps->file_buf) {
        if (!munmap(ps->file_buf, ps->bufsize)) {
            ps->file_buf = NULL;
        } else {
            fprintf(stderr, "Unmapping file %s: %s\n", ps->path,
                    strerror(errno));
            abort();
        }
    }

    if (ps->fd && !close(ps->fd)) {
        ps->fd = 0;
    } else {
        fprintf(stderr, "Closing file %s: %s\n", ps->path, strerror(errno));
        abort();
    }
}
