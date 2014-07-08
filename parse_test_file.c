/* parse_test_file.c - parse a test file like the one below:

    set cname
      dep "../file1.F90"
      dep "../file2.F90"
    
      use a_module
    
      tolerance 0.00001
    
      setup
        ! fortran code to run before each test case
      end setup
    
      teardown
        ! fortran code to run after each test case
      end teardown
    
      test name1
        ...
        assert_equal(a, 7)
        ...
      end test name
    end set cname
 */
#include "funit.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>

static const char *END_OF_LINE = (char *)0x1;

static struct Code *parse_fortran(struct TestFile *, int *);

static void free_code(struct Code *code)
{
    if (code->next)
        free_code(code->next);
    if (code->type == MACRO_CODE) {
        if (code->u.m.args)
            free_code(code->u.m.args);
    }
    free(code);
}

static void free_cases(struct TestCase *test)
{
    if (test->next)
        free_cases(test->next);
    if (test->code)
        free_code(test->code);
    free(test);
}

static void free_deps(struct TestDependency *deps)
{
    if (deps->next)
        free_deps(deps->next);
    free(deps);
}

static void free_sets(struct TestSet *set)
{
    if (set->next)
        free_sets(set->next);
    if (set->deps)
        free_deps(set->deps);
    if (set->setup)
        free_code(set->setup);
    if (set->teardown)
        free_code(set->teardown);
    if (set->tests)
        free_cases(set->tests);
    if (set->code)
        free_code(set->code);
    free(set);
}

/* Call when done with the test file and all data structures associated
 * with it.  Frees the TestSet list returned by parse_test_file().
 */
void close_testfile(struct TestFile *tf)
{
    assert(tf != NULL);

    if (tf->sets)
        free_sets(tf->sets);

    if (tf->file_buf) {
        if (!munmap(tf->file_buf, tf->statbuf.st_size)) {
            tf->file_buf = NULL;
        } else {
            fprintf(stderr, "Unmapping file %s: %s\n", tf->path,
                    strerror(errno));
            abort();
        }
    }

    if (tf->fd && !close(tf->fd)) {
        tf->fd = 0;
        tf->path = NULL;
    } else {
        fprintf(stderr, "Closing file %s: %s\n", tf->path,
                strerror(errno));
        abort();
    }

    free(tf);
}

static void fail(struct TestFile *tf, const char *col, const char *message)
{
    assert(tf->line_pos != NULL);

    // file:line:
    assert(tf->path != NULL);
    fprintf(stderr, "%s:%li:\n\n", tf->path, tf->lineno);
    // code
    fwrite(tf->line_pos, tf->next_line_pos - tf->line_pos, 1, stderr);
    fputs("\n", stderr);
    // column indicator
    if (col) {
        int n = col - tf->line_pos;
        int spaces = n > 2 ? n - 2 : n;
        while (spaces > 0) {
            fputs(" ", stderr);
            spaces--;
        }
        if (n > 2)
            fputs("--", stderr);
        fputs("^", stderr);
        if (n <= 2)
            fputs("--", stderr);
        fputs("\n", stderr);
    }
    // error message
    fprintf(stderr, "Error: %s\n", message);
}

static void vfail(struct TestFile *tf, const char *col, const char *format, ...)
{
    char message[256];
    va_list ap;

    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);

    fail(tf, col, message);
}

static void syntax_error(struct TestFile *tf)
{
    fail(tf, tf->read_pos, "syntax error");
}

/* Returns a pointer to the next line of data, or NULL if at last line.
 * Does not handle comments.
 */
