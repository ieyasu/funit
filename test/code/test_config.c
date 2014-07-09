#include "../../funit.h"
#include "../../util.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void write_funit(const char *content)
{
    FILE *fout = fopen(".funit", "w");
    fputs(content, fout);
    fclose(fout);
}

static void test_no_funit(void)
{
    struct Config conf;
    int r = read_config(&conf);
    assert(r == -1);
}

static const char full_config[] =
    "# test config file\n"
    "build = 'a build rule' # comment after value\n"
    "tempdir = /my/temp#yo\n"
    "\n"
    "fortran_ext=.yummy\n"
    "template_ext = \".nice\"#peter pan\n"
    "\n"
    "# more commenting!\n"
    ;

static void test_dot_funit(void)
{
    write_funit(full_config);

    struct Config conf;
    int r = read_config(&conf);
    assert(r == 0);

    assert(strcmp(conf.build, "a build rule") == 0);
    assert(strcmp(conf.tempdir, "/my/temp") == 0);
    assert(strcmp(conf.fortran_ext, ".yummy") == 0);
    assert(strcmp(conf.template_ext, ".nice") == 0);
}

static const char empty_config[] = " # just a comment";

static void test_defaults(void)
{
    write_funit(empty_config);

    struct Config conf;
    int r = read_config(&conf);
    assert(r == 0);

    assert(strcmp(conf.build, "make {{EXE}}") == 0);
    assert(conf.tempdir != NULL);
    assert(strlen(conf.tempdir) > 0);
    assert(fu_isdir(conf.tempdir));
    assert(strcmp(conf.fortran_ext, ".F90") == 0);
    assert(strcmp(conf.template_ext, ".fun") == 0);
}

static const char bad_config[] =
    "# blech\n"
    "\n"
    "key = # comment\n"
    "\n"
    " # wut"
;

static void test_bad_funit(void)
{
    write_funit(bad_config);

    struct Config conf;
    int r = read_config(&conf);
    assert(r == -1);
    assert(conf.build == NULL);
    assert(conf.tempdir == NULL);
    assert(conf.fortran_ext == NULL);
    assert(conf.template_ext == NULL);
}

int main(int argc, char **argv)
{
    unlink(".funit");

    test_no_funit();
    test_dot_funit();
    test_defaults();
    test_bad_funit();

    printf("%s success!\n", argv[0]);
    return 0;
}
