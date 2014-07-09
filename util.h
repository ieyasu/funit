#ifndef __FUNIT_UTIL_H__
#define __FUNIT_UTIL_H__

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define TRUE (-1)
#define FALSE (0)


#define NEW(type)  (type *)fu_alloc(sizeof(type))
#define NEW0(type) (type *)fu_alloc0(sizeof(type))

/* Wrappers for malloc() and realloc() that abort on failure.
 */
void *fu_alloc(size_t bytes);
void *fu_alloc0(size_t bytes);
void *fu_realloc(void *ptr, size_t bytes);

// XXX for now...
#define fu_alloca(bytes) fu_alloc(bytes)
#define fu_freea(ptr) free(ptr)


char *fu_strndup(char *str, size_t len);
char *fu_strdup(char *str);


struct Buffer {
    char *s;     // the malloc()'d buffer space
    size_t size; // size of the buffer
    size_t i;    // current write index in the buffer
};

void init_buffer(struct Buffer *buf, size_t initsize);
void grow_buffer(struct Buffer *buf);
void buffer_ncat(struct Buffer *buf, const char *app, size_t applen);
void buffer_cat(struct Buffer *buf, const char *app);
void free_buffer(struct Buffer *buf);


int fu_stat(const char *path, struct stat *bufp);
int fu_isdir(const char *path);
int fu_pathcat(char *buf, size_t bufsize, const char *path1, const char *path2);

#endif // __FUNIT_UTIL_H__
