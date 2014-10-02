#include "../funit.h"
#include "../build_rule.c"

void test_len_without_ext(void)
{
    assert(len_without_ext("foo.eep", ".eep", 4, "what", "for") == 3);

    assert(len_without_ext("foo", ".eep", 4, "metavar", "testing") == 3);

    assert(len_without_ext("foo.wop", ".eep", 4, "meta-var", "testing") == 7);
}

void test_expand_deps(void)
{
    struct StringBuffer sb;
    sb_init(&sb, 16);

    struct TestDependency dep1a = {.filename = "a", .len = 1, .next = NULL};
    struct TestDependency dep1b = {.filename = "b", .len = 1, .next = &dep1a};
    struct TestDependency dep2a = {.filename = "c", .len = 1, .next = NULL};
    struct TestDependency dep2b = {.filename = "b", .len = 1, .next = &dep2a};
    struct TestDependency dep3  = {.filename = "d", .len = 1, .next = NULL};

    struct TestSet set1 = {.deps = &dep1b, .n_deps = 2, .next = NULL};
    struct TestSet set2 = {.deps = &dep2b, .n_deps = 2, .next = &set1};
    struct TestSet set3 = {.deps = &dep3,  .n_deps = 1, .next = &set2};

    struct TestFile tf = {.sets = &set3};

    expand_deps(&sb, &tf, NULL);

    assert(sb.len == 7);
    assert(strncmp("d b c a", sb.s, 7) == 0);

    sb_free(&sb);
}

void test_expand_mods_with(void)
{
    struct StringBuffer sb;
    sb_init(&sb, 16);

    struct TestModule mod1a = {.name = "a", .len = 1, .next = NULL};
    struct TestModule mod1b = {.name = "b", .len = 1, .next = &mod1a};
    struct TestModule mod1c = {.name = "b", .len = 1, .next = &mod1b};
    struct TestModule mod2a = {.name = "c", .len = 1, .next = NULL};
    struct TestModule mod2b = {.name = "d", .len = 1, .next = &mod2a};
    struct TestModule mod3a = {.name = "e", .len = 1, .next = NULL};
    struct TestModule mod3b = {.name = "f", .len = 1, .next = &mod3a};
    struct TestModule mod3c = {.name = "c", .len = 1, .next = &mod3b};

    struct TestSet set1 = {.mods = &mod1c, .n_mods = 2, .next = NULL};
    struct TestSet set2 = {.mods = &mod2b, .n_mods = 2, .next = &set1};
    struct TestSet set3 = {.mods = &mod3c, .n_mods = 3, .next = &set2};

    struct TestFile tf = {.sets = &set3};

    // with extension
    expand_mods_with(&sb, &tf, ".ext");

    assert(sb.len == 35);
    assert(strncmp("c.ext f.ext e.ext d.ext b.ext a.ext", sb.s, 35) == 0);

    sb.len = 0;

    // without extension
    expand_mods_with(&sb, &tf, NULL);

    assert(sb.len == 11);
    assert(strncmp("c f e d b a", sb.s, 11) == 0);

    sb_free(&sb);
}

void test_parse_internal_var(void)
{
    struct StringBuffer sb;
    sb_init(&sb, 4);

    struct BRFragments f;
    memset(&f, 0, sizeof(struct BRFragments));

    // basic var reference
    char *build = fu_strdup("foo {{EXE}} bar");
    size_t i = 4;

    i = parse_internal_var(&f, &sb, build, i);
    assert(i == 11);
    assert(f.n == 2);
    assert(f.frags[0].type == BR_STRING);
    assert(strcmp(f.frags[0].frag.s, "foo "));
    assert(f.frags[1].type == BR_IVAR);
    assert(f.frags[1].frag.expandcb == expand_exe);

    // setup for reuse?
    assert(sb.len == 0);
    assert(sb.s != NULL);

    free(build);

    // unknown name
    build = fu_strdup("foo {{FOOM}} bar");
    i = 4;

    i = parse_internal_var(&f, &sb, build, i);
    assert(i == 4 + 2);
    assert(sb.len == 2);
    assert(strncmp(sb.s, "{{", 2) == 0);
    assert(f.n == 2); // unchanged

    free(build);
    sb.len = 0;

    // unterminated
    build = fu_strdup("foo {{EXE bar");
    i = 4;

    i = parse_internal_var(&f, &sb, build, i);
    assert(i == 4 + 2);
    assert(sb.len == 2);
    assert(strncmp(sb.s, "{{", 2) == 0);
    assert(f.n == 2); // unchanged

    free(build);
    sb.len = 0;

    // empty name
    build = fu_strdup("foo {{}} bar");
    i = 4;

    i = parse_internal_var(&f, &sb, build, i);
    assert(i == 4 + 2);
    assert(sb.len == 2);
    assert(strncmp(sb.s, "{{", 2) == 0);
    assert(f.n == 2); // unchanged

    free(build);
    sb.len = 0;

    sb_free(&sb);
}