static char *next_line(struct TestFile *tf)
{
    char *prev_line;

    assert(tf->file_buf != NULL);

    if (tf->line_pos) {
        assert(tf->next_line_pos >= tf->line_pos ||
               tf->line_pos == tf->file_end);
        prev_line = tf->line_pos;
        tf->line_pos = tf->next_line_pos;
        // read past \r\n
        while (tf->line_pos < tf->file_end) {
            if (*tf->line_pos == '\n') {
                tf->line_pos++;
                tf->lineno++;
                break;
            } else if (*tf->line_pos == '\r') {
                if (tf->line_pos + 1 < tf->file_end &&
                    *(tf->line_pos + 1) == '\n')
                    tf->line_pos += 2;
                else
                    tf->line_pos++;
                tf->lineno++;
                break;
            }
            tf->line_pos++;
        }
        if (tf->line_pos >= tf->file_end) {
            tf->line_pos = prev_line;
            tf->lineno--;
            tf->read_pos = tf->file_end;
            return NULL;
        }
    } else { // very first line
        tf->line_pos = tf->file_buf;
        tf->lineno = 1;
    }
    tf->read_pos = tf->next_pos = tf->line_pos;

    // find next \r\n
    tf->next_line_pos = tf->line_pos;
    while (tf->next_line_pos < tf->file_end) {
        switch (*tf->next_line_pos) {
        case '\n':
        case '\r':
            goto eol;
        default:
            tf->next_line_pos++;
            break;
        }
    }
 eol:
    return tf->line_pos;
}

static char *skip_ws(struct TestFile *tf)
{
    assert(tf->read_pos != NULL);

    while (tf->read_pos < tf->next_line_pos) {
        switch (*tf->read_pos) {
        case ' ':
        case '\t':
            tf->read_pos++;
            break;
        default:
            goto eol;
        }
    }
 eol:
    return tf->read_pos;
}

static char *skip_next_ws(struct TestFile *tf)
{
    assert(tf->next_pos != NULL);

    while (tf->next_pos < tf->next_line_pos) {
        switch (*tf->next_pos) {
        case ' ':
        case '\t':
            tf->next_pos++;
            break;
        default:
            goto eol;
        }
    }
 eol:
    return tf->next_pos;
}

typedef void end_finder_fun(struct TestFile *tf);

static char *
next_thing(struct TestFile *tf, size_t *len, end_finder_fun end_fun)
{
    tf->read_pos = tf->next_pos;
    skip_ws(tf);

    tf->next_pos = tf->read_pos;
    end_fun(tf);

    if (tf->next_pos - tf->read_pos > 0) {
        if (len)
            *len = tf->next_pos - tf->read_pos;
        return tf->read_pos;
    }
    return (char *)END_OF_LINE;
}

static void token_end_finder(struct TestFile *tf)
{
    while (tf->next_pos < tf->next_line_pos) {
        switch (*tf->next_pos) {
        case ' ':
        case '\t':
        case '!':
        case '\r':
        case '\n':
        case ',':
            return;
        default:
            tf->next_pos++;
            break;
        }
    }
}

static char *next_token(struct TestFile *tf, size_t *len)
{
    return next_thing(tf, len, token_end_finder);
}

static void name_end_finder(struct TestFile *tf)
{
    while (tf->next_pos < tf->next_line_pos) {
        switch (*tf->next_pos) {
        case '!':
        case '\r':
        case '\n':
            return;
        default:
            tf->next_pos++;
            break;
        }
    }
}

static char *next_name(struct TestFile *tf, size_t *len)
{
    return next_thing(tf, len, name_end_finder);
}

static void quoted_string_end_finder(struct TestFile *tf)
{
    if (*tf->next_pos != '"')
        return;
    tf->next_pos++;
    while (tf->next_pos < tf->next_line_pos) {
        switch (*tf->next_pos) {
        case '"':
            tf->next_pos++;
            return;
        case '!':
        case '\r':
        case '\n':
            return;
        default:
            tf->next_pos++;
            break;
        }
    }
}

static char *next_quoted_string(struct TestFile *tf, size_t *len)
{
    size_t full_len;
    char *s;

    s = next_thing(tf, &full_len, quoted_string_end_finder);

    assert(s != NULL);
    if (s == END_OF_LINE || *s != '"') {
        fail(tf, s, "expected a quote (\") to begin a string");
        return NULL;
    }

    if (tf->next_pos == s || *(tf->next_pos - 1) != '"') {
        fail(tf, tf->next_pos, "expected a quote(\") to end the string");
        return NULL;
    }

    tf->read_pos++; // skip past leading "
    if (len)
        *len = tf->next_pos - tf->read_pos - 1;
    return tf->read_pos;
}

