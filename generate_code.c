/* generate_code.c - generate test code from .fun template
 */
#include "funit.h"

// for module_code string variable
#include "funit_fortran_module.h"

// XXX no more globals!
static FILE *fout;
const char *test_set_file_name;
static double tolerance = -1.0;

static int check_assert_args2(const char *macro_name, struct Code *macro,
                              struct Code *args, int min_args, int max_args)
{
    long lineno;
    int n;

    assert(min_args > 0 && min_args <= max_args);

    if (!args) {
        fprintf(stderr, "near %s:%li: no arguments to %s()\n",
                test_set_file_name, macro->lineno, macro_name);
        return -1;
    }
    n = 0;
    while (args) {
        lineno = args->lineno;
        n++;
        if (n > max_args)
            goto badargs;
        args = args->next;
    }
    if (n < min_args)
        goto badargs;
    return n;
 badargs:
    if (min_args == max_args) {
        fprintf(stderr, "near %s:%li: expected %i argument%s to %s()\n",
                test_set_file_name, lineno, max_args,
                (max_args > 1) ? "s" : "", macro_name);
    } else {
        assert(min_args + 1 == max_args);
        fprintf(stderr, "near %s:%li: expected %i or %i arguments to %s()\n",
                test_set_file_name, lineno, min_args, max_args, macro_name);
    }
    return -1;
}

static int check_assert_args(const char *macro_name, struct Code *macro,
                              struct Code *args, int n_expected)
{
    return check_assert_args2(macro_name, macro, args, n_expected, n_expected);
}

// plain printing
#define PRINT_CODE(arg) fwrite((arg)->u.c.str, (arg)->u.c.len, 1, fout)

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
            fwrite(start, amp - start, 1, fout);
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
    fwrite(start, end - start, 1, fout);
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

    if (check_assert_args("assert_true", macro, arg, 1) != 1)
        return -1;

    fputs("! assert_true()\n", fout);
    fputs("    if (.not. (", fout);
    PRINT_CODE(arg);
    fputs(")) then\n", fout);
    fputs("      write(funit_message_,*) \"'", fout);
    print_macro_arg(arg);
    fputs("' is false\"\n", fout);
    fputs("      funit_passed_ = .false.\n", fout);
    fputs("      return\n", fout);
    fputs("    end if", fout);

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

    if (check_assert_args("assert_false", macro, arg, 1) != 1)
        return -1;

    fputs("! assert_false()\n", fout);
    fputs("    if (", fout);
    PRINT_CODE(arg);
    fputs(") then\n", fout);
    fputs("      write(funit_message_,*) \"'", fout);
    print_macro_arg(arg);
    fputs("' is true\"\n", fout);
    fputs("      funit_passed_ = .false.\n", fout);
    fputs("      return\n", fout);
    fputs("    end if", fout);

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

    if (check_assert_args("assert_equal", macro, a, 2) != 2)
        return -1;
    b = a->next;

    fputs("! assert_equal()\n", fout);
    fputs("    if ((", fout);
    PRINT_CODE(a);
    fputs(") /= (", fout);
    PRINT_CODE(b);
    fputs(")) then\n", fout);
    fputs("      write(funit_message_,*) \"'",fout);
    print_macro_arg(a);
    fputs("' (\", ", fout);
    PRINT_CODE(a);
    fputs(", &\n\") is not equal to '", fout);
    print_macro_arg(b);
    fputs("'\"\n", fout);
    fputs("      funit_passed_ = .false.\n", fout);
    fputs("      return\n", fout);
    fputs("    end if", fout);

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

    if (check_assert_args("assert_not_equal", macro, a, 2) != 2)
        return -1;
    b = a->next;

    fputs("! assert_not_equal()\n", fout);
    fputs("    if ((", fout);
    PRINT_CODE(a);
    fputs(") == (", fout);
    PRINT_CODE(b);
    fputs(")) then\n", fout);
    fputs("      write(funit_message_,*) \"'",fout);
    print_macro_arg(a);
    fputs("' (\", ", fout);
    PRINT_CODE(a);
    fputs(", &\n\") is equal to '", fout);
    print_macro_arg(b);
    fputs("'\"\n", fout);
    fputs("      funit_passed_ = .false.\n", fout);
    fputs("      return\n", fout);
    fputs("    end if", fout);

    return 0;
}

