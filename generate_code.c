/* generate_code.c - generate test code from .fun template
 */
#include "funit.h"
#include <stdio.h>

// for module_code string variable
#include "funit_fortran_module.h"

static double tolerance = 0.0;

static int check_assert_args(const char *macro_name, struct Code *macro,
                             struct Code *args, int num_expected)
{
    long lineno;
    int n;

    assert(num_expected > 0);

    if (!args) {
        fprintf(stderr, "near %s:%li: no arguments to %s()\n",
                test_suite_file_name, macro->lineno, macro_name);
        return -1;
    }
    n = 0;
    while (args) {
        lineno = args->lineno;
        n++;
        if (n > num_expected)
            goto badargs;
        args = args->next;
    }
    if (n < num_expected)
        goto badargs;
    return 0;
 badargs:
    fprintf(stderr, "near %s:%li: expected %i argument%s to %s()\n",
            test_suite_file_name, lineno, num_expected,
            (num_expected > 1) ? "s" : "", macro_name);
    return -1;
}

// plain printing
#define PRINT_CODE(arg) fwrite((arg)->u.c.str, (arg)->u.c.len, 1, stdout)

static char *find_line_continuation(char *s, char *end, int in_string)
{
    char *line_start;

    assert(*s == '\n' || *s == '\r');

 skip_line:
    while (s < end) {
        switch (*s) {
        case '\r':
            if (*(s + 1) == '\n')
                s++;
            // fallthrough
        case '\n':
            s++;
            goto eol;
        default:
            break;
        }
        s++;
    }
 eol:
    line_start = s;

    // find first non-ws char
    while (s < end && (*s == ' ' || *s == '\t'))
        s++;
    switch (*s) {
    case '!':
        goto skip_line;
    case '&':
        if (in_string)
            return s + 1; // skip past whitespace eater
        // fallthrough
    default:
        return line_start;
    }
}

/* Prints the macro argument between s and end to the fout stream, handling
 * newlines by inserting a leading '&' if one is not already present.
 */
