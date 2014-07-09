/* util.c - general utility functions.
 */
#include "funit.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

/* MEMORY ALLOCATION
 */

static void check_alloc_error(void *p, size_t bytes)
{
    if (!p) {
        fprintf(stderr, "error: ran out of memory try to allocate %lu bytes\n",
                (unsigned long)bytes);
        abort();
    }
}

void *fu_alloc(size_t bytes)
{
    void *p = malloc(bytes);
    check_alloc_error(p, bytes);
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
    check_alloc_error(p, bytes);
    return p;
}


/* GENERIC STRING FUNCTIONS
 */

char *fu_strndup(char *str, size_t len)
{
    char *copy = fu_alloc(len + 1);
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
    buf->s[0] = '\0';
    buf->size = initsize;
    buf->i = 0;
}

void grow_buffer(struct Buffer *buf)
{
    buf->size = 2 * buf->size;
    buf->s = fu_realloc(buf->s, buf->size);
}

/* Given the Buffer, append app at the current write position, first growing
 * the buffer if needed.
 */
void buffer_ncat(struct Buffer *buf, const char *app, size_t applen)
{
    if (buf->i + applen + 1 > buf->size) {
        grow_buffer(buf);
    }

    for (size_t j = 0; j < applen; j++) {
        buf->s[buf->i] = app[j];
        if (app[j] == '\0') return;
        buf->i++;
    }
    buf->s[buf->i] = '\0';
}

/* Appends the given string to the buffer at the current write position,
 * growing the buffer if necessary.
 */
void buffer_cat(struct Buffer *buf, const char *app)
{
    size_t applen = strlen(app);
    buffer_ncat(buf, app, applen);
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


/* OTHER FUNCTIONS
 */
void fu_error3(const char *prefix, const char *s, size_t len,
               const char *postfix)
{
    fputs(prefix, stderr);
    fwrite(s, len, 1, stderr);
    fputs(postfix, stderr);
}

int fu_system(const char *command)
{
    int ret = system(command);
    if (ret == -1) {
        fprintf(stderr, "error: system(): %s\n", strerror(errno));
    } else if (ret == 127) {
        fputs("error: system(): failed to execute shell\n", stderr);
        ret = -1;
    }
    return ret;
}
