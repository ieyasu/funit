/* util.c - general utility functions.
 */
//#include "funit.h"
#include "util.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

/* MEMORY ALLOCATION
 */

static void check_alloc_error(void *p)
{
    if (!p) {
        fputs("error: ran out of memory\n", stderr);
        abort();
    }
}

void *fu_alloc(size_t bytes)
{
    void *p = malloc(bytes);
    check_alloc_error(p);
    return p;
}

void *fu_alloc0(size_t bytes)
{
    void *p = fu_alloc(bytes);
    memset(p, 0, bytes);
    return p;
}

void *fu_realloc(void *ptr, size_t bytes)
{
    void *p = realloc(ptr, bytes);
    check_alloc_error(p);
    return p;
}


/* GENERIC STRING FUNCTIONS
 */

char *fu_strndup(char *str, size_t len)
{
    char *copy = malloc(len + 1);
    strncpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

char *fu_strdup(char *str)
{
    size_t len = strlen(str);
    return fu_strndup(str, len);
}


/* BUFFERS
 */

void init_buffer(struct Buffer *buf, size_t initsize)
{
    buf->s = fu_alloc(initsize);
    buf->size = initsize;
    buf->i = 0;
}

void grow_buffer(struct Buffer *buf)
{
    buf->size = 2 * buf->size;
    buf->s = fu_realloc(buf->s, buf->size);
}

/* Given the buffer base pointer buf, the current write position bufi and
 * the allocated buffer size bufsize, append app at the current write position,
 * first growing the buffer if needed.  bufi is incremented past app, and
 * bufsize will reflect the new buffer size if it grew.
 *
 * Returns a pointer to the buffer which could be different
 */
void buffer_append(struct Buffer *buf, const char *app)
{
    size_t applen = strlen(app);

    if (buf->i + applen > buf->size) {
        grow_buffer(buf);
    }

    memcpy(buf->s+buf->i, app, applen);
    buf->i += applen;
}

void free_buffer(struct Buffer *buf)
{
    free(buf->s);
}


/* PATH OPERATIONS
 */

int fu_stat(const char *path, struct stat *bufp)
{
    if (stat(path, bufp)) {
        if (errno == ENOENT) {
            return -1;
        } else {
            fprintf(stderr, "error stat()ing '%s': %s\n",
                    path, strerror(errno));
            abort();
        }
    }
    return 0;
}

int fu_isdir(const char *restrict path)
{
    struct stat buf;

    if (!fu_stat(path, &buf) && (buf.st_mode & S_IFDIR)) return TRUE;

    return FALSE;
}

static const char fu_pathsep =
#ifdef _WIN32
                            '\\';
#else
                            '/';
#endif

int fu_pathcat(char *buf, size_t bufsize, const char *path1, const char *path2)
{
    size_t len1 = strlen(path1), len2 = strlen(path2);
    if (bufsize < len1 + len2 + 2) {
        // XXX buf too small - set errno?
        return -1;
    }

    memcpy(buf, path1, len1);

    if (path1[len1 - 1] != fu_pathsep) {
        buf[len1] = fu_pathsep;
        len1++;
    }

    strcpy(buf+len1, path2);

    return 0;
}
