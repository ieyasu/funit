/* generate_code.c - generate test code from .fun template
 */
#include "funit.h"
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <strings.h>

#include "funit_fortran_module.h"

/* Looks for the string needle in the fixed-length haystack.  Returns the
 * start of the needle if found, else NULL.
 */
static char *strncasestr(char *haystack, size_t haystack_len,
                         const char *needle, size_t needle_len)
{
    size_t i, j;

    assert(needle_len > 0);
#ifndef NDEBUG
    for (i = 0; i < needle_len; i++)
        assert(needle[i] == tolower(needle[i]));
#endif

    i = j = 0;
    while (i < haystack_len) {
        if (tolower(haystack[i]) == needle[j]) {
            j++;
            if (j == needle_len) // needle found
                return haystack + i - needle_len;
        } else {
            j = 0;
        }
        i++;
    }
    return NULL;
}

/* Scan code for an assert macro, returning the start of the macro if found or
 * NULL if not.
 */
static char *find_assert(char *code, size_t len, char *code_end, char **after)
{
    char *a, *f, *assertion;

    a = strncasestr(code, len, "assert_", 7);
    if (a) {
        size_t rest_len = code_end - a;
        char *s = a + 7;
        if (rest_len > 6 && !strncasecmp(s, "array_", 6)) { // accept _array
            if (rest_len > 10 && !strncasecmp(s, "equal_with", 10)) {
                *after = s + 10;
            } else if (rest_len > 5 && !strncasecmp(s, "equal", 5)) {
                *after = s + 5;
            } else {
                a = code_end; // not an assert macro
            }
        } else if (rest_len >  4 && !strncasecmp(s, "true",        4)) {
            *after = s + 4;
        } else if (rest_len <  5 && !strncasecmp(s, "false",       5)) {
            *after = s + 5;
        } else if (rest_len < 10 && !strncasecmp(s, "equal_with", 10)) {
            *after = s + 10;
        } else if (rest_len <  5 && !strncasecmp(s, "equal",       5)) {
            *after = s + 5;
        } else if (rest_len <  9 && !strncasecmp(s, "not_equal",   9)) {
            *after = s + 9;
        } else {
            a = code_end; // not an assert macro
        }
    } else {
        a = code_end; // "assert" never found
    }

    f = strncasestr(code, len, "flunk",  5);
    if (f) {
        *after = f + 5;
    } else {
        f = code_end;
    }

    assertion = a < f ? a : f; // find first of "assert_XXX" and "flunk"

    if (assertion < code_end) {
        switch (**after) {
        case ' ':
        case '\t':
        case '(':
            return assertion; // asertion token terminated with space or (
        default:
            break;
        }
    }
    return code_end;
}

/* assert_true(expr[,msg]) becomes:
 *
 *     if (.not. (-expr-)) then
 *       write(_message,*) "-expr-", "is false"
 *       _funit_passed = .false.
 *       return
 *     end if
 */
static char *generate_assert_true(char *s, size_t len)
{
}

/* assert_false(expr[,msg]) becomes:
 *
 *     if (-expr-) then
 *       write(_message,*) "-expr-", "is true"
 *       _funit_passed = .false.
 *       return
 *     end if
 */
static char *generate_assert_false(char *s, size_t len)
{
}

/* assert_equal(a,b[,msg]) becomes:
 *
 *     if ((a) /= (b)) then
 *       write(_message,*) "-a- is not equal to -b-"
 *       _funit_passed = .false.
 *       return
 *     end if
 */
static char *generate_assert_equal(char *s, size_t len)
{
}

/* assert_not_equal(a,b[,msg]) becomes:
 *
 *     if ((a) == (b)) then
 *       write(_message,*) "-a- is not equal to -b-"
 *       _funit_passed = .false.
 *       return
 *     end if
 */
static char *generate_assert_not_equal(char *s, size_t len)
{
}

/* assert_equal_with(a,b[,msg]) becomes:
 *
 *     if (abs((a) - (b)) > TOLERANCE) then
 *       write(_message,*) "-a- is not within", TOLERANCE, "of -b-"
 *       _funit_passed = .false.
 *       return
 *     end if
 */
static char *generate_assert_equal_with(char *s, size_t len)
{
}

/* assert_array_equal(a,b[,msg]) becomes:
 *
 *     if (size(a) /= size(b)) then
 *       write(_message,*) "-a- is length", size(a), &
 *         "which is not the same as -b- which is length", size(b)
 *       _funit_passed = .false.
 *       return
 *     end if
 *     do _funit_i = 1,size(a)
 *       if (a(_funit_i) /= b(_funit_i)) then
 *         write(_message,*) "-a-(", _funit_i, ") is not equal to -b-(", &
 *           _funit_i, "): ", a(_funit_i), "vs", b(_funit_i)
 *         _funit_passed = .false.
 *         return
 *       end if
 *     end do
 */