/* Looks for the string needle in the fixed-length haystack.  Returns the
 * start of the needle if found, else NULL.  The needle should be all
 * lower case.
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
                return haystack + i - needle_len + 1;
        } else {
            j = 0;
        }
        i++;
    }
    return NULL;
}

static char *skip_line_continuation(struct TestFile *tf, int in_string)
{
    assert(*tf->next_pos == '&');
    tf->next_pos++;

    // expect ' ' or '\t' only until next_line_pos
    skip_next_ws(tf);
    if (tf->next_pos < tf->next_line_pos) {
        fail(tf, tf->next_pos, "expected newline after '&'");
        return NULL;
    }
    while (tf->next_pos < tf->file_end) {
        char *save_read_pos = tf->read_pos;
        if (!next_line(tf))
            break;
        tf->read_pos = save_read_pos;
        skip_next_ws(tf);
        switch (*tf->next_pos) {
        case '!': // skip comment lines
            break;
        case '&':
            if (in_string)
                tf->next_pos++; // skip past whitespace truncator
            // FALLTHROUGH
        default:
            return tf->next_pos;
        }
    }
    syntax_error(tf);
    return NULL;
}

static char *skip_ampersand_in_string(struct TestFile *tf)
{
    char *amp_pos = tf->next_pos;

    assert(*tf->next_pos == '&');
    tf->next_pos++;

    skip_next_ws(tf);
    if (tf->next_pos < tf->next_line_pos &&
        *tf->next_pos != '\r' && *tf->next_pos != '\n') {
        // not a line continuation
        tf->next_pos--;
        return tf->next_pos;
    }
    tf->next_pos = amp_pos;
    return skip_line_continuation(tf, 1);
}

/* Look for the next ',' or un-balanced ')'.  Returns a pointer to that
 * ',' or ')' or NULL if not found before end of file.  This may continue
 * to the next line.
 */
static char *split_macro_arg(struct TestFile *tf)
{
    int paren_count = 0, in_string = 0;
    char string_delim;

    tf->next_pos = tf->read_pos;
    while (tf->next_pos < tf->next_line_pos) {
        switch (*tf->next_pos) {
        case '\'':
        case '"': // open or close a string
            if (in_string) {
                if (*tf->next_pos == string_delim) {
                    if (*(tf->next_pos + 1) == string_delim) {
                        tf->next_pos++; // doubled to escape
                    } else {
                        in_string = 0; // close quote
                    }
                }
            } else {
                in_string = -1;
                string_delim = *tf->next_pos;
            }
            break;
        case '(':
            if (!in_string) {
                paren_count++;
            }
            break;
        case ')':
            if (!in_string) {
                if (paren_count == 0) {
                    return tf->next_pos; // closing paren or separating comma
                } else {
                    paren_count--;
                }
            }
            break;
        case ',':
            if (!in_string && paren_count == 0) {
                return tf->next_pos; // closing paren or separating comma
            }
            break;
        case '&': // line continuation
            if (in_string) {
                if (!skip_ampersand_in_string(tf))
                    return NULL;
            } else { // line continuation
                if (!skip_line_continuation(tf, 0))
                    return NULL;
            }
            break;
        case '\r':
        case '\n':
            fail(tf, tf->next_pos, "unexpected end of line");
            return NULL;
        default:
            break;
        }
        assert(paren_count >= 0);
        tf->next_pos++;
    }
    return NULL;
}

static struct Code *parse_macro_args(struct TestFile *tf, int *error)
{
    struct Code *code;

    if (!split_macro_arg(tf)) {
        *error = 1;
        return NULL;
    }

    code = NEW0(struct Code);
    code->type = ARG_CODE;
    code->lineno = tf->lineno;
    code->u.c.str = tf->read_pos;
    code->u.c.len = tf->next_pos - tf->read_pos;
    tf->read_pos = tf->next_pos + 1;
    if (*tf->next_pos == ',') {
        tf->next_pos++;
        skip_ws(tf);
        code->next = parse_macro_args(tf, error);
        if (*error) {
            free(code);
            return NULL;
        }
    }
    assert(*tf->next_pos == ')');
    return code;
}

