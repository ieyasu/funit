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

static struct Code *parse_fortran(struct ParseState *, int *);

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

    close_parse_file(&tf->ps);
}

static void token_end_finder(struct ParseState *ps)
{
    while (ps->next_pos < ps->next_line_pos) {
        switch (*ps->next_pos) {
        case ' ':
        case '\t':
        case '!':
        case '\r':
        case '\n':
        case ',':
            return;
        }
        ps->next_pos++;
    }
}

static char *next_token(struct ParseState *ps, size_t *len)
{
    return next_thing(ps, len, token_end_finder);
}

static void name_end_finder(struct ParseState *ps)
{
    while (ps->next_pos < ps->next_line_pos) {
        switch (*ps->next_pos) {
        case '!':
        case '\r':
        case '\n':
            return;
        default:
            break;
        }
        ps->next_pos++;
    }
}

static char *next_name(struct ParseState *ps, size_t *len)
{
    return next_thing(ps, len, name_end_finder);
}

static void quoted_string_end_finder(struct ParseState *ps)
{
    if (*ps->next_pos != '"')
        return;
    ps->next_pos++;
    while (ps->next_pos < ps->next_line_pos) {
        switch (*ps->next_pos) {
        case '"':
            ps->next_pos++;
            return;
        case '!':
        case '\r':
        case '\n':
            return;
        default:
            ps->next_pos++;
            break;
        }
    }
}

static char *next_quoted_string(struct ParseState *ps, size_t *len)
{
    char *s = next_thing(ps, len, quoted_string_end_finder);
    assert(s != NULL);
    if (s == END_OF_LINE || *s != '"') {
        parse_fail(ps, s, "expected a quote (\") to begin a string");
        return NULL;
    }

    if (ps->next_pos == s || *(ps->next_pos - 1) != '"') {
        parse_fail(ps, ps->next_pos, "expected a quote(\") to end the string");
        return NULL;
    }

    ps->read_pos++; // skip past leading "
    *len = ps->next_pos - ps->read_pos - 1;
    return ps->read_pos;
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

static char *skip_line_continuation(struct ParseState *ps, int in_string)
{
    assert(*ps->next_pos == '&');
    ps->next_pos++;

    // expect ' ' or '\t' only until next_line_pos
    skip_next_ws(ps);
    if (ps->next_pos < ps->next_line_pos) {
        parse_fail(ps, ps->next_pos, "expected newline after '&'");
        return NULL;
    }
    while (ps->next_pos < ps->file_end) {
        char *save_read_pos = ps->read_pos;
        if (!next_line(ps))
            break;
        ps->read_pos = save_read_pos;
        skip_next_ws(ps);
        switch (*ps->next_pos) {
        case '!': // skip comment lines
            break;
        case '&':
            if (in_string)
                ps->next_pos++; // skip past whitespace truncator
            // FALLTHROUGH
        default:
            return ps->next_pos;
        }
    }
    syntax_error(ps);
    return NULL;
}

static char *skip_ampersand_in_string(struct ParseState *ps)
{
    char *amp_pos = ps->next_pos;

    assert(*ps->next_pos == '&');
    ps->next_pos++;

    skip_next_ws(ps);
    if (ps->next_pos < ps->next_line_pos &&
        *ps->next_pos != '\r' && *ps->next_pos != '\n') {
        // not a line continuation
        ps->next_pos--;
        return ps->next_pos;
    }
    ps->next_pos = amp_pos;
    return skip_line_continuation(ps, 1);
}

/* Look for the next ',' or un-balanced ')'.  Returns a pointer to that
 * ',' or ')' or NULL if not found before end of file.  This may continue
 * to the next line.
 */
static char *split_macro_arg(struct ParseState *ps)
{
    int paren_count = 0, in_string = 0;
    char string_delim;

    ps->next_pos = ps->read_pos;
    while (ps->next_pos < ps->next_line_pos) {
        switch (*ps->next_pos) {
        case '\'':
        case '"': // open or close a string
            if (in_string) {
                if (*ps->next_pos == string_delim) {
                    if (*(ps->next_pos + 1) == string_delim) {
                        ps->next_pos++; // doubled to escape
                    } else {
                        in_string = 0; // close quote
                    }
                }
            } else {
                in_string = 1;
                string_delim = *ps->next_pos;
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
                    return ps->next_pos; // closing paren or separating comma
                } else {
                    paren_count--;
                }
            }
            break;
        case ',':
            if (!in_string && paren_count == 0) {
                return ps->next_pos; // closing paren or separating comma
            }
            break;
        case '&': // line continuation
            if (in_string) {
                if (!skip_ampersand_in_string(ps))
                    return NULL;
            } else { // line continuation
                if (!skip_line_continuation(ps, 0))
                    return NULL;
            }
            break;
        case '\r':
        case '\n':
            parse_fail(ps, ps->next_pos, "unexpected end of line");
            return NULL;
        default:
            break;
        }
        assert(paren_count >= 0);
        ps->next_pos++;
    }
    return NULL;
}