static char *generate_assert_array_equal(char *s, size_t len)
{
}

/* assert_array_equal(a,b[,msg]) becomes:
 *
 *     if (size(a) /= size(b)) then
 *       write(_message,*) "-a- is length", size(a), &
 *         "which is not the same as -b- which is length", size(b)
 *       _funit_passed = .false.
 *       return
 *     end if
 *     do _funit_i = 1,size(a)
 *       if (abs(a(_funit_i) - b(_funit_i)) > TOLERANCE) then
 *         write(_message,*) "-a-(", _funit_i, ") is not within", TOLERANCE, &
 *           "of -b-(", _funit_i, "): ", a(_funit_i), "vs", b(_funit_i)
 *         _funit_passed = .false.
 *         return
 *       end if
 *     end do
 */
static char *generate_assert_array_equal_with(char *s, size_t len)
{
}

/* flunk(msg) becomes:
 *
 *     write(_funit_message,*) -msg-
 *     _funit_passed = .false.
 *     return
 */
static char *generate_flunk(char *s, size_t len)
{
}

static char *generate_assert(char *assert, size_t len)
{
    if (len > 7 && !strncasecmp(assert, "assert_", 7)) {
        char *s = assert + 7; // skip "assert_"
        len -= 7;
        if (len > 6 && !strncasecmp(s, "array_", 6)) {
            char *t = s + 6; // skip "array_"
            len -= 6;
            if (len > 10 && !strncasecmp(t, "equal_with", 10)) {
                return generate_assert_array_equal_with(s + 10, len - 10);
            } else {
                assert(len > 5 && !strncasecmp(t, "equal", 5));
                return generate_assert_array_equal(s + 5, len - 5);
            }
        } else {
            if (!strncasecmp(s, "true", 4)) {
                return generate_assert_true(s + 4, len - 4);
            } else if (!strncasecmp(s, "false",       5)) {
                return generate_assert_false(s + 5, len - 5);
            } else if (!strncasecmp(s, "equal_with", 10)) {
                return generate_assert_equal_with(s + 10, len - 10);
            } else if (!strncasecmp(s, "equal",       5)) {
                return generate_assert_equal(s + 5, len - 5);
            } else {
                assert(len > 9 && !strncasecmp(s, "not_equal", 9));
                return generate_assert_not_equal(s + 9, len - 9);
            }
        }
    } else {
        assert(len > 5 && !strncasecmp(assert, "flunk", 5));
        return generate_flunk(assert + 5, len - 5);
    }
}

static void generate_code(struct Code *code)
{
    char *s, *code_end, *after;

    if (code->next)
        generate_code(code->next);

    s = code->str;
    code_end = code->str + code->len;
    for (;;) {
        size_t len = code_end - s;
        char *assert = find_assert(s, len, code_end, &after);
        // emit code up to assertion
        fwrite(s, assert - s, 1, stdout);
        // if assert found, dissect
        if (assert < code_end) {
            // XXX make sure advances s past the whole macro
            s = generate_assert(assert, len);
        } else {
            break;
        }
    }
}

static void generate_test(struct TestRoutine *test, int *test_i)
{
    if (test->next)
        generate_test(test->next, test_i);

    *test_i += 1;
    printf("  subroutine _funit_test%i(_funit_passed, _funit_message)\n", *test_i);
    fputs("    implicit none\n", stdout);
    fputs("    logical, intent(out) :: _funit_passed\n", stdout);
    fputs("    character(*), intent(out) :: _funit_message\n", stdout);
    fputs("    integer :: _funit_i\n\n", stdout);

    if (test->code)
        generate_code(test->code);
    printf("  end subroutine _funit_test%i\n\n", *test_i);
}

static void generate_support(struct Code *code, const char *type)
{
    printf("  subroutine _funit_%s\n", type);
    generate_code(code);
    printf("  end subroutine _funit_%s\n\n", type);
}

static void generate_test_call(struct TestSuite *suite,
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

static void generate_suite(struct TestSuite *suite, int *suite_i)
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

static void generate_suite_call(struct TestSuite *suite, int *suite_i)
{
    if (suite->next)
        generate_suite_call(suite->next, suite_i);

    *suite_i += 1;
    fputs("\n  call start_suite(\"", stdout);
    fwrite(suite->name, suite->name_len, 1, stdout);
    fputs("\")\n", stdout);
    printf("  call _funit_suite%i\n", *suite_i);
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