void test_parse_env_var(void)
{
    struct StringBuffer sb;
    sb_init(&sb, 4);

    // normal expansion
    char *build = fu_strdup("foo ${CC} bar");
    size_t i = 4;
    assert(build[i] == '$' && build[i + 1] == '{');

    setenv("CC", "wibbit", 1);

    i = parse_env_var(&sb, build, i);
    assert(i == 9);
    assert(strncmp(sb.s, "wibbit", 6) == 0);

    free(build);
    sb.len = 0;

    // expansion of an unset env var
    build = fu_strdup("foo ${empty} bar");
    i = 4;

    unsetenv("empty");

    i = parse_env_var(&sb, build, i);
    assert(i == 12);
    assert(sb.len == 0);

    free(build);

    // unclosed env var -> no expansion
    build = fu_strdup("foo ${unclosed bar");
    i = 4;

    i = parse_env_var(&sb, build, i);
    assert(i == 4 + 1);
    assert(sb.len == 1);
    assert(sb.s[0] == '$');

    free(build);
    sb.len = 0;

    // empty var name -> warning
    build = fu_strdup("foo ${} bar");
    i = 4;

    i = parse_env_var(&sb, build, i);
    assert(i == 4 + 1);
    assert(sb.len == 1);
    assert(sb.s[0] == '$');

    free(build);

    sb_free(&sb);
}

struct BRFragments *test_parse_build_rule(void)
{
    setenv("FC", "f95", 1);

    char *build = fu_strdup("${FC} blah {{EXE}} $woop ${tea \\{\\} \\\\");
    struct BRFragments *f = parse_build_rule(build);

    assert(f->n == 3);
    assert(f->cap >= 3);

    assert(f->frags[0].type == BR_STRING);
    assert(strcmp(f->frags[0].frag.s, "f95 blah ") == 0);

    static const int EXE_I = 1;
    assert(f->frags[1].type == BR_IVAR);
    assert(strcmp(ivars[EXE_I].name, "EXE") == 0); // make sure index hasn't changed
    assert(f->frags[1].frag.expandcb == ivars[EXE_I].expandcb);

    assert(f->frags[2].type == BR_STRING);
    assert(strcmp(f->frags[2].frag.s, " $woop ${tea {} \\") == 0);

    return f;
}

void test_make_build_command(struct BRFragments *f)
{
    struct StringBuffer sb;
    sb_init(&sb, 4);

    struct TestFile tf = {.path = "test_path", .exe = "test.exe"};

    struct Config conf = {.build_fragments = f};

    make_build_command(&sb, &tf, &conf);
    assert(sb.len == 35);
    assert(strcmp(sb.s, "f95 blah test.exe $woop ${tea {} \\") == 0);
}

int main(int argc, char **argv)
{
    puts("There should be several Warning messages below.\n");

    test_len_without_ext();
    test_expand_deps();
    test_expand_mods_with();

    test_parse_internal_var();
    test_parse_env_var();

    struct BRFragments *f = test_parse_build_rule();
    test_make_build_command(f);
    free_build_fragments(f);

    puts("\nall build rule parsing tests passed!");
}
