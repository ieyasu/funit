/* Utility functions for FUnit.
 */
#include "funit.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

char *fu_strndup(const char *str, size_t len)
{
    char *copy = malloc(len + 1);
    strcpy(copy, str);
    return copy;
}

char *fu_strdup(const char *str)
{
    size_t len = strlen(str);
    return fu_strndup(str, len);
}

int fu_file_exists(const char *path)
{
    struct stat sb;

    if (stat(path, &sb)) {
        if (errno != ENOENT) {
            fprintf(stderr, "Error stat()ing %s: %s\n", path, strerror(errno));
        }
        return FALSE;
    }

    if (sb.st_mode & (S_IFREG | S_IFLNK)) {
        return TRUE;
    }
    return FALSE;
}

void sb_init(struct StringBuffer *sb, size_t length)
{
    sb->s = NEWA(char, length);
    sb->cap = length;
    sb->len = 0;
}

void sb_free(struct StringBuffer *sb)
{
    free(sb->s);
}

/* Make sure there's enough capacity for at least the given number of chars
 * past the current end of the buffer, allocating if necessary.
 */
void sb_ensure(struct StringBuffer *sb, size_t at_least)
{
    if (sb->cap >= sb->len + at_least) return;

    while (sb->cap < sb->len + at_least) sb->cap += sb->cap;

    sb->s = realloc(sb->s, sb->cap);
    if (!sb->s) abort(); // XXX or handle allocation better?
}

void sb_add_char(struct StringBuffer *sb, char c)
{
    sb_ensure(sb, 1);
    sb->s[sb->len++] = c;
}

void sb_add_nstr(struct StringBuffer *sb, const char *s, size_t len)
{
    sb_ensure(sb, len);
    memcpy(sb->s + sb->len, s, len);
    sb->len += len;
}

void sb_add_str(struct StringBuffer *sb, const char *s)
{
    sb_add_nstr(sb, s, strlen(s));
}