/* assert_equal_with(a,b[,tol]) becomes:
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
    double this_tolerance;
    int num_args;

    num_args = check_assert_args2("assert_equal_with", macro, a, 2, 3);
    if (num_args < 2) {
        return -1;
    }
    b = a->next;
    if (num_args == 2) {
        if (tolerance <= 0.0) {
            fprintf(stderr, "near %s:%li: missing a tolerance argument or "
                    "a set-level default tolerance\n",
                    test_set_file_name, macro->lineno);
            return -1;
        }
        this_tolerance = tolerance;
    } else if (num_args == 3) {
        this_tolerance = strtod(b->next->u.c.str, NULL);
        if (this_tolerance <= 0.0) {
            fprintf(stderr, "near %s:%li: in assert_array_equal(): parsed "
                    "a tolerance <= 0.0; you need to fix that\n",
                    test_set_file_name, macro->lineno);
            return -1;
        }
    }

    fprintf(fout, "! assert_equal_with(%s)\n", (num_args == 3) ? "tol" : "");
    fputs("    if (abs((", fout);
    PRINT_CODE(a);
    fputs(") - (", fout);
    PRINT_CODE(b);
    fprintf(fout, ")) > %g) then\n", this_tolerance);
    fputs("      write(funit_message_,*) \"'",fout);
    print_macro_arg(a);
    fputs("' (\", ", fout);
    PRINT_CODE(a);
    fprintf(fout, ", &\n\") is not within %g of '", this_tolerance);
    print_macro_arg(b);
    fputs("'\"\n", fout);
    fputs("      funit_passed_ = .false.\n", fout);
    fputs("      return\n", fout);
    fputs("    end if", fout);

    return 0;
}

static void print_array_size_check(struct Code *a, struct Code *b)
{
    fputs("    if (size(", fout);
    PRINT_CODE(a);
    fputs(") /= size(", fout);
    PRINT_CODE(b);
    fputs(")) then\n", fout);
    fputs("      write(funit_message_,*) \"'",fout);
    print_macro_arg(a);
    fputs("' and '", fout);
    print_macro_arg(b);
    fputs("' &\n        &are not the same length:\", size(", fout);
    PRINT_CODE(a);
    fputs("), \"vs.\", size(", fout);
    PRINT_CODE(b);
    fputs(")\n", fout);
    fputs("      funit_passed_ = .false.\n", fout);
    fputs("      return\n", fout);
    fputs("    end if\n", fout);
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

    if (check_assert_args("assert_array_equal", macro, a, 2) != 2)
        return -1;
    b = a->next;

    fputs("! assert_array_equal()\n", fout);

    print_array_size_check(a, b);

    // do loop
    fputs("    do funit_i_ = 1,size(", fout);
    PRINT_CODE(a);
    fputs(")\n", fout);
    fputs("      if (", fout);
    PRINT_CODE(a);
    fputs("(funit_i_) /= ", fout);
    PRINT_CODE(b);
    fputs("(funit_i_)) then\n", fout);
    fputs("        write(funit_message_,*) \"", fout);
    print_macro_arg(a);
    fputs("(\", funit_i_, &\n          \") is not equal to ", fout);
    print_macro_arg(b);
    fputs("(\", funit_i_, &\n          \"): \", ", fout);
    PRINT_CODE(a);
    fputs("(funit_i_), \"vs\", ", fout);
    PRINT_CODE(b);
    fputs("(funit_i_)\n", fout);
    fputs("        funit_passed_ = .false.\n", fout);
    fputs("        return\n", fout);
    fputs("      end if\n", fout);
    fputs("    end do", fout);

    return 0;
}

/* assert_array_equal_with(a,b[,tol]) becomes:
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
    float this_tolerance;
    int num_args;

    num_args = check_assert_args2("assert_array_equal", macro, a, 2, 3);
    if (num_args < 2) {
        return -1;
    }
    b = a->next;
    if (num_args == 2) {
        if (tolerance <= 0.0) {
            fprintf(stderr, "near %s:%li: in assert_array_equal(): missing "
                    "a tolerance argument or a set-level default tolerance\n",
                    test_set_file_name, macro->lineno);
            return -1;
        }
        this_tolerance = tolerance;
    } else if (num_args == 3) {
        this_tolerance = strtod(b->next->u.c.str, NULL);
        if (this_tolerance <= 0.0) {
            fprintf(stderr, "near %s:%li: in assert_array_equal(): parsed "
                    "a tolerance <= 0.0; you need to fix that\n",
                    test_set_file_name, macro->lineno);
            return -1;
        }
    }

    fprintf(fout, "! assert_array_equal_with(%s)\n",
            (num_args == 3) ? "tol" : "");

    // length check
    print_array_size_check(a, b);

    // do loop
    fputs("    do funit_i_ = 1,size(", fout);
    PRINT_CODE(a);
    fputs(")\n", fout);
    fputs("      if (abs(", fout);
    PRINT_CODE(a);
    fputs("(funit_i_) - ", fout);
    PRINT_CODE(b);
    fprintf(fout, "(funit_i_)) > %g) then\n", this_tolerance);
    fputs("        write(funit_message_,*) \"", fout);
    print_macro_arg(a);
    fprintf(fout, "(\", funit_i_, &\n          \") is not within %g of ",
            this_tolerance);
    print_macro_arg(b);
    fputs("(\", funit_i_, &\n          \"): \", ", fout);
    PRINT_CODE(a);
    fputs("(funit_i_), \"vs\", ", fout);
    PRINT_CODE(b);
    fputs("(funit_i_)\n", fout);
    fputs("        funit_passed_ = .false.\n", fout);
    fputs("        return\n", fout);
    fputs("      end if\n", fout);
    fputs("    end do", fout);

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

    if (check_assert_args("flunk", macro, arg, 1) != 1)
        return -1;

    fputs("! flunk()\n", fout);
    fputs("    write(funit_message_,*) ", fout);
    PRINT_CODE(arg);
    fputs("\n", fout);
    fputs("    funit_passed_ = .false.\n", fout);
    fputs("    return\n", fout);

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
    default:
        fprintf(stderr, "unknown assert type %i\n", macro->u.m.type);
        abort();
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
                test_set_file_name, code->lineno, (int)code->type);
        abort();
        break;
    }
    if (code->next)
        return generate_code(code->next);
    return 0;
}

static int generate_test(struct TestCase *test, int *test_i)
{
    if (test->next)
        generate_test(test->next, test_i);

    *test_i += 1;
    fprintf(fout, "  subroutine funit_test%i(funit_passed_, funit_message_)\n",
            *test_i);
    fputs("    implicit none\n\n", fout);
    fputs("    logical, intent(out) :: funit_passed_\n", fout);
    fputs("    character(*), intent(out) :: funit_message_\n", fout);
    if (test->need_array_iterator)
        fputs("    integer :: funit_i_\n", fout);
    fputs("\n", fout);

    if (test->code) {
        if (generate_code(test->code))
            return -1;
    }

    fputs("\n    funit_passed_ = .true.\n", fout);
    fprintf(fout, "  end subroutine funit_test%i\n\n", *test_i);

    return 0;
}

static void generate_support(struct Code *code, const char *type)
{
    fprintf(fout, "  subroutine funit_%s\n", type);
    generate_code(code);
    fprintf(fout, "  end subroutine funit_%s\n\n", type);
}

static void generate_test_call(struct TestSet *set,
                               struct TestCase *test, int *test_i,
                               size_t max_name)
{
    if (test->next)
        generate_test_call(set, test->next, test_i, max_name);

    *test_i += 1;

    fputs("\n", fout);
    if (set->setup)
        fprintf(fout, "  call funit_setup\n");
    fprintf(fout, "  call funit_test%i(funit_passed_, funit_message_)\n",
            *test_i);
    fputs("  call pass_fail(funit_passed_, funit_message_, \"", fout);
    fwrite(test->name, test->name_len, 1, fout);
    fprintf(fout, "\", %u)\n", (unsigned int)max_name);
    if (set->teardown)
        fputs("  call funit_teardown\n\n", fout);
}

static void print_use(struct TestModule *mod)
{
    if (mod->next)
        print_use(mod->next);

    fputs("  use ", fout);
    fwrite(mod->name, mod->len, 1, fout);
    if (mod->elen > 0) {
        fwrite(mod->extra, mod->elen, 1, fout);
    }
    fputs("\n", fout);
}

static void max_name_width(struct TestCase *test, size_t *max)
{
    if (test->name_len > *max)
        *max = test->name_len;
    if (test->next)
        max_name_width(test->next, max);
}

static size_t max_test_name_width(struct TestCase *test)
{
    size_t max = 0;
    max_name_width(test, &max);
    return max + 2;
}

static int generate_set(struct TestSet *set, int *set_i)
{
    int test_i;

    if (set->next)
        generate_set(set->next, set_i);

    tolerance = (set->tolerance > 0.0) ? set->tolerance : DEFAULT_TOLERANCE;

    (*set_i)++;
    fprintf(fout, "subroutine funit_set%i\n", *set_i);
    fputs("  use funit\n", fout);
    
    if (set->mods)
        print_use(set->mods);
    
    fputs("\n", fout);
    fputs("  implicit none\n\n", fout);
    fputs("  character*1024 :: funit_message_\n", fout);
    fputs("  logical :: funit_passed_\n\n", fout);
    
    if (set->code)
        generate_code(set->code);
    
    if (set->tests) {
        size_t max_name = max_test_name_width(set->tests);
        test_i = 0;
        generate_test_call(set, set->tests, &test_i, max_name);
    }
    
    fputs("contains\n\n", fout);
    
    if (set->setup)
        generate_support(set->setup, "setup");
    if (set->teardown)
        generate_support(set->setup, "teardown");
    if (set->tests) {
        test_i = 0;
        if (generate_test(set->tests, &test_i))
            return -1;
    }

    fprintf(fout, "end subroutine funit_set%i\n", *set_i);

    return 0;
}

static void generate_set_call(struct TestSet *set, int *set_i)
{
    if (set->next)
        generate_set_call(set->next, set_i);

    (*set_i)++;
    fputs("\n  call start_set(\"", fout);
    fwrite(set->name, set->name_len, 1, fout);
    fputs("\")\n", fout);
    fprintf(fout, "  call funit_set%i\n", *set_i);
}

static void generate_main(struct TestSet *file, int *set_i)
{
    fputs("\n\nprogram main\n", fout);
    fputs("  use funit\n\n",  fout);
    fputs("  call clear_stats\n", fout);
    generate_set_call(file, set_i);
    fputs("\n  call report_stats\n", fout);
    fprintf(fout, "end program main\n");
}

int generate_code_file(struct TestSet *set, FILE *file_out)
{
    // set file-wide out file pointer
    fout = file_out;

    // XXX look for this file and emit it if not present
    fputs(module_code, fout);

    int set_i = 0;
    if (generate_set(set, &set_i))
        return -1;
    set_i = 0;
    generate_main(set, &set_i);

    return 0;
}
