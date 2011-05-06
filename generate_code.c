/* generate_code.c - generate test code from .fun template
 */
#include "funit.h"
#include <stdio.h>

#include "funit_fortran_module.h"

static double tolerance = 0.0;

static int check_assert_args(const char *macro_name,
                             struct Code *args, int num_expected)
{
    int n;

    assert(num_expected > 0);

    if (!args) {
        // XXX no argument to -macro_name
        return -1;
    }
    n = 0;
    while (args) {
        n++;
        if (n > num_expected)
            goto badargs;
        args = args->next;
    }
    if (n < num_expected)
        goto badargs;
    return 0;
 badargs:
    // XXX expected -num_expected- argument(s) to -macro_name-
    return -1;
}

// plain printing
#define PRINT_ARG(arg) fwrite((arg)->u.c.str, (arg)->u.c.len, 1, stdout)

/* Prints the macro argument between s and end to the fout stream, handling
 * newlines by inserting a leading '&' if one is not already present.
 */
static void print_macro_arg(struct Code *arg)
{
    assert(arg && arg->type == ARG_CODE);

    // XXX print it out
}

/* assert_true(expr) becomes:
 *
 *     if (.not. (-expr-)) then
 *       write(funit_message_,*) "-expr-", "is false"
 *       funit_passed_ = .false.
 *       return
 *     end if
 */
static int generate_assert_true(struct Code *macro)
{
    struct Code *arg = macro->u.m.args;

    if (check_assert_args("assert_true", arg, 1))
        return -1;

    fputs("if (.not. (", stdout);
    PRINT_ARG(arg);
    fputs(")) then\n", stdout);
    fputs("      write(funit_message_,*) \"'", stdout);
    print_macro_arg(arg);
    fputs("' is false\"\n", stdout);
    fputs("      funit_passed_ = .false.\n", stdout);
    fputs("      return\n", stdout);
    fputs("    end if", stdout);

    return 0;
}

/* assert_false(expr) becomes:
 *
 *     if (-expr-) then
 *       write(_message,*) "-expr-", "is true"
 *       funit_passed_ = .false.
 *       return
 *     end if
 */
static int generate_assert_false(struct Code *macro)
{
    struct Code *arg = macro->u.m.args;

    if (check_assert_args("assert_false", arg, 1))
        return -1;

    fputs("if (", stdout);
    PRINT_ARG(arg);
    fputs(") then\n", stdout);
    fputs("      write(funit_message_,*) '", stdout);
    print_macro_arg(arg);
    fputs("' is true\"\n", stdout);
    fputs("      funit_passed_ = .false.\n", stdout);
    fputs("      return\n", stdout);
    fputs("    end if", stdout);

    return 0;
}

/* assert_equal(a,b) becomes:
 *
 *     if ((a) /= (b)) then
 *       write(_message,*) "-a- is not equal to -b-"
 *       funit_passed_ = .false.
 *       return
 *     end if
 */
static int generate_assert_equal(struct Code *macro)
{
    struct Code *a = macro->u.m.args, *b;

    if (check_assert_args("assert_equal", a, 2))
        return -1;
    b = a->next;

    fputs("if ((", stdout);
    PRINT_ARG(a);
    fputs(") /= (", stdout);
    PRINT_ARG(b);
    fputs(")) then\n", stdout);
    fputs("      write(funit_message_,*) \"'",stdout);
    print_macro_arg(a);
    fputs("' is not equal to '", stdout);
    print_macro_arg(b);
    fputs("'\"\n", stdout);
    fputs("      funit_passed_ = .false.\n", stdout);
    fputs("      return\n", stdout);
    fputs("    end if", stdout);

    return 0;
}

/* assert_not_equal(a,b) becomes:
 *
 *     if ((a) == (b)) then
 *       write(_message,*) "-a- is equal to -b-"
 *       funit_passed_ = .false.
 *       return
 *     end if
 */
static int generate_assert_not_equal(struct Code *macro)
{
    struct Code *a = macro->u.m.args, *b;

    if (check_assert_args("assert_not_equal", a, 2))
        return -1;
    b = a->next;

    fputs("if ((", stdout);
    PRINT_ARG(a);
    fputs(") == (", stdout);
    PRINT_ARG(b);
    fputs(")) then\n", stdout);
    fputs("      write(funit_message_,*) \"'",stdout);
    print_macro_arg(a);
    fputs("' is equal to '", stdout);
    print_macro_arg(b);
    fputs("'\"\n", stdout);
    fputs("      funit_passed_ = .false.\n", stdout);
    fputs("      return\n", stdout);
    fputs("    end if", stdout);

    return 0;
}

/* assert_equal_with(a,b) becomes:
 *
 *     if (abs((a) - (b)) > TOLERANCE) then
 *       write(_message,*) "-a- is not within", TOLERANCE, "of -b-"
 *       funit_passed_ = .false.
 *       return
 *     end if
 */