static struct Code *
parse_macro(struct TestFile *tf, enum MacroType mtype, int *need_array_it)
{
    struct Code *code = NEW0(struct Code);
    int error = 0;

    code->type = MACRO_CODE;
    code->lineno = tf->lineno;
    code->u.m.type = mtype;
    code->u.m.args = parse_macro_args(tf, &error);
    if (error)
        goto cleanup;
    if (code->u.m.args) {
        tf->read_pos = tf->next_pos + 1;
        code->next = parse_fortran(tf, need_array_it);
        if (code->next)
            return code;
    }
    fail(tf, tf->read_pos, "expected macro argument");
 cleanup:
    free_code(code);
    return NULL;
}

/* Scan code for an assert macro, returning the start of the macro if found or
 * NULL if not. The out param type returns the assertion type, and afterp is
 * set to point immediately after the macro's opening paren.
 */
static char *
find_macro(struct TestFile *tf, enum MacroType *type, int *need_array_it)
{
    size_t len = tf->next_line_pos - tf->read_pos;
    char *assert_pos;

    assert_pos = strncasestr(tf->read_pos, len, "assert_", 7);
    if (assert_pos) {
        if (!need_array_it) {
            fail(tf, assert_pos, "assertions not allowed here");
        }
        char *s = assert_pos + 7;
        size_t rest_len = tf->next_line_pos - s;
        if (rest_len > 6 && !strncasecmp(s, "array_", 6)) { // accept array_
            s += 6;
            rest_len -= 6;
            if (rest_len > 10 && !strncasecmp(s, "equal_with", 10)) {
                *need_array_it = 1;
                tf->next_pos = s + 10;
                *type = ASSERT_ARRAY_EQUAL_WITH;
            } else if (rest_len > 5 && !strncasecmp(s, "equal", 5)) {
                *need_array_it = 1;
                tf->next_pos = s + 5;
                *type = ASSERT_ARRAY_EQUAL;
            } else {
                assert_pos = NULL; // not an assert macro
            }
        } else if (rest_len >  4 && !strncasecmp(s, "true",        4)) {
            tf->next_pos = s + 4;
            *type = ASSERT_TRUE;
        } else if (rest_len >  5 && !strncasecmp(s, "false",       5)) {
            tf->next_pos = s + 5;
            *type = ASSERT_FALSE;
        } else if (rest_len > 10 && !strncasecmp(s, "equal_with", 10)) {
            tf->next_pos = s + 10;
            *type = ASSERT_EQUAL_WITH;
        } else if (rest_len >  5 && !strncasecmp(s, "equal",       5)) {
            tf->next_pos = s + 5;
            *type = ASSERT_EQUAL;
        } else if (rest_len >  9 && !strncasecmp(s, "not_equal",   9)) {
            tf->next_pos = s + 9;
            *type = ASSERT_NOT_EQUAL;
        } else {
            assert_pos = NULL; // not an assert macro
        }
    } else {
        assert_pos = strncasestr(tf->read_pos, len, "flunk",  5);
        if (assert_pos) {
            tf->next_pos = assert_pos + 5;
            *type = FLUNK;
        } else {
            assert_pos = NULL; // no assertions found
        }
    }

    if (assert_pos) {
        // make sure not commented out
        char *s = assert_pos;
        while (--s >= tf->line_pos) {
            if (*s == '!')
                return NULL;
        }

        // find open paren
        while (tf->next_pos < tf->next_line_pos) {
            switch (*tf->next_pos) {
            case ' ':
            case '\t':
                tf->next_pos++;
                break;
            case '(':
                tf->next_pos++;
                tf->read_pos = assert_pos;
                return assert_pos;
            default:
                fail(tf, tf->next_pos, "expected '('");
                return NULL;
            }
        }
    }
    return NULL;
}