static void print_macro_arg(struct Code *arg)
{
    char *s, *start, *end, *amp, string_delim;
    int in_string = 0;

    assert(arg && arg->type == ARG_CODE);

    s = start = arg->u.c.str;
    end = s + arg->u.c.len;
    amp = NULL;
    while (s < end) {
        switch (*s) {
        case '\'':
        case '"':
            if (in_string) {
                if (*s == string_delim) {
                    if (*(s + 1) == string_delim)
                        s++; // doubled to escape
                    else
                        in_string = 0; // close quote
                }
            } else {
                in_string = 1;
                string_delim = *s;
            }
            amp = NULL;
            break;
        case '&':
            amp = s;
            break;
        case ' ':
        case '\t':
            // leave amp alone
            break;
        case '\n':
        case '\r':
            // line continuation
            assert(amp != NULL);
            // print prior arg-part
            fwrite(start, amp - start, 1, stdout);
            // scan for next arg-part
            start = find_line_continuation(s, end, in_string);
            s = start - 1;
            break;
        default:
            amp = NULL;
            break;
        }
        s++;
    }
    // print remaining
    fwrite(start, end - start, 1, stdout);
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

    if (check_assert_args("assert_true", macro, arg, 1))
        return -1;

    fputs("if (.not. (", stdout);
    PRINT_CODE(arg);
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

    if (check_assert_args("assert_false", macro, arg, 1))
        return -1;

    fputs("if (", stdout);
    PRINT_CODE(arg);
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

    if (check_assert_args("assert_equal", macro, a, 2))
        return -1;
    b = a->next;

    fputs("if ((", stdout);
    PRINT_CODE(a);
    fputs(") /= (", stdout);
    PRINT_CODE(b);
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

    if (check_assert_args("assert_not_equal", macro, a, 2))
        return -1;
    b = a->next;

    fputs("if ((", stdout);
    PRINT_CODE(a);
    fputs(") == (", stdout);
    PRINT_CODE(b);
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

    if (check_assert_args("assert_equal_with", macro, a, 2))
        return -1;
    b = a->next;

    fputs("if (abs((", stdout);
    PRINT_CODE(a);
    fputs(") - (", stdout);
    PRINT_CODE(b);
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
    PRINT_CODE(a);
    fputs(") /= size(", stdout);
    PRINT_CODE(b);
    fputs(")) then\n", stdout);
    fputs("      write(funit_message_,*) \"'",stdout);
    print_macro_arg(a);
    fputs("' is length, size(", stdout);
    PRINT_CODE(a);
    fputs("), &\n", stdout);
    fputs("        which is not the same as '", stdout);
    print_macro_arg(b);
    fputs("' which is length, size(", stdout);
    PRINT_CODE(b);
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

    if (check_assert_args("assert_array_equal", macro, a, 2))
        return -1;
    b = a->next;

    print_array_size_check(a, b);

    // do loop
    fputs("    do funit_i = 1,size(", stdout);
    PRINT_CODE(a);
    fputs(")\n", stdout);
    fputs("      if (", stdout);
    PRINT_CODE(a);
    fputs("(funit_i_) /= ", stdout);
    PRINT_CODE(b);
    fputs("(funit_i_)) then\n", stdout);
    fputs("        write(funit_message_,*) \"", stdout);
    print_macro_arg(a);
    fputs("(\", funit_i_, \") is not equal to ", stdout);
    print_macro_arg(b);
    fputs("(\", funit_i_, \"): \", ", stdout);
    PRINT_CODE(a);
    fputs("(funit_i_), \"vs\", ", stdout);
    PRINT_CODE(b);
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

    if (check_assert_args("assert_array_equal", macro, a, 2))
        return -1;
    b = a->next;

    // length check
    print_array_size_check(a, b);

    // do loop
    fputs("    do funit_i = 1,size(", stdout);
    PRINT_CODE(a);
    fputs(")\n", stdout);
    fputs("      if (abs(", stdout);
    PRINT_CODE(a);
    fputs("(funit_i_) - ", stdout);
    PRINT_CODE(b);
    fprintf(stdout, "(funit_i_)) > %f) then\n", tolerance);
    fputs("        write(funit_message_,*) \"", stdout);
    print_macro_arg(a);
    fprintf(stdout, "(\", funit_i_, \") is not within %f of \"", tolerance);
    print_macro_arg(b);
    fputs("(\", funit_i_, \"): \", ", stdout);
    PRINT_CODE(a);
    fputs("(funit_i_), \"vs\", ", stdout);
    PRINT_CODE(b);
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

    if (check_assert_args("flunk", macro, arg, 1))
        return -1;

    fputs("write(funit_message_,*) ", stdout);
    PRINT_CODE(arg);
    fputs("\n", stdout);
    fputs("    funit_passed_ = .false.\n", stdout);
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
    return -1;
}

static int generate_code(struct Code *code)
{
    switch (code->type) {
    case FORTRAN_CODE:
        PRINT_CODE(code);
        break;
    case MACRO_CODE:
        if (generate_assert(code))
            return -1;
        break;
    default: // arg code
        fprintf(stderr, "near %s:%li: bad code type %i in generate_code\n",
                test_suite_file_name, code->lineno, (int)code->type);
        abort();
        break;
    }
    if (code->next)
        return generate_code(code->next);
    return 0;
}

static int generate_test(struct TestRoutine *test, int *test_i)
{
    if (test->next)
        generate_test(test->next, test_i);

    *test_i += 1;
    printf("  subroutine funit_test%i(funit_passed_, funit_message_)\n", *test_i);
    fputs("    implicit none\n\n", stdout);
    fputs("    logical, intent(out) :: funit_passed_\n", stdout);
    fputs("    character(*), intent(out) :: funit_message_\n", stdout);
    fputs("    integer :: funit_i_\n\n", stdout);

    if (test->code) {
        if (generate_code(test->code))
            return -1;
    }

    fputs("\n    funit_passed_ = .true.\n", stdout);
    printf("  end subroutine funit_test%i\n\n", *test_i);

    return 0;
}