static struct Code *parse_macro_args(struct ParseState *ps, int *error)
{
    struct Code *code;

    if (!split_macro_arg(ps)) {
        *error = 1;
        return NULL;
    }

    code = NEW0(struct Code);
    code->type = ARG_CODE;
    code->lineno = ps->lineno;
    code->u.c.str = ps->read_pos;
    code->u.c.len = ps->next_pos - ps->read_pos;
    if (*ps->next_pos == ',') {
        ps->next_pos++;
        skip_next_ws(ps);
        ps->read_pos = ps->next_pos;
        code->next = parse_macro_args(ps, error);
        if (*error) {
            free(code);
            return NULL;
        }
    }
    assert(*ps->next_pos == ')');
    return code;
}

static struct Code *
parse_macro(struct ParseState *ps, enum MacroType mtype, int *need_array_it)
{
    struct Code *code = NEW0(struct Code);
    int error = 0;

    code->type = MACRO_CODE;
    code->lineno = ps->lineno;
    code->u.m.type = mtype;
    code->u.m.args = parse_macro_args(ps, &error);
    if (error)
        goto cleanup;
    if (code->u.m.args) {
        ps->read_pos = ps->next_pos + 1;
        code->next = parse_fortran(ps, need_array_it);
        if (code->next)
            return code;
    }
    parse_fail(ps, ps->read_pos, "expected macro argument");
 cleanup:
    free_code(code);
    return NULL;
}

/* Scan code for an assert macro, returning the start of the macro if found or
 * NULL if not. The out param type returns the assertion type, and afterp is
 * set to point immediately after the macro's opening paren.
 */