static int same_token(const char *expected, size_t expected_len,
                      const char *actual, size_t actual_len)
{
    return (actual != END_OF_LINE && expected_len == actual_len &&
            !strncasecmp(expected, actual, expected_len));
}

/* Tokens which end parsing of fortran code.
*/
static int is_test_token(char *tok, size_t len)
{
    return (tok != END_OF_LINE &&
            (same_token(tok, len, "test",        4) ||
             same_token(tok, len, "setup",       5) ||
             same_token(tok, len, "teardown",    8) ||
             same_token(tok, len, "dep",         3) ||
             // XXX add 'use'?
             same_token(tok, len, "tolerance",   9)));
}

/* Tokens which follow an "end" token to denote a sequence of non-fortran code.
 */
static int next_is_test_end_token(struct TestFile *tf)
{
    char *tok;
    size_t len;

    tok = next_token(tf, &len);
    assert(tok != NULL);

    return (tok != END_OF_LINE &&
            (same_token(tok, len, "test",        4) ||
             same_token(tok, len, "setup",       5) ||
             same_token(tok, len, "teardown",    8) ||
             same_token(tok, len, "set", 3)));
}

static struct Code *parse_fortran(struct TestFile *tf, int *need_array_it)
{
    struct Code *code = NEW0(struct Code);
    char *start = tf->read_pos, *tok, *save_pos;
    enum MacroType mtype;
    size_t len;

    code->type = FORTRAN_CODE;
    code->lineno = tf->lineno;

    // read lines until a recognized end sequence appears
    while (tf->read_pos < tf->file_end) {
        // look for end sequence
        save_pos = tf->read_pos;
        tok = next_token(tf, &len);
        assert(tok != NULL);
        if (is_test_token(tok, len) ||
            (same_token("end", 3, tok, len) && next_is_test_end_token(tf))) {
            break;
        } else if (tf->read_pos == tf->file_end) {
            break;
        } else { // not found, this is fortran
            tf->read_pos = save_pos;
        }

        if (find_macro(tf, &mtype, need_array_it)) {
            //  record initial fortran code
            code->u.c.str = start;
            code->u.c.len = tf->read_pos - start;
            // parse the macro
            tf->read_pos = tf->next_pos;
            code->next = parse_macro(tf, mtype, need_array_it);
            // parse_macro recurses to parse more code, so just return here
            if (!code->next)
                goto error;
            return code;
        } else {
            next_line(tf);
        }
    }
    if (tf->line_pos < tf->file_end) { // found non-fortran code
        // record bounds of fortran code
        code->u.c.str = start;
        code->u.c.len = tf->line_pos - start;
        tf->next_pos = tf->read_pos = tf->line_pos;
        return code;
    }
    syntax_error(tf);
 error:
    free_code(code);
    return NULL;
}

static int expect_eol(struct TestFile *tf)
{
    char *tok = next_token(tf, NULL);
    assert(tok != NULL);
    if (tok != END_OF_LINE) {
        syntax_error(tf);
        return -1;
    }
    if (!next_line(tf)) {
        fail(tf, tf->read_pos, "expected a newline");
        return -1;
    }
    return 0;
}

static int
expect_token(struct TestFile *tf, const char *expected, const char *message)
{
    char *tok;
    size_t len;

    tok = next_token(tf, &len);
    assert(tok != NULL);
    if (tok == END_OF_LINE ||
        !same_token(expected, strlen(expected),tok, len)) {
        vfail(tf, tf->read_pos, "\"%s\" expected", message);
        return -1;
    }
    return 0;
}

static char *expect_name(struct TestFile *tf, size_t *len, const char *kind)
{
    char *name = next_name(tf, len);
    if (!name || name == END_OF_LINE) {
        vfail(tf, tf->read_pos, "expected %s name", kind);
        return NULL;
    }
    return name;
}

