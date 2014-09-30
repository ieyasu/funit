#include "../../funit.h"
#include "../../config.c"
#include <string.h>

void test_try_open_file()
{
    char *path = "abcdef";
    struct ParseState ps;

    int ret = try_open_file(path, &ps);
    assert(ret == 0);

    assert(ps.bufsize == 6);
    assert(memcmp(ps.file_buf, "abcdef", 6) == 0);

    close_parse_file(&ps);

    path = "idontexist";
    ret = try_open_file(path, &ps);
    assert(ret == -1);
}

// only tricky code is finding the .funit in $HOME, so test that
void test_find_config_file()
{
    setenv("HOME", "test-home", 1);

    char config_path[PATH_MAX + 1];
    struct ParseState ps;

    int ret = find_config_file(config_path, &ps);
    assert(ret == 0);
    assert(memcmp(ps.file_buf, "build = 'home'", 14) == 0);

    close_parse_file(&ps);
}

void test_copy_escaped_string()
{
    char *s = copy_escaped_string("a \\\\ b \\' c \\\"", 14);
    assert(strcmp(s, "a \\\\ b ' c \"") == 0);
    free(s);

    // make sure it doesn't try to copy the close quote
    s = copy_escaped_string("foo \\' \"", 7);
    assert(strcmp(s, "foo ' ") == 0);
    free(s);
}

void test_parse_config_setting()
{
    struct ParseState ps;
    struct Config conf;

    memset(&conf, 0, sizeof(struct Config));

    int ret = try_open_file("one_setting", &ps);
    assert(ret == 0);

    assert(next_line(&ps) != NULL);
    skip_next_ws(&ps);
    ret = parse_config_setting(&ps, &conf);
    assert(ret == 0);
    assert(conf.fortran_ext_len == 4);
    assert(strncmp(conf.fortran_ext, ".F90", 4) == 0);

    close_parse_file(&ps);
}

void test_parse_config()
{
    struct ParseState ps;
    struct Config conf;

    memset(&conf, 0, sizeof(struct Config));

    int ret = try_open_file("funitrc-1", &ps);
    assert(ret == 0);

    ret = parse_config(&ps, &conf);
    assert(ret == 0);

    assert(conf.build_len == 22);
    assert(strncmp(conf.build, "make me a pretty salad", 22) == 0);
    assert(conf.fortran_ext_len == 4);
    assert(strncmp(conf.fortran_ext, ".ftn", 4) == 0);
    assert(conf.template_ext_len == 4);
    assert(strncmp(conf.template_ext, ".foo", 4) == 0);

    close_parse_file(&ps);
}

void test_parse_config_escapes()
{
    struct ParseState ps;
    struct Config conf;

    memset(&conf, 0, sizeof(struct Config));

    int ret = try_open_file("funitrc-2", &ps);
    assert(ret == 0);

    ret = parse_config(&ps, &conf);
    assert(ret == 0);

    assert(strcmp(conf.build, "escaped `woh` \" \\\\ \\{{build} \\${rule}'") == 0);
    assert(strchr(conf.build, '#') == NULL); // unquoted, comment at end
    assert(strncmp(conf.fortran_ext, ".qqq", 4) == 0); // second def overrides first

    close_parse_file(&ps);
}

/* Expect error from bad key(s) in funitrc-[3-5] -- totally wrong 'whatever',
 * 'fortran_ex' and 'builde'.
 */
void test_parse_config_bad_keys()
{
    struct ParseState ps;
    struct Config conf;

    memset(&conf, 0, sizeof(struct Config));

    int ret = try_open_file("funitrc-3", &ps);
    assert(ret == 0);

    ret = parse_config(&ps, &conf);
    assert(ret != 0);

    assert(conf.build == NULL);
    assert(conf.fortran_ext == NULL);
    assert(conf.template_ext == NULL);

    close_parse_file(&ps);

    ret = try_open_file("funitrc-4", &ps);
    assert(ret == 0);

    ret = parse_config(&ps, &conf);
    assert(ret != 0);

    assert(conf.build == NULL);
    assert(conf.fortran_ext == NULL);
    assert(conf.template_ext == NULL);

    close_parse_file(&ps);

    ret = try_open_file("funitrc-5", &ps);
    assert(ret == 0);

    ret = parse_config(&ps, &conf);
    assert(ret != 0);

    assert(conf.build == NULL);
    assert(conf.fortran_ext == NULL);
    assert(conf.template_ext == NULL);

    close_parse_file(&ps);
}

void test_set_defaults_empty(void)
{
    struct Config conf;

    memset(&conf, 0, sizeof(struct Config));

    set_defaults(&conf);

    assert(conf.build != NULL);
    assert(conf.build_len == strlen(conf.build));
    assert(conf.build_fragments != NULL);
    assert(conf.fortran_ext != NULL);
    assert(conf.fortran_ext_len == strlen(conf.fortran_ext));
    assert(conf.template_ext != NULL);
    assert(conf.template_ext_len == strlen(conf.template_ext));

    free_config(&conf);
}

void test_set_defaults_full(void)
{
    struct Config conf;
    static char *build = "gfortran -o {{EXE}} {{SRC}}";
    static char *fext = ".foobar";
    static char *text = ".baz";

    memset(&conf, 0, sizeof(struct Config));

    conf.build = build;
    conf.build_len = strlen(build);
    conf.fortran_ext = fext;
    conf.fortran_ext_len = strlen(fext);
    conf.template_ext = text;
    conf.template_ext_len = strlen(text);

    set_defaults(&conf);

    assert(conf.build != build);
    assert(strcmp(conf.build, build) == 0);
    assert(conf.build_fragments != NULL);
    assert(conf.fortran_ext != fext);
    assert(strcmp(conf.fortran_ext, fext) == 0);
    assert(conf.template_ext != text);
    assert(strcmp(conf.template_ext, text) == 0);

    free_config(&conf);
}

int main(int argc, char **argv)
{
    test_try_open_file();
    test_find_config_file();

    test_copy_escaped_string();

    test_parse_config_setting();
    test_parse_config();
    test_parse_config_escapes();
    test_parse_config_bad_keys();

    test_set_defaults_empty();
    test_set_defaults_full();

    puts("all config file tests passed!");
}
