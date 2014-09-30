#include "../../funit.h"
#include <stdio.h>

void print_dependency(struct TestDependency *dep)
{
    if (dep->next)
        print_dependency(dep->next);

    printf("  Dep: '");
    fwrite(dep->filename, dep->len, 1, stdout);
    puts("'");
}

void print_module(struct TestModule *mod)
{
    if (mod->next)
        print_module(mod->next);

    printf("  Mod: '");
    fwrite(mod->name, mod->len, 1, stdout);
    puts("'");
}

void print_code(char *label, struct Code *code)
{
    if (!label || code->type != FORTRAN_CODE) {
        switch (code->type) {
        case FORTRAN_CODE:
            label = "  Code";
            break;
        case MACRO_CODE:
            label = "  Macro";
            break;
        case ARG_CODE:
            label = "    Arg";
            break;
        }
    }
    printf("  %s: ", label);

    if (code->type == MACRO_CODE) {
        switch (code->u.m.type) {
        case ASSERT_TRUE:
            puts("assert_true");
            break;
        case ASSERT_FALSE:
            puts("assert_false");
            break;
        case ASSERT_EQUAL:
            puts("assert_equal");
            break;
        case ASSERT_NOT_EQUAL:
            puts("assert_not_equal");
            break;
        case ASSERT_EQUAL_WITH:
            puts("assert_equal_with");
            break;
        case ASSERT_ARRAY_EQUAL:
            puts("assert_array_equal");
            break;
        case ASSERT_ARRAY_EQUAL_WITH:
            puts("assert_array_equal_with");
            break;
        case FLUNK:
            puts("flunk");
            break;
        default:
            printf("macro type %i unknown!", code->u.m.type);
            abort();
        }
    } else {
        printf("'");
        fwrite(code->u.c.str, code->u.c.len, 1, stdout);
        printf("'\n");
    }

    if (code->type == MACRO_CODE && code->u.m.args)
        print_code(NULL, code->u.m.args);

    if (code->next) {
        if (code->type != FORTRAN_CODE)
            label = NULL;
        print_code(label, code->next);
    }
}

void print_test(struct TestCase *test)
{
    if (test->next)
        print_test(test->next);

    printf("  Test '");
    fwrite(test->name, test->namelen, 1, stdout);
    puts("'");

    if (test->code)
        print_code("    Code", test->code);
}

void print_sets(struct TestSet *set)
{
    if (set->next)
        print_sets(set->next);

    printf("Set '");
    fwrite(set->name, set->namelen, 1, stdout);
    printf("'\n");

    if (set->tolerance > 0.0) {
        printf("  Tolerance %f\n", set->tolerance);
    } else {
        puts("  No tolerance given");
    }

    printf("  # deps: %zu\n", set->n_deps);
    printf("  # mods: %zu\n", set->n_mods);
    printf("  # test cases: %zu\n", set->n_tests);

    if (set->deps)
        print_dependency(set->deps);

    if (set->mods)
        print_module(set->mods);

    if (set->code)
        print_code("  Code", set->code);

    if (set->setup)
        print_code("  Setup", set->setup);

    if (set->teardown)
        print_code("  Teardown", set->teardown);

    if (set->tests)
        print_test(set->tests);

    puts("");
}

int main(int argc, char **argv)
{
    if (argc == 1) {
        printf("Usage: %s TEST_FILE...\n", argv[0]);
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        printf("Parsing %s:\n\n", argv[i]);
        struct TestFile *tf = parse_test_file(argv[i]);
        if (tf && tf->sets) {
            print_sets(tf->sets);
            close_testfile(tf);
        } else {
            puts("!!! Parse file returned NULL");
        }
    }
    return 0;
}
