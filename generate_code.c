/* generate_code.c - generate test code from .fun template
 */
#include "funit.h"
#include <stdio.h>

#include "funit_fortran_module.h"

void generate_code(struct Code *code)
{
    if (code->next)
        generate_code(code->next);

    // XXX expand asserts to real code
    fwrite(code->str, code->len, 1, stdout);
}

void generate_test(struct TestRoutine *test, int *test_i)
{
    if (test->next)
        generate_test(test->next, test_i);

    *test_i += 1;
    printf("  subroutine _funit_test%i(_funit_passed, _funit_message)\n", *test_i);
    fputs("    implicit none\n", stdout);
    fputs("    logical, intent(out) :: _funit_passed\n", stdout);
    fputs("    character(*), intent(out) :: _funit_message\n\n", stdout);

    if (test->code)
        generate_code(test->code);
    printf("  end subroutine _funit_test%i\n\n", *test_i);
}

void generate_support(struct Code *code, const char *type)
{
    printf("  subroutine _funit_%s\n", type);
    generate_code(code);
    printf("  end subroutine _funit_%s\n\n", type);
}

void generate_test_call(struct TestSuite *suite,
                        struct TestRoutine *test, int *test_i)
{
    if (test->next)
        generate_test_call(suite, test->next, test_i);

    *test_i += 1;

    fputs("\n", stdout);
    if (suite->setup)
        printf("  call _funit_setup\n");
    printf("  call _funit_test%i(_funit_passed, _funit_message)\n", *test_i);
    fputs("  call pass_fail(_funit_passed, _funit_message, \"", stdout);
    fwrite(test->name, test->name_len, 1, stdout);
    fputs("\")\n", stdout);
    if (suite->teardown)
        fputs("  call _funit_teardown\n\n", stdout);
}

void generate_suite(struct TestSuite *suite, int *suite_i)
{
    int test_i;

    if (suite->next)
        generate_suite(suite->next, suite_i);

    *suite_i += 1;
    printf("subroutine _funit_suite%i\n", *suite_i);
    fputs("  implicit none\n\n", stdout);
    fputs("  use funit\n\n", stdout);
    fputs("  character*512 :: _funit_message\n", stdout);
    fputs("  integer :: _funit_passed\n\n", stdout);
    if (suite->code)
        generate_code(suite->code);
    if (suite->tests) {
        test_i = 0;
        generate_test_call(suite, suite->tests, &test_i);
    }
    fputs("contains\n\n", stdout);
    if (suite->setup)
        generate_support(suite->setup, "setup");
    if (suite->teardown)
        generate_support(suite->setup, "teardown");
    if (suite->tests) {
        test_i = 0;
        generate_test(suite->tests, &test_i);
    }
    printf("end subroutine _funit_suite%i\n", *suite_i);
}

void generate_suite_call(struct TestSuite *suite, int *suite_i)
{
    if (suite->next)
        generate_suite_call(suite->next, suite_i);

    *suite_i += 1;
    fputs("\n  call start_suite(\"", stdout);
    fwrite(suite->name, suite->name_len, 1, stdout);
    fputs("\")\n", stdout);
    printf("  call _funit_suite%i\n", *suite_i);
}

void generate_main(struct TestSuite *suite, int *suite_i)
{
    fputs("\n\nprogram main\n", stdout);
    fputs("  use funit\n\n",  stdout);
    fputs("  call clear_stats\n", stdout);
    generate_suite_call(suite, suite_i);
    fputs("\n  call report_stats\n", stdout);
    printf("end program main\n");
}

int main(int argc, char **argv)
{
    struct TestSuite *suites;
    int suite_i;

    suites = parse_suite_file(argv[1]);

    fputs(module_code, stdout);

    suite_i = 0;
    generate_suite(suites, &suite_i);

    suite_i = 0;
    generate_main(suites, &suite_i);

    close_suite_and_free(suites);

    return 0;
}