static int parse_end_sequence(struct TestFile *tf, const char *kind,
                              const char *name, size_t name_len)
{
    char message[32] = "end ";
    char *tok;
    size_t len;

    strncpy(message + 4, kind, sizeof(message) - 4);
    if (expect_token(tf, "end", message))
        return -1;
    // expect kind
    tok = next_token(tf, &len);
    assert(tok != NULL);
    if (tok == END_OF_LINE) {
        syntax_error(tf);
        return -1;
    }
    if (!same_token(kind, strlen(kind), tok, len)) {
        vfail(tf, tf->read_pos, "expected \"end %s\"", kind);
        return -1;
    }
    // check end name if given and present
    if (name) {
        tok = next_name(tf, &len);
        assert(tok != NULL);
        if (tok != END_OF_LINE && !same_token(name, name_len, tok, len)) {
            vfail(tf, tf->read_pos, "mismatched %s name", kind);
            return -1;
        }
    }
    return expect_eol(tf);
}

static struct TestDependency *parse_dependency(struct TestFile *tf)
{
    struct TestDependency *dep = NEW0(struct TestDependency);

    dep->filename = next_quoted_string(tf, &dep->len);
    if (!dep->filename) {
        free(dep);
        return NULL;
    }
    if (expect_eol(tf)) {
        free(dep);
        return NULL;
    }
    return dep;
}

static struct TestModule *parse_module(struct TestFile *tf)
{
    struct TestModule *mod = NEW0(struct TestModule);

    mod->name = next_token(tf, &mod->len);
    if (mod->name == END_OF_LINE) {
        fail(tf, tf->read_pos, "expected a module name");
        free(mod);
        return NULL;
    }

    // there might be Fortran code after the use <mod name>
    if (tf->next_pos < tf->next_line_pos) {
        tf->read_pos = tf->next_pos;
        tf->next_pos = tf->next_line_pos;
        mod->extra = tf->read_pos;
        mod->elen = tf->next_pos - tf->read_pos;
    }

    if (expect_eol(tf)) {
        free(mod);
        return NULL;
    }
    return mod;
}

static struct Code *parse_support(struct TestFile *tf, const char *kind)
{
    struct Code *code = NULL;

    if (expect_eol(tf))
        goto err;

    code = parse_fortran(tf, NULL);
    if (!code)
        goto err;

    if (parse_end_sequence(tf, kind, NULL, 0))
        goto err;

    return code;
 err:
    if (code)
        free_code(code);
    return NULL;
}

static struct TestCase *parse_test_case(struct TestFile *tf)
{
    struct TestCase *test = NEW0(struct TestCase);

    test->name = expect_name(tf, &test->name_len, "test");
    if (!test->name)
        goto err;
    if (memchr(test->name, '"', test->name_len)) {
        fail(tf, tf->read_pos, "double quotes (\") not allowed in test names");
        goto err;
    }

    if (expect_eol(tf))
        goto err;

    test->need_array_iterator = 0;
    test->code = parse_fortran(tf, &test->need_array_iterator);
    if (!test->code)
        goto err;

    if (parse_end_sequence(tf, "test", test->name, test->name_len))
        goto err;

    return test;
 err:
    free_cases(test);
    return NULL;
}

static struct TestSet *parse_set(struct TestFile *tf)
{
    struct TestSet *set = NEW0(struct TestSet);
    char *tok;
    size_t len;

    assert(tf->next_pos);
    assert(tf->next_pos > tf->read_pos);

    set->name = expect_name(tf, &set->name_len, "set");
    if (!set->name)
        goto err;

    if (expect_eol(tf))
        goto err;