static char *
find_macro(struct ParseState *ps, enum MacroType *type, int *need_array_it)
{
    size_t len = ps->next_line_pos - ps->read_pos;
    char *assert_pos;

    assert_pos = strncasestr(ps->read_pos, len, "assert_", 7);
    if (assert_pos) {
        if (!need_array_it) {
            parse_fail(ps, assert_pos, "assertions not allowed here");
        }
        char *s = assert_pos + 7;
        size_t rest_len = ps->next_line_pos - s;
        if (rest_len > 6 && !strncasecmp(s, "array_", 6)) { // accept array_
            s += 6;
            rest_len -= 6;
            if (rest_len > 10 && !strncasecmp(s, "equal_with", 10)) {
                *need_array_it = 1;
                ps->next_pos = s + 10;
                *type = ASSERT_ARRAY_EQUAL_WITH;
            } else if (rest_len > 5 && !strncasecmp(s, "equal", 5)) {
                *need_array_it = 1;
                ps->next_pos = s + 5;
                *type = ASSERT_ARRAY_EQUAL;
            } else {
                assert_pos = NULL; // not an assert macro
            }
        } else if (rest_len >  4 && !strncasecmp(s, "true",        4)) {
            ps->next_pos = s + 4;
            *type = ASSERT_TRUE;
        } else if (rest_len >  5 && !strncasecmp(s, "false",       5)) {
            ps->next_pos = s + 5;
            *type = ASSERT_FALSE;
        } else if (rest_len > 10 && !strncasecmp(s, "equal_with", 10)) {
            ps->next_pos = s + 10;
            *type = ASSERT_EQUAL_WITH;
        } else if (rest_len >  5 && !strncasecmp(s, "equal",       5)) {
            ps->next_pos = s + 5;
            *type = ASSERT_EQUAL;
        } else if (rest_len >  9 && !strncasecmp(s, "not_equal",   9)) {
            ps->next_pos = s + 9;
            *type = ASSERT_NOT_EQUAL;
        } else {
            assert_pos = NULL; // not an assert macro
        }
    } else {
        assert_pos = strncasestr(ps->read_pos, len, "flunk",  5);
        if (assert_pos) {
            ps->next_pos = assert_pos + 5;
            *type = FLUNK;
        } else {
            assert_pos = NULL; // no assertions found
        }
    }

    if (assert_pos) {
        // make sure not commented out
        char *s = assert_pos;
        while (--s >= ps->line_pos) {
            if (*s == '!')
                return NULL;
        }

        // find open paren
        while (ps->next_pos < ps->next_line_pos) {
            switch (*ps->next_pos) {
            case ' ':
            case '\t':
                ps->next_pos++;
                break;
            case '(':
                ps->next_pos++;
                ps->read_pos = assert_pos;
                return assert_pos;
            default:
                parse_fail(ps, ps->next_pos, "expected '('");
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
static int next_is_test_end_token(struct ParseState *ps)
{
    char *tok;
    size_t len;

    tok = next_token(ps, &len);
    assert(tok != NULL);

    return (tok != END_OF_LINE &&
            (same_token(tok, len, "test",        4) ||
             same_token(tok, len, "setup",       5) ||
             same_token(tok, len, "teardown",    8) ||
             same_token(tok, len, "set", 3)));
}

static struct Code *parse_fortran(struct ParseState *ps, int *need_array_it)
{
    struct Code *code = NEW0(struct Code);
    char *start = ps->read_pos, *tok, *save_pos;
    enum MacroType mtype;
    size_t len;

    code->type = FORTRAN_CODE;
    code->lineno = ps->lineno;

    // read lines until a recognized end sequence appears
    while (ps->read_pos < ps->file_end) {
        // look for end sequence
        save_pos = ps->read_pos;
        tok = next_token(ps, &len);
        assert(tok != NULL);
        if (is_test_token(tok, len) ||
            (same_token("end", 3, tok, len) && next_is_test_end_token(ps))) {
            break;
        } else if (ps->read_pos == ps->file_end) {
            break;
        } else { // not found, this is fortran
            ps->read_pos = save_pos;
        }

        if (find_macro(ps, &mtype, need_array_it)) {
            //  record initial fortran code
            code->u.c.str = start;
            code->u.c.len = ps->read_pos - start;
            // parse the macro
            ps->read_pos = ps->next_pos;
            code->next = parse_macro(ps, mtype, need_array_it);
            // parse_macro recurses to parse more code, so just return here
            if (!code->next)
                goto error;
            return code;
        } else {
            next_line(ps);
        }
    }
    if (ps->line_pos < ps->file_end) { // found non-fortran code
        // record bounds of fortran code
        code->u.c.str = start;
        code->u.c.len = ps->line_pos - start;
        ps->next_pos = ps->read_pos = ps->line_pos;
        return code;
    }
    syntax_error(ps);
 error:
    free_code(code);
    return NULL;
}

static int expect_eol2(struct ParseState *ps, int need_nl)
{
    char *tok = next_token(ps, NULL);
    assert(tok != NULL);
    if (tok != END_OF_LINE) {
        syntax_error(ps);
        return -1;
    }
    if (!next_line(ps) && need_nl) {
        parse_fail(ps, ps->read_pos, "expected a newline");
        return -1;
    }
    return 0;
}

static int expect_eol(struct ParseState *ps)
{
    return expect_eol2(ps, TRUE);
}

static int
expect_token(struct ParseState *ps, const char *expected, const char *message)
{
    char *tok;
    size_t len;

    tok = next_token(ps, &len);
    assert(tok != NULL);
    if (tok == END_OF_LINE ||
        !same_token(expected, strlen(expected),tok, len)) {
        parse_vfail(ps, ps->read_pos, "\"%s\" expected", message);
        return -1;
    }
    return 0;
}

static char *expect_name(struct ParseState *ps, size_t *len, const char *kind)
{
    char *name = next_name(ps, len);
    if (!name || name == END_OF_LINE) {
        parse_vfail(ps, ps->read_pos, "expected %s name", kind);
        return NULL;
    }
    return name;
}

static int parse_end_sequence(struct ParseState *ps, const char *kind,
                              const char *name, size_t name_len)
{
    char message[32] = "end ";
    char *tok;
    size_t len;

    strncpy(message + 4, kind, sizeof(message) - 4);
    if (expect_token(ps, "end", message))
        return -1;
    // expect kind
    tok = next_token(ps, &len);
    assert(tok != NULL);
    if (tok == END_OF_LINE) {
        syntax_error(ps);
        return -1;
    }
    if (!same_token(kind, strlen(kind), tok, len)) {
        parse_vfail(ps, ps->read_pos, "expected \"end %s\"", kind);
        return -1;
    }
    // check end name if given and present
    if (name) {
        tok = next_name(ps, &len);
        assert(tok != NULL);
        if (tok != END_OF_LINE && !same_token(name, name_len, tok, len)) {
            parse_vfail(ps, ps->read_pos, "mismatched %s name", kind);
            return -1;
        }
    }

    int need_nl = strcmp(kind, "set") != 0;
    return expect_eol2(ps, need_nl);
}

static struct TestDependency *parse_dependency(struct ParseState *ps)
{
    struct TestDependency *dep = NEW0(struct TestDependency);

    dep->filename = next_quoted_string(ps, &dep->len);
    if (!dep->filename) {
        free(dep);
        return NULL;
    }
    if (expect_eol(ps)) {
        free(dep);
        return NULL;
    }
    return dep;
}

static struct TestModule *parse_module(struct ParseState *ps)
{
    struct TestModule *mod = NEW0(struct TestModule);

    mod->name = next_token(ps, &mod->len);
    if (mod->name == END_OF_LINE) {
        parse_fail(ps, ps->read_pos, "expected a module name");
        free(mod);
        return NULL;
    }

    // there might be Fortran code after the use <mod name>
    if (ps->next_pos < ps->next_line_pos) {
        ps->read_pos = ps->next_pos;
        ps->next_pos = ps->next_line_pos;
        mod->extra = ps->read_pos;
        mod->elen = ps->next_pos - ps->read_pos;
    }

    if (expect_eol(ps)) {
        free(mod);
        return NULL;
    }
    return mod;
}

static struct Code *parse_support(struct ParseState *ps, const char *kind)
{
    struct Code *code = NULL;

    if (expect_eol(ps))
        goto err;

    code = parse_fortran(ps, NULL);
    if (!code)
        goto err;

    if (parse_end_sequence(ps, kind, NULL, 0))
        goto err;

    return code;
 err:
    if (code)
        free_code(code);
    return NULL;
}

static struct TestCase *parse_test_case(struct ParseState *ps)
{
    struct TestCase *test = NEW0(struct TestCase);

    test->name = expect_name(ps, &test->name_len, "test");
    if (!test->name)
        goto err;
    if (memchr(test->name, '"', test->name_len)) {
        parse_fail(ps, ps->read_pos, "double quotes (\") not allowed in test names");
        goto err;
    }

    if (expect_eol(ps))
        goto err;

    test->need_array_iterator = 0;
    test->code = parse_fortran(ps, &test->need_array_iterator);
    if (!test->code)
        goto err;

    if (parse_end_sequence(ps, "test", test->name, test->name_len))
        goto err;

    return test;
 err:
    free_cases(test);
    return NULL;
}

static struct TestSet *parse_set(struct ParseState *ps)
{
    struct TestSet *set = NEW0(struct TestSet);
    char *tok;
    size_t len;

    assert(ps->next_pos);
    assert(ps->next_pos > ps->read_pos);

    set->name = expect_name(ps, &set->name_len, "set");
    if (!set->name)
        goto err;

    if (expect_eol(ps))
        goto err;

    // set contents: dep, tolerance, setup, teardown, test case, fortran
    set->tolerance = 0.0; // XXX magic number == BAD
    for (;;) {
        tok = next_token(ps, &len);
        if (tok == END_OF_LINE) {
            if (!next_line(ps)) {
                syntax_error(ps);
                goto err; // EOF
            }
        } else if (tok) {
            if (same_token("dep", 3, tok, len)) {
                struct TestDependency *dep = parse_dependency(ps);
                if (!dep)
                    goto err;
                dep->next = set->deps;
                set->deps = dep;
                set->n_deps++;
            } else if (same_token("use", 3, tok, len)) {
                struct TestModule *mod = parse_module(ps);
                if (!mod)
                    goto err;

                mod->next = set->mods;
                set->mods = mod;
                set->n_mods++;
            } else if (same_token("tolerance", 9, tok, len)) {
                char *tolend;
                tok = next_token(ps, &len);
                if (!tok || tok == END_OF_LINE) {
                    parse_fail(ps, ps->read_pos, "expected tolerance value");
                    goto err;
                }
                set->tolerance = strtod(ps->read_pos, &tolend);
                if (tolend == ps->read_pos || tolend != ps->next_pos) {
                    parse_fail(ps, ps->read_pos, "not a floating point value");
                    goto err;
                }
                ps->next_pos = tolend;
            } else if (same_token("setup", 5, tok, len)) {
                if (set->setup) {
                    parse_fail(ps, ps->next_pos,
                         "more than one setup case specified");
                    goto err;
                }
                set->setup = parse_support(ps, "setup");
                if (!set->setup)
                    goto err;
            } else if (same_token("teardown", 8, tok, len)) {
                if (set->teardown) {
                    parse_fail(ps, ps->next_pos,
                         "more than one teardown case specified");
                    goto err;
                }
                set->teardown = parse_support(ps, "teardown");
                if (!set->teardown)
                    goto err;
            } else if (same_token("test", 4, tok, len)) {
                struct TestCase *test = parse_test_case(ps);
                if (!test)
                    goto err;
                test->next = set->tests;
                set->tests = test;
                set->n_tests++;
            } else if (same_token("end", 3, tok, len)) {
                ps->next_pos = ps->read_pos;
                break; // end of test set
            } else { // fortran code
                struct Code *code;
                ps->next_pos = ps->read_pos = ps->line_pos;
                code = parse_fortran(ps, NULL);
                // XXX check for parse fail?
                code->next = set->code;
                set->code = code;
            }
        } else { // EOF
            parse_fail(ps, ps->read_pos, "expected end set");
            goto err;
        }
    }

    // end set name
    if (parse_end_sequence(ps, "set", set->name, set->name_len))
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
    struct ParseState *ps = &tf->ps;

    if (open_file_for_parsing(path, ps)) {
        free(tf);
        return NULL;
    }

    if (!next_line(ps)) {
        syntax_error(ps);
        goto close_it;
    }

    for (;;) {
        size_t toklen;
        char *token = next_token(ps, &toklen);
        if (same_token("set", 3, token, toklen)) {
            struct TestSet *set = parse_set(ps);
            if (!set) // parse failure already reported
                goto close_it;
            set->next = tf->sets;
            tf->sets = set;
        } else if (token == END_OF_LINE) {
            if (!next_line(ps)) { // EOF
                if (!tf->sets) {
                    goto no_sets;
                }
                goto done;
            }
            // else swallow blank line
        } else {
no_sets:
            parse_fail(ps, ps->read_pos, "expected a test set");
            goto close_it;
        }
    }

 close_it:
    close_testfile(tf);
 done:
    return tf;
}