static int generate_assert_equal_with(struct Code *macro)
{
    struct Code *a = macro->u.m.args, *b;

    if (check_assert_args("assert_equal_with", a, 2))
        return -1;
    b = a->next;

    fputs("if (abs((", stdout);
    PRINT_ARG(a);
    fputs(") - (", stdout);
    PRINT_ARG(b);
    fprintf(stdout, ")) > %f) then\n", tolerance);
    fputs("      write(funit_message_,*) \"'",stdout);
    print_macro_arg(a);
    fprintf(stdout, "' is not within %f of '", tolerance);
    print_macro_arg(b);
    fputs("'\"\n", stdout);
    fputs("      funit_passed_ = .false.\n", stdout);
    fputs("      return\n", stdout);
    fputs("    end if", stdout);

    return 0;
}

static void print_array_size_check(struct Code *a, struct Code *b)
{
    fputs("if (size(", stdout);
    PRINT_ARG(a);
    fputs(") /= size(", stdout);
    PRINT_ARG(b);
    fputs(")) then\n", stdout);
    fputs("      write(funit_message_,*) \"'",stdout);
    print_macro_arg(a);
    fputs("' is length, size(", stdout);
    PRINT_ARG(a);
    fputs("), &\n", stdout);
    fputs("        which is not the same as '", stdout);
    print_macro_arg(b);
    fputs("' which is length, size(", stdout);
    PRINT_ARG(b);
    fputs(")\n", stdout);
    fputs("      funit_passed_ = .false.\n", stdout);
    fputs("      return\n", stdout);
    fputs("    end if", stdout);
}

/* assert_array_equal(a,b) becomes:
 *
 *     if (size(a) /= size(b)) then
 *       write(funit_message_,*) "-a- is length", size(a), &
 *         "which is not the same as -b- which is length", size(b)
 *       funit_passed_ = .false.
 *       return
 *     end if
 *     do funit_i_ = 1,size(a)
 *       if (a(funit_i_) /= b(funit_i_)) then
 *         write(funit_message_,*) "-a-(", funit_i_, ") is not equal to -b-(", &
 *           funit_i_, "): ", a(funit_i_), "vs", b(funit_i_)
 *         funit_passed_ = .false.
 *         return
 *       end if
 *     end do
 */
static int generate_assert_array_equal(struct Code *macro)
{
    struct Code *a = macro->u.m.args, *b;

    if (check_assert_args("assert_array_equal", a, 2))
        return -1;
    b = a->next;

    print_array_size_check(a, b);

    // do loop
    fputs("    do funit_i = 1,size(", stdout);
    PRINT_ARG(a);
    fputs(")\n", stdout);
    fputs("      if (", stdout);
    PRINT_ARG(a);
    fputs("(funit_i_) /= ", stdout);
    PRINT_ARG(b);
    fputs("(funit_i_)) then\n", stdout);
    fputs("        write(funit_message_,*) \"", stdout);
    print_macro_arg(a);
    fputs("(\", funit_i_, \") is not equal to ", stdout);
    print_macro_arg(b);
    fputs("(\", funit_i_, \"): \", ", stdout);
    PRINT_ARG(a);
    fputs("(funit_i_), \"vs\", ", stdout);
    PRINT_ARG(b);
    fputs("(funit_i_)\n", stdout);
    fputs("        funit_passed_ = .false.\n", stdout);
    fputs("        return\n", stdout);
    fputs("      end if", stdout);
    fputs("    end do", stdout);

    return 0;
}

/* assert_array_equal(a,b) becomes:
 *
 *     if (size(a) /= size(b)) then
 *       write(_message,*) "-a- is length", size(a), &
 *         "which is not the same as -b- which is length", size(b)
 *       funit_passed_ = .false.
 *       return
 *     end if
 *     do funit_i_ = 1,size(a)
 *       if (abs(a(funit_i_) - b(funit_i_)) > TOLERANCE) then
 *         write(_message,*) "-a-(", funit_i_, ") is not within", TOLERANCE, &
 *           "of -b-(", funit_i_, "): ", a(funit_i_), "vs", b(funit_i_)
 *         funit_passed_ = .false.
 *         return
 *       end if
 *     end do
 */
static int generate_assert_array_equal_with(struct Code *macro)
{
    struct Code *a = macro->u.m.args, *b;

    if (check_assert_args("assert_array_equal", a, 2))
        return -1;
    b = a->next;

    // length check
    print_array_size_check(a, b);

    // do loop
    fputs("    do funit_i = 1,size(", stdout);
    fwrite(a->u.c.str, a->u.c.len, 1, stdout);
    fputs(")\n", stdout);
    fputs("      if (abs(", stdout);
    PRINT_ARG(a);
    fputs("(funit_i_) - ", stdout);
    PRINT_ARG(b);
    fprintf(stdout, "(funit_i_)) > %f) then\n", tolerance);
    fputs("        write(funit_message_,*) \"", stdout);
    print_macro_arg(a);
    fprintf(stdout, "(\", funit_i_, \") is not within %f of \"", tolerance);
    print_macro_arg(b);
    fputs("(\", funit_i_, \"): \", ", stdout);
    PRINT_ARG(a);
    fputs("(funit_i_), \"vs\", ", stdout);
    PRINT_ARG(b);
    fputs("(funit_i_)\n", stdout);
    fputs("        funit_passed_ = .false.\n", stdout);
    fputs("        return\n", stdout);
    fputs("      end if", stdout);
    fputs("    end do", stdout);

    return 0;
}

