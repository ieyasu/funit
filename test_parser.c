#include "funit.h"
#include <stdio.h>

void print_dependency(struct TestDependency *dep)
{
    if (dep->next)
        print_dependency(dep->next);

    printf("  Dep: '");
    fwrite(dep->filename, dep->len, 1, stdout);
    puts("'");
}

void print_code(char *label, struct Code *code)
{
    if (code->next)
        print_code(label, code->next);

    printf("%s: ", label);
    if (code->len < 50) {
        fwrite(code->str, code->len, 1, stdout);
    } else {
        fwrite(code->str, 25, 1, stdout);
        fputs("...", stdout);
        fwrite(code->str + code->len - 25, 25, 1, stdout);
    }
}

void print_test(struct TestRoutine *test)
{
    if (test->next)
        print_test(test->next);

    printf("  Test '");
    fwrite(test->name, test->name_len, 1, stdout);
    puts("'");

    if (test->code)
        print_code("    Code", test->code);
}

void print_suite(struct TestSuite *suite)
{
    if (suite->next)
        print_suite(suite->next);

    printf("Suite '");
    fwrite(suite->name, suite->name_len, 1, stdout);
    printf("'\n");

    if (suite->tolerance > 0.0) {
        printf("  Tolerance %f\n", suite->tolerance);
    } else {
        puts("  No tolerance given");
    }

    printf("  # deps: %d\n", suite->n_deps);
    printf("  # tests: %d\n", suite->n_tests);

    print_dependency(suite->deps);

    if (suite->code)
        print_code("  Code", suite->code);

    if (suite->setup)
        print_code("  Setup", suite->setup);

    if (suite->teardown)
        print_code("  Teardown", suite->teardown);

    print_test(suite->tests);

    puts("");
}

int main(int argc, char **argv)
{
    struct TestSuite *suite;
    int i;

    if (argc == 1) {
        printf("Usage: %s TEST_FILE...\n", argv[0]);
        exit(1);
    }

    for (i = 1; i < argc; i++) {
        printf("Parsing %s:\n\n", argv[i]);
        suite = parse_suite_file(argv[i]);
        if (suite) {
            print_suite(suite);
            close_suite_and_free(suite);
        } else {
            puts("!!! Parse file returned NULL");
        }
    }
    return 0;
}
