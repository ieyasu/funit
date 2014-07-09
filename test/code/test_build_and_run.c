#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define fu_system(path) dummy_system(path)

static char *expected_command = NULL;
static int system_ret = 0;

static int dummy_system(const char *command)
{
    if (strcmp(command, expected_command)) {
        fprintf(stderr, "expected command\n    `%s`\n but got\n    `%s` instead\n", expected_command, command);
    }
    return system_ret;
}

#include "../../build_and_run.c"

static void make_some_deps(struct TestFile *tf)
{
    static struct TestDependency
        dep1 = {.next = NULL, .filename = "dep1.F90", .len = 8},
        dep2 = {.next = &dep1,.filename = "dep2.f90", .len = 8},
        dep3 = {.next = NULL, .filename = "dep3.F",   .len = 6};
    static struct TestSet
        set1 = {.next = NULL, .deps = &dep2},
        set2 = {.next = &set1,.deps = &dep3};

    tf->sets = &set2;
}

static void test_expand_env_var(void) {
    struct Buffer buf;
    init_buffer(&buf, 64);

    // test existing environment variable
    setenv("foo", "bar", TRUE);
    char *build = "weeble ${foo} wobble";
    size_t bri = 7;
    int r = expand_env_var(&buf, build, &bri);
    assert(r == 0);
    assert(bri == 13);
    assert(buf.i == 3);
    assert(!strcmp(buf.s, "bar"));

    // test unset env var
    unsetenv("foo");
    buf.i = 0;
    buf.s[0] = 'z';
    buf.s[1] = '\0';
    bri = 7;
    r = expand_env_var(&buf, build, &bri);
    assert(r == 0);
    assert(bri == 13);
    assert(buf.i == 0);
    assert(!strcmp(buf.s, "z"));

    // test unclosed env var reference
    build = "baz ${foo wobble";
    bri = 4;
    buf.i = 0;
    r = expand_env_var(&buf, build, &bri);
    assert(r == -1);
    assert(bri == 4);
    assert(buf.i == 0);

    // test missing env var name
    build = "baz ${} wobble";
    bri = 4;
    buf.i = 0;
    r = expand_env_var(&buf, build, &bri);
    assert(r == -1);
    assert(bri == 4);
    assert(buf.i == 0);

    free_buffer(&buf);
}

static void test_expand_internal_var(void)
{
    struct Buffer buf;
    const char *outpath = "/tmp/test.F90";
    const char *exepath = "/tmp/test";
    struct TestFile tf;

    make_some_deps(&tf);

    init_buffer(&buf, 32);

    char *build = "{{EXE}} ";
    size_t bri = 0;
    int r = expand_internal_var(&buf, build, &bri, outpath, exepath, &tf);
    assert(r == 0);
    assert(bri == 7);
    assert(buf.i == strlen(exepath));
    assert(!strcmp(buf.s, exepath));

    build = "{{TEST}} ";
    buf.i = bri = 0;
    r = expand_internal_var(&buf, build, &bri, outpath, exepath, &tf);
    assert(r == 0);
    assert(bri == 8);
    assert(buf.i == strlen(outpath));
    assert(!strcmp(buf.s, outpath));

    build = "{{DEPS}} ";
    buf.i = bri = 0;
    r = expand_internal_var(&buf, build, &bri, outpath, exepath, &tf);
    assert(r == 0);
    assert(bri == 8);
    assert(buf.i == 24);
    assert(!strcmp(buf.s, "dep3.F dep2.f90 dep1.F90"));

    build = "{{}} ";
    buf.i = bri = 0;
    r = expand_internal_var(&buf, build, &bri, outpath, exepath, &tf);
    assert(r == -1);
    assert(bri == 0);
    assert(buf.i == 0);

    build = "{{TEST} ";
    buf.i = bri = 0;
    r = expand_internal_var(&buf, build, &bri, outpath, exepath, &tf);
    assert(r == -1);
    assert(bri == 0);
    assert(buf.i == 0);

    build = "{{TEST ";
    buf.i = bri = 0;
    r = expand_internal_var(&buf, build, &bri, outpath, exepath, &tf);
    assert(r == -1);
    assert(bri == 0);
    assert(buf.i == 0);
}

static void test_expand_build_vars(void)
{
    const char *outpath = "/tmp/test.F90";
    const char *exepath = "/tmp/test";
    struct TestFile tf;

    make_some_deps(&tf);

    setenv("env_var", "foo", -1);

    char *build = "gfortran -o {{EXE}} {{TEST}} {{DEPS}} ${env_var} {{blah}";
    char *s = expand_build_vars(build, outpath, exepath, &tf);
    assert(s != NULL);
    assert(!strcmp(s, "gfortran -o /tmp/test /tmp/test.F90 dep3.F dep2.f90 dep1.F90 foo {{blah}"));
}

int main(int argc, char **argv) {
    test_expand_env_var();
    test_expand_internal_var();
    test_expand_build_vars();

    printf("%s success!\n", argv[0]);
    return 0;
}