    // set contents: dep, tolerance, setup, teardown, test case, fortran
    set->tolerance = 0.0; // XXX magic number == BAD
    for (;;) {
        tok = next_token(tf, &len);
        if (tok == END_OF_LINE) {
            if (!next_line(tf)) {
                syntax_error(tf);
                goto err; // EOF
            }
        } else if (tok) {
            if (same_token("dep", 3, tok, len)) {
                struct TestDependency *dep = parse_dependency(tf);
                if (!dep)
                    goto err;
                dep->next = set->deps;
                set->deps = dep;
                set->n_deps++;
            } else if (same_token("use", 3, tok, len)) {
                struct TestModule *mod = parse_module(tf);
                if (!mod)
                    goto err;

                mod->next = set->mods;
                set->mods = mod;
                set->n_mods++;
            } else if (same_token("tolerance", 9, tok, len)) {
                char *tolend;
                tok = next_token(tf, &len);
                if (!tok || tok == END_OF_LINE) {
                    fail(tf, tf->read_pos, "expected tolerance value");
                    goto err;
                }
                set->tolerance = strtod(tf->read_pos, &tolend);
                if (tolend == tf->read_pos || tolend != tf->next_pos) {
                    fail(tf, tf->read_pos, "not a floating point value");
                    goto err;
                }
                tf->next_pos = tolend;
            } else if (same_token("setup", 5, tok, len)) {
                if (set->setup) {
                    fail(tf, tf->next_pos,
                         "more than one setup case specified");
                    goto err;
                }
                set->setup = parse_support(tf, "setup");
                if (!set->setup)
                    goto err;
            } else if (same_token("teardown", 8, tok, len)) {
                if (set->teardown) {
                    fail(tf, tf->next_pos,
                         "more than one teardown case specified");
                    goto err;
                }
                set->teardown = parse_support(tf, "teardown");
                if (!set->teardown)
                    goto err;
            } else if (same_token("test", 4, tok, len)) {
                struct TestCase *test = parse_test_case(tf);
                if (!test)
                    goto err;
                test->next = set->tests;
                set->tests = test;
                set->n_tests++;
            } else if (same_token("end", 3, tok, len)) {
                tf->next_pos = tf->read_pos;
                break; // end of test set
            } else { // fortran code
                struct Code *code;
                tf->next_pos = tf->read_pos = tf->line_pos;
                code = parse_fortran(tf, NULL);
                // XXX check for parse fail?
                code->next = set->code;
                set->code = code;
            }
        } else { // EOF
            fail(tf, tf->read_pos, "expected end set");
            goto err;
        }
    }

    // end set name
    if (parse_end_sequence(tf, "set", set->name, set->name_len))
        goto err;

    return set;
 err:
    free_sets(set);
    return NULL;
}

/* Parser entry point.  Opens and parses the test sets in the given file.
 */
struct TestFile *parse_test_file(const char *path)
{
    struct TestFile *tf = NEW0(struct TestFile);

    tf->fd = open(path, O_RDONLY);
    if (tf->fd == -1) {
        fprintf(stderr, "Opening file %s: %s\n", path, strerror(errno));
        return NULL;
    }
    tf->path = path;

    if (fstat(tf->fd, &tf->statbuf)) {
        fprintf(stderr, "Stat file %s: %s\n", path, strerror(errno));
        goto close_it;
    }
    if (tf->statbuf.st_size < 1) {
        fprintf(stderr, "File %s is empty!\n", path);
        goto close_it;
    }

    tf->file_buf =
        (char *)mmap(NULL, tf->statbuf.st_size, PROT_READ, MAP_SHARED, tf->fd, 0);
    if (!tf->file_buf) {
        fprintf(stderr, "Mapping file %s: %s\n", path, strerror(errno));
        goto close_it;
    }
    tf->file_end = tf->file_buf + tf->statbuf.st_size;

    if (!next_line(tf)) {
        syntax_error(tf);
        goto close_it;
    }

    for (;;) {
        size_t toklen;
        char *token = next_token(tf, &toklen);
        if (same_token("set", 3, token, toklen)) {
            struct TestSet *set = parse_set(tf);
            if (!set) // parse failure already reported
                goto close_it;
            set->next = tf->sets;
            tf->sets = set;
        } else if (token == END_OF_LINE) {
            if (!next_line(tf)) { // EOF
                if (!tf->sets) {
                    goto no_sets;
                }
                goto done;
            }
            // else swallow blank line
        } else {
no_sets:
            fail(tf, tf->read_pos, "expected a test set");
            goto close_it;
        }
    }

 close_it:
    if (tf->sets) {
        free_sets(tf->sets);
        tf->sets = NULL;
    }
    close_testfile(tf);
 done:
    return tf;
}