/* flunk(msg) becomes:
 *
 *     write(funit_message_,*) -msg-
 *     funit_passed_ = .false.
 *     return
 */
static int generate_flunk(struct Code *macro)
{
    struct Code *arg = macro->u.m.args;

    if (check_assert_args("flunk", arg, 1))
        return -1;

    fputs("write(funit_message_,*) ", stdout);
    PRINT_ARG(arg);
    fputs("\n", stdout);
    fputs("    funit_message_ = .false.\n", stdout);
    fputs("    return\n", stdout);

    return 0;
}

static int generate_assert(struct Code *macro)
{
    assert(macro->type == MACRO_CODE);

    switch (macro->u.m.type) {
    case ASSERT_TRUE:
        return generate_assert_true(macro);
    case ASSERT_FALSE:
        return generate_assert_false(macro);
    case ASSERT_EQUAL:
        return generate_assert_equal(macro);
    case ASSERT_NOT_EQUAL:
        return generate_assert_not_equal(macro);
    case ASSERT_EQUAL_WITH:
        return generate_assert_equal_with(macro);
    case ASSERT_ARRAY_EQUAL:
        return generate_assert_array_equal(macro);
    case ASSERT_ARRAY_EQUAL_WITH:
        return generate_assert_array_equal_with(macro);
    case FLUNK:
        return generate_flunk(macro);
    }
    abort();
    return -1;
}

static void generate_code(struct Code *code)
{
    switch (code->type) {
    case FORTRAN_CODE:
        fwrite(code->u.c.str, code->u.c.len, 1, stdout);
        break;
    case MACRO_CODE:
        if (!generate_assert(code)) {
            // XXX report error
            abort();
        }
        break;
    default: // arg code
        // XXX should not have been reached!
        abort();
        break;
    }

    if (code->next)
        generate_code(code->next);
}

static void generate_test(struct TestRoutine *test, int *test_i)
{
    if (test->next)
        generate_test(test->next, test_i);

    *test_i += 1;
    printf("  subroutine funit_test%i(funit_passed_, funit_message_)\n", *test_i);
    fputs("    implicit none\n", stdout);
    fputs("    logical, intent(out) :: funit_passed_\n", stdout);
    fputs("    character(*), intent(out) :: funit_message_\n", stdout);
    fputs("    integer :: funit_i_\n\n", stdout);

    if (test->code)
        generate_code(test->code);
    printf("  end subroutine funit_test%i\n\n", *test_i);
}

static void generate_support(struct Code *code, const char *type)
{
    printf("  subroutine funit_%s\n", type);
    generate_code(code);
    printf("  end subroutine funit_%s\n\n", type);
}

static void generate_test_call(struct TestSuite *suite,
                        struct TestRoutine *test, int *test_i)
{
    if (test->next)
        generate_test_call(suite, test->next, test_i);

    *test_i += 1;

    fputs("\n", stdout);
    if (suite->setup)
        printf("  call funit_setup\n");
    printf("  call funit_test%i(funit_passed_, funit_message_)\n", *test_i);
    fputs("  call pass_fail(funit_passed_, funit_message_, \"", stdout);
    fwrite(test->name, test->name_len, 1, stdout);
    fputs("\")\n", stdout);
    if (suite->teardown)
        fputs("  call funit_teardown\n\n", stdout);
}

static void generate_suite(struct TestSuite *suite, int *suite_i)
{
    int test_i;

    if (suite->next)
        generate_suite(suite->next, suite_i);

    tolerance = (suite->tolerance > 0.0) ? suite->tolerance : DEFAULT_TOLERANCE;

    *suite_i += 1;
    printf("subroutine funit_suite%i\n", *suite_i);
    fputs("  implicit none\n\n", stdout);
    fputs("  use funit\n\n", stdout);
    fputs("  character*512 :: funit_message_\n", stdout);
    fputs("  integer :: funit_passed_\n\n", stdout);
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
    printf("end subroutine funit_suite%i\n", *suite_i);
}

static void generate_suite_call(struct TestSuite *suite, int *suite_i)
{
    if (suite->next)
        generate_suite_call(suite->next, suite_i);

    *suite_i += 1;
    fputs("\n  call start_suite(\"", stdout);
    fwrite(suite->name, suite->name_len, 1, stdout);
    fputs("\")\n", stdout);
    printf("  call funit_suite%i\n", *suite_i);
}

static void generate_main(struct TestSuite *suite, int *suite_i)
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