static void generate_support(struct Code *code, const char *type)
{
    printf("  subroutine funit_%s\n", type);
    generate_code(code);
    printf("  end subroutine funit_%s\n\n", type);
}

static void generate_test_call(struct TestSuite *suite,
                               struct TestRoutine *test, int *test_i,
                               size_t max_name)
{
    if (test->next)
        generate_test_call(suite, test->next, test_i, max_name);

    *test_i += 1;

    fputs("\n", stdout);
    if (suite->setup)
        printf("  call funit_setup\n");
    printf("  call funit_test%i(funit_passed_, funit_message_)\n", *test_i);
    fputs("  call pass_fail(funit_passed_, funit_message_, \"", stdout);
    fwrite(test->name, test->name_len, 1, stdout);
    fprintf(stdout, "\", %u)\n", (unsigned int)max_name);
    if (suite->teardown)
        fputs("  call funit_teardown\n\n", stdout);
}

static void print_use(struct TestModule *mod)
{
    if (mod->next)
        print_use(mod->next);

    fputs("  use ", stdout);
    fwrite(mod->name, mod->len, 1, stdout);
    fputs("\n", stdout);
}

static void max_name_width(struct TestRoutine *test, size_t *max)
{
    if (test->name_len > *max)
        *max = test->name_len;
    if (test->next)
        max_name_width(test->next, max);
}

static size_t max_test_name_width(struct TestRoutine *test)
{
    size_t max = 0;
    max_name_width(test, &max);
    return max + 2;
}

static int generate_suite(struct TestSuite *suite, int *suite_i)
{
    int test_i;

    if (suite->next)
        generate_suite(suite->next, suite_i);

    tolerance = (suite->tolerance > 0.0) ? suite->tolerance : DEFAULT_TOLERANCE;

    *suite_i += 1;
    printf("subroutine funit_suite%i\n", *suite_i);
    fputs("  use funit\n", stdout);
    if (suite->mods)
        print_use(suite->mods);
    fputs("\n", stdout);
    fputs("  implicit none\n\n", stdout);
    fputs("  character*1024 :: funit_message_\n", stdout);
    fputs("  logical :: funit_passed_\n\n", stdout);
    if (suite->code)
        generate_code(suite->code);
    if (suite->tests) {
        size_t max_name = max_test_name_width(suite->tests);
        test_i = 0;
        generate_test_call(suite, suite->tests, &test_i, max_name);
    }
    fputs("contains\n\n", stdout);
    if (suite->setup)
        generate_support(suite->setup, "setup");
    if (suite->teardown)
        generate_support(suite->setup, "teardown");
    if (suite->tests) {
        test_i = 0;
        if (generate_test(suite->tests, &test_i))
            return -1;
    }
    printf("end subroutine funit_suite%i\n", *suite_i);

    return 0;
}

static void generate_suite_call(struct TestSuite *suite, int *suite_i)
{
    if (suite->next)
        generate_suite_call(suite->next, suite_i);

    *suite_i += 1;
    fputs("\n  call start_suite(\"", stdout);
    fwrite(suite->name, suite->name_len, 1, stdout);
    fputs("\")\n", stdout);
    fprintf(stdout, "  call funit_suite%i\n", *suite_i);
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
    int suite_i, code = 0;

    suites = parse_suite_file(argv[1]);

    fputs(module_code, stdout);

    suite_i = 0;
    if (generate_suite(suites, &suite_i)) {
        code = -1;
        goto err;
    }

    suite_i = 0;
    generate_main(suites, &suite_i);

 err:
    close_suite_and_free(suites);

    return code;
}
