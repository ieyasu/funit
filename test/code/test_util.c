#include "../../util.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

void test_allocation(void)
{
    int i;

    // test fu_alloc
    char *p = fu_alloc(20);
    assert(p != NULL);
    memset(p, 99, 20); // make sure we can access all of the allocated region
    free(p); // musn't crash

    // test fu_realloc
    p = fu_alloc(10);
    assert(p != NULL);
    memset(p, 99, 10); // make sure we can access all of the allocated region
    p = fu_realloc(p, 20);
    for (i = 0; i < 10; i++) { // make sure previous contents remains
        assert(p[i] == 99);
    }
    memset(p+10, 77, 10); // make sure we can write new space
    free(p); // musn't crash
               

    // test fu_alloc0
    p = fu_alloc0(20);
    assert(p != NULL);
    for (i = 0; i < 20; i++) { // make sure region is zero'd out
        assert(p[i] == 0);
    }
    memset(p, 99, 20); // make sure we can write all of the allocated region
    free(p); // musn't crash

    // test NEW macro
    long *l = NEW(long);
    assert(l != NULL);
    *l = -1;
    free(l);

    // test NEW0 macro
    l = NEW0(long);
    assert(l != NULL);
    assert(*l == 0);
    *l = -1;
    free(l);

    // test fu_alloca and fu_freea
    p = fu_alloca(10);
    assert(p != NULL);
    memset(p, 55, 10);
    fu_freea(p);
}

static void test_string_ops(void)
{
    // test fu_strndup
    char *s = fu_strndup("foo", 2);
    assert(s != NULL);
    assert(strlen(s) == 2);
    assert(strcmp("fo", s) == 0);
    free(s);

    s = fu_strndup("bar", 3);
    assert(s != NULL);
    assert(strlen(s) == 3);
    assert(strcmp("bar", s) == 0);
    free(s);

    s = fu_strndup(" wibbit", 4);
    assert(s != NULL);
    assert(strlen(s) == 4);
    assert(strcmp(" wib", s) == 0);
    free(s);

    // test fu_strdup
    s = fu_strdup("blah");
    assert(s != NULL);
    assert(strlen(s) == 4);
    assert(strcmp("blah", s) == 0);
    free(s);
}

static void test_buffer_ops()
{
    struct Buffer buf;

    // test init_buffer
    buf.s = (char *)0xDEADBEEF; buf.size = 102; buf.i = 987; // bad values
    init_buffer(&buf, 1);
    assert(buf.s != NULL);
    assert(buf.size == 1);
    assert(buf.i == 0);
    buf.s[0] = 'a'; // ensure writable up to end of buffer size

    // test grow_buffer
    grow_buffer(&buf);
    assert(buf.s != NULL);
    assert(buf.size > 1);
    buf.s[buf.size - 1] =  'b'; // ensure writable up to end of new buffer size

    // test buffer_append
    buffer_append(&buf, "foo");
    assert(buf.s != NULL);
    assert(strncmp(buf.s, "foo", 3) == 0);
    assert(buf.i == 3);
    assert(buf.size >= 3);

    buffer_append(&buf, "bar");
    assert(buf.s != NULL);
    assert(strncmp(buf.s, "foobar", 6) == 0);
    assert(buf.i == 6);
    assert(buf.size >= 6);

    // test free_buffer
    free_buffer(&buf);
}

static void test_path_operations(void)
{
    // test fu_stat
    struct stat buf;
    int r = fu_stat("test_util.c", &buf);
    assert(r == 0);
    assert(buf.st_size > 0);
    assert(buf.st_mode & S_IFREG);

    r = fu_stat("thisfiledoesnotexist", &buf);
    assert(r == -1);
    assert(errno == ENOENT);

    // test fu_isdir
    r = fu_isdir("../../test");
    assert(r == TRUE);

    r = fu_isdir("thisdirdoesnotexist");
    assert(r == FALSE);
}

int main(int argc, char **argv)
{
    test_allocation();
    test_string_ops();
    test_buffer_ops();
    test_path_operations();

    printf("%s success!\n", argv[0]);
    return 0;
}
