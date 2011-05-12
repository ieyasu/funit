/* parse_suite.c - parse a test suite file like the one below:

    test_suite suite name
      dep "../file1.F90"
      dep "../file2.F90"
    
      use a_module
    
      tolerance 0.00001

      setup
        ! fortran code to run before each test
      end setup
    
      teardown
        ! fortran code to run after each test
      end teardown
    
      test case1
        ...
        assert_equal(a, 7)
        ...
      end test case1
    end test_suite
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


const char *test_suite_file_name = NULL;

static int fd = 0;
static struct stat statbuf;
static char *file_buf = NULL, *file_end = NULL;
static char *line_pos = NULL, *next_line_pos = NULL;
static char *read_pos = NULL, *next_pos = NULL;
static long lineno = 0;
static int need_array_iterator = 0;


static struct Code *parse_fortran(void);

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

static void free_tests(struct TestRoutine *routine)
{
    if (routine->next)
        free_tests(routine->next);
    if (routine->code)
        free_code(routine->code);
    free(routine);
}

static void free_deps(struct TestDependency *deps)
{
    if (deps->next)
        free_deps(deps->next);
    free(deps);
}

static void free_suites(struct TestSuite *suites)
{
    if (suites->next)
        free_suites(suites->next);
    if (suites->deps)
        free_deps(suites->deps);
    if (suites->setup)
        free_code(suites->setup);
    if (suites->teardown)
        free_code(suites->teardown);
    if (suites->tests)
        free_tests(suites->tests);
    if (suites->code)
        free_code(suites->code);
    free(suites);
}

/* Call when done with the test file and all data structures associated
 * with it.  Frees the TestSuite list returned by parse_suite_file().
 */
void close_suite_and_free(struct TestSuite *suites)
{
    if (file_buf && !munmap(file_buf, statbuf.st_size)) {
        file_buf = NULL;
    } else {
        fprintf(stderr, "Unmapping file %s: %s\n", test_suite_file_name,
                strerror(errno));
        abort();
    }

    if (fd && !close(fd)) {
        fd = 0;
        test_suite_file_name = NULL;
    } else {
        fprintf(stderr, "Closing file %s: %s\n", test_suite_file_name,
                strerror(errno));
        abort();
    }

    free_suites(suites);
}

static void fail(const char *col, const char *message)
{
    assert(line_pos != NULL);

    // file:line:
    assert(test_suite_file_name != NULL);
    fprintf(stderr, "%s:%li:\n\n", test_suite_file_name, lineno);
    // code
    fwrite(line_pos, next_line_pos - line_pos, 1, stderr);
    fputs("\n", stderr);
    // column indicator
    if (col) {
        int n = col - line_pos;
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

static void vfail(const char *col, const char *format, ...)
{
    char message[256];
    va_list ap;

    va_start(ap, format);
    vsnprintf(message, sizeof(message), format, ap);
    va_end(ap);

    fail(col, message);
}

static void syntax_error(void)
{
    fail(read_pos, "syntax error");
}

/* Returns a pointer to the next line of data, or NULL if at last line.
 * Does not handle comments.
 */
static char *next_line(void)
{
    char *prev_line;

    assert(file_buf != NULL);

    if (line_pos) {
        assert(next_line_pos >= line_pos || line_pos == file_end);
        prev_line = line_pos;
        line_pos = next_line_pos;
        // read past \r\n
        while (line_pos < file_end) {
            if (*line_pos == '\n') {
                line_pos++;
                lineno++;
                break;
            } else if (*line_pos == '\r') {
                if (line_pos + 1 < file_end && *(line_pos + 1) == '\n')
                    line_pos += 2;
                else
                    line_pos++;
                lineno++;
                break;
            }
            line_pos++;
        }
        if (line_pos >= file_end) {
            line_pos = prev_line;
            lineno--;
            read_pos = file_end;
            return NULL;
        }
    } else { // very first line
        line_pos = file_buf;
        lineno = 1;
    }
    read_pos = next_pos = line_pos;
    // find next \r\n
    next_line_pos = line_pos;
    while (next_line_pos < file_end) {
        switch (*next_line_pos) {
        case '\n':
        case '\r':
            goto eol;
        default:
            next_line_pos++;
            break;
        }
    }
 eol:
    return line_pos;
}

static char *skip_ws(void)
{
    assert(read_pos != NULL);

    while (read_pos < next_line_pos) {
        switch (*read_pos) {
        case ' ':
        case '\t':
            read_pos++;
            break;
        default:
            goto eol;
        }
    }
 eol:
    return read_pos;
}

static char *skip_next_ws(void)
{
    assert(next_pos != NULL);

    while (next_pos < next_line_pos) {
        switch (*next_pos) {
        case ' ':
        case '\t':
            next_pos++;
            break;
        default:
            goto eol;
        }
    }
 eol:
    return next_pos;
}

typedef void end_finder_fun(void);

static char *next_thing(size_t *len, end_finder_fun end_fun)
{
    read_pos = next_pos;
    skip_ws();

    next_pos = read_pos;
    end_fun();

    if (next_pos - read_pos > 0) {
        if (len)
            *len = next_pos - read_pos;
        return read_pos;
    }
    return (char *)END_OF_LINE;
}

static void token_end_finder(void)
{
    while (next_pos < next_line_pos) {
        switch (*next_pos) {
        case ' ':
        case '\t':
        case '!':
        case '\r':
        case '\n':
            return;
        default:
            next_pos++;
            break;
        }
    }
}

static char *next_token(size_t *len)
{
    return next_thing(len, token_end_finder);
}

static void name_end_finder(void)
{
    while (next_pos < next_line_pos) {
        switch (*next_pos) {
        case '!':
        case '\r':
        case '\n':
            return;
        default:
            next_pos++;
            break;
        }
    }
}

static char *next_name(size_t *len)
{
    return next_thing(len, name_end_finder);
}

static void quoted_string_end_finder(void)
{
    if (*next_pos != '"')
        return;
    next_pos++;
    while (next_pos < next_line_pos) {
        switch (*next_pos) {
        case '"':
            next_pos++;
            return;
        case '!':
        case '\r':
        case '\n':
            return;
        default:
            next_pos++;
            break;
        }
    }
}

static char *next_quoted_string(size_t *len)
{
    size_t full_len;
    char *s;

    s = next_thing(&full_len, quoted_string_end_finder);

    assert(s != NULL);
    if (s == END_OF_LINE || *s != '"') {
        fail(s, "expected a quote (\") to begin a string");
        return NULL;
    }

    if (next_pos == s || *(next_pos - 1) != '"') {
        fail(next_pos, "expected a quote(\") to end the string");
        return NULL;
    }

    read_pos++; // skip past leading "
    if (len)
        *len = next_pos - read_pos - 1;
    return read_pos;
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

static char *skip_line_continuation(int in_string)
{
    assert(*next_pos == '&');
    next_pos++;

    // expect ' ' or '\t' only until next_line_pos
    skip_next_ws();
    if (next_pos < next_line_pos) {
        fail(next_pos, "expected newline after '&'");
        return NULL;
    }
    while (next_pos < file_end) {
        char *save_read_pos = read_pos;
        if (!next_line())
            break;
        read_pos = save_read_pos;
        skip_next_ws();
        switch (*next_pos) {
        case '!': // skip comment lines
            break;
        case '&':
            if (in_string)
                next_pos++; // skip past whitespace truncator
            // FALLTHROUGH
        default:
            return next_pos;
        }
    }
    syntax_error();
    return NULL;
}

static char *skip_ampersand_in_string(void)
{
    char *amp_pos = next_pos;

    assert(*next_pos == '&');
    next_pos++;

    skip_next_ws();
    if (next_pos < next_line_pos && *next_pos != '\r' && *next_pos != '\n') {
        // not a line continuation
        next_pos--;
        return next_pos;
    }
    next_pos = amp_pos;
    return skip_line_continuation(1);
}

/* Look for the next ',' or un-balanced ')'.  Returns a pointer to that
 * ',' or ')' or NULL if not found before end of file.  This may continue
 * to the next line.
 */
static char *split_macro_arg(void)
{
    int paren_count = 0, in_string = 0;
    char string_delim;

    next_pos = read_pos;
    while (next_pos < next_line_pos) {
        switch (*next_pos) {
        case '\'':
        case '"':
            if (in_string) {
                if (*next_pos == string_delim) {
                    if (*(next_pos + 1) == string_delim)
                        next_pos++; // doubled to escape
                    else
                        in_string = 0; // close quote
                }
            } else {
                in_string = 1;
                string_delim = *next_pos;
            }
            break;
        case '(':
            if (!in_string)
                paren_count++;
            break;
        case ')':
        case ',':
            if (!in_string) {
                if (paren_count == 0)
                    return next_pos; // closing paren or separating comma
                else
                    paren_count--;
            }
            break;
        case '&':
            if (in_string) {
                if (!skip_ampersand_in_string())
                    return NULL;
            } else { // line continuation
                if (!skip_line_continuation(0))
                    return NULL;
            }
            break;
        case '\r':
        case '\n':
            fail(next_pos, "unexpected end of line");
            return NULL;
        default:
            break;
        }
        assert(paren_count >= 0);
        next_pos++;
    }
    return NULL;
}

static struct Code *parse_macro_args(int *error)
{
    struct Code *code;

    if (!split_macro_arg()) {
        *error = 1;
        return NULL;
    }

    code = NEW0(struct Code);
    code->type = ARG_CODE;
    code->lineno = lineno;
    code->u.c.str = read_pos;
    code->u.c.len = next_pos - read_pos;
    read_pos = next_pos + 1;
    if (*next_pos == ',') {
        next_pos++;
        skip_ws();
        code->next = parse_macro_args(error);
        if (*error) {
            free(code);
            return NULL;
        }
    }
    assert(*next_pos == ')');
    return code;
}

static struct Code *parse_macro(enum MacroType mtype)
{
    struct Code *code = NEW0(struct Code);
    int error = 0;

    code->type = MACRO_CODE;
    code->lineno = lineno;
    code->u.m.type = mtype;
    code->u.m.args = parse_macro_args(&error);
    if (error)
        goto cleanup;
    if (code->u.m.args) {
        read_pos = next_pos + 1;
        code->next = parse_fortran();
        if (code->next)
            return code;
    }
    fail(read_pos, "expected macro argument");
 cleanup:
    free_code(code);
    return NULL;
}

/* Scan code for an assert macro, returning the start of the macro if found or
 * NULL if not. The out param type returns the assertion type, and afterp is
 * set to point immediately after the macro's opening paren.
 */
static char *find_macro(enum MacroType *type)
{
    size_t len = next_line_pos - read_pos;
    char *assert;

    assert = strncasestr(read_pos, len, "assert_", 7);
    if (assert) {
        char *s = assert + 7;
        size_t rest_len = next_line_pos - s;
        if (rest_len > 6 && !strncasecmp(s, "array_", 6)) { // accept _array
            s += 6;
            rest_len -= 6;
            if (rest_len > 10 && !strncasecmp(s, "equal_with", 10)) {
                next_pos = s + 10;
                *type = ASSERT_ARRAY_EQUAL_WITH;
            } else if (rest_len > 5 && !strncasecmp(s, "equal", 5)) {
                next_pos = s + 5;
                *type = ASSERT_ARRAY_EQUAL;
            } else {
                assert = NULL; // not an assert macro
            }
        } else if (rest_len >  4 && !strncasecmp(s, "true",        4)) {
            next_pos = s + 4;
            *type = ASSERT_TRUE;
        } else if (rest_len >  5 && !strncasecmp(s, "false",       5)) {
            next_pos = s + 5;
            *type = ASSERT_FALSE;
        } else if (rest_len > 10 && !strncasecmp(s, "equal_with", 10)) {
            next_pos = s + 10;
            *type = ASSERT_EQUAL_WITH;
        } else if (rest_len >  5 && !strncasecmp(s, "equal",       5)) {
            next_pos = s + 5;
            *type = ASSERT_EQUAL;
        } else if (rest_len >  9 && !strncasecmp(s, "not_equal",   9)) {
            next_pos = s + 9;
            *type = ASSERT_NOT_EQUAL;
        } else {
            assert = NULL; // not an assert macro
        }
    } else {
        assert = strncasestr(read_pos, len, "flunk",  5);
        if (assert) {
            next_pos = assert + 5;
            *type = FLUNK;
        } else {
            assert = NULL; // no assertions found
        }
    }

    if (assert) {
        // make sure not commented out
        char *s = assert;
        while (--s >= line_pos) {
            if (*s == '!')
                return NULL;
        }

        // find open paren
        while (next_pos < next_line_pos) {
            switch (*next_pos) {
            case ' ':
            case '\t':
                next_pos++;
                break;
            case '(':
                next_pos++;
                read_pos = assert;
                return assert;
            default:
                fail(next_pos, "expected '('");
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
static int is_suite_token(char *tok, size_t len)
{
    return (tok != END_OF_LINE &&
            (same_token(tok, len, "test",        4) ||
             same_token(tok, len, "setup",       5) ||
             same_token(tok, len, "teardown",    8) ||
             same_token(tok, len, "dep",         3) ||
             same_token(tok, len, "tolerance",   9)));
}

/* Tokens which follow an "end" token to denote a sequence of non-fortran code.
 */
static int next_is_suite_end_token(void)
{
    char *tok;
    size_t len;

    tok = next_token(&len);
    assert(tok != NULL);

    return (tok != END_OF_LINE &&
            (same_token(tok, len, "test",        4) ||
             same_token(tok, len, "setup",       5) ||
             same_token(tok, len, "teardown",    8) ||
             same_token(tok, len, "test_suite", 10)));
}

static struct Code *parse_fortran(void)
{
    struct Code *code = NEW0(struct Code);
    char *start = read_pos, *tok, *save_pos;
    enum MacroType mtype;
    size_t len;

    code->type = FORTRAN_CODE;
    code->lineno = lineno;

    // read lines until a recognized end sequence appears
    while (read_pos < file_end) {
        // look for end sequence
        save_pos = read_pos;
        tok = next_token(&len);
        assert(tok != NULL);
        if (is_suite_token(tok, len) ||
            (same_token("end", 3, tok, len) && next_is_suite_end_token())) {
            break;
        } else if (read_pos == file_end) {
            break;
        } else { // not found, this is fortran
            read_pos = save_pos;
        }

        if (find_macro(&mtype)) {
            //  record initial fortran code
            code->u.c.str = start;
            code->u.c.len = read_pos - start;
            // parse the macro
            read_pos = next_pos;
            code->next = parse_macro(mtype);
            // parse_macro recurses to parse more code, so just return here
            if (!code->next)
                goto error;
            return code;
        } else {
            next_line();
        }
    }
    if (line_pos < file_end) { // found non-fortran code
        // record bounds of fortran code
        code->u.c.str = start;
        code->u.c.len = line_pos - start;
        next_pos = read_pos = line_pos;
        return code;
    }
    syntax_error();
 error:
    free_code(code);
    return NULL;
}

static int expect_eol(void)
{
    char *tok = next_token(NULL);
    assert(tok != NULL);
    if (tok != END_OF_LINE) {
        syntax_error();
        return -1;
    }
    if (!next_line()) {
        fail(read_pos, "expected a newline");
        return -1;
    }
    return 0;
}

static int expect_token(const char *expected, const char *message)
{
    char *tok;
    size_t len;

    tok = next_token(&len);
    assert(tok != NULL);
    if (tok == END_OF_LINE ||
        !same_token(expected, strlen(expected),tok, len)) {
        vfail(read_pos, "expected to see \"%s\"", message);
        return -1;
    }
    return 0;
}

static char *expect_name(size_t *len, const char *kind)
{
    char *name = next_name(len);
    if (!name || name == END_OF_LINE) {
        vfail(read_pos, "expected %s name", kind);
        return NULL;
    }
    return name;
}

static int
parse_end_sequence(const char *kind, const char *name, size_t name_len)
{
    char message[32] = "end ";
    char *tok;
    size_t len;

    strncpy(message + 4, kind, sizeof(message) - 4);
    if (expect_token("end", message))
        return -1;
     // expect kind
     tok = next_token(&len);
     assert(tok != NULL);
     if (tok == END_OF_LINE) {
         syntax_error();
         return -1;
     }
     if (!same_token(kind, strlen(kind), tok, len)) {
         vfail(read_pos, "expected \"end %s\"", kind);
         return -1;
     }
     // check end name if given and present
     if (name) {
         tok = next_name(&len);
         assert(tok != NULL);
         if (tok != END_OF_LINE && !same_token(name, name_len, tok, len)) {
             vfail(read_pos, "mismatched %s name", kind);
             return -1;
         }
     }
     return expect_eol();
}

static struct TestDependency *parse_dependency(void)
{
    struct TestDependency *dep = NEW0(struct TestDependency);

    dep->filename = next_quoted_string(&dep->len);
    if (!dep->filename) {
        free(dep);
        return NULL;
    }
    if (expect_eol()) {
        free(dep);
        return NULL;
    }
    return dep;
}

static struct TestModule *parse_module(void)
{
    struct TestModule *mod = NEW0(struct TestModule);

    mod->name = next_token(&mod->len);
    if (mod->name == END_OF_LINE) {
        fail(read_pos, "expected a module name");
        free(mod);
        return NULL;
    }
    if (expect_eol()) {
        free(mod);
        return NULL;
    }
    return mod;
}

static struct Code *parse_support(const char *kind)
{
    struct Code *code = NULL;

    if (expect_eol())
        goto err;

    code = parse_fortran();
    if (!code)
        goto err;

    if (parse_end_sequence(kind, NULL, 0))
        goto err;

    return code;
 err:
    if (code)
        free_code(code);
    return NULL;
}

static struct TestRoutine *parse_test(void)
{
    struct TestRoutine *routine = NEW0(struct TestRoutine);

    routine->name = expect_name(&routine->name_len, "test");
    if (!routine->name)
        goto err;
    if (memchr(routine->name, '"', routine->name_len)) {
        fail(read_pos, "double quotes (\") not allowed in test names");
        goto err;
    }

    if (expect_eol())
        goto err;

    need_array_iterator = 0;

    routine->code = parse_fortran();
    if (!routine->code)
        goto err;

    if (parse_end_sequence("test", routine->name, routine->name_len))
        goto err;

    routine->need_array_iterator = need_array_iterator;

    return routine;
 err:
    free_tests(routine);
    return NULL;
}

static struct TestSuite *parse_suite(void)
{
    struct TestSuite *suite = NEW0(struct TestSuite);
    char *tok;
    size_t len;

    assert(next_pos);
    assert(next_pos > read_pos);

    suite->name = expect_name(&suite->name_len, "test suite");
    if (!suite->name)
        goto err;

    if (expect_eol())
        goto err;

    // suite contents: dep, tolerance, setup, teardown, test case, fortran
    suite->tolerance = 0.0;
    for (;;) {
        tok = next_token(&len);
        if (tok == END_OF_LINE) {
            if (!next_line()) {
                syntax_error();
                goto err; // EOF
            }
        } else if (tok) {
            if (same_token("dep", 3, tok, len)) {
                struct TestDependency *dep = parse_dependency();
                if (!dep)
                    goto err;
                dep->next = suite->deps;
                suite->deps = dep;
                suite->n_deps++;
            } else if (same_token("mod", 3, tok, len)) {
                struct TestModule *mod = parse_module();
                if (!mod)
                    goto err;
                mod->next = suite->mods;
                suite->mods = mod;
                suite->n_mods++;
            } else if (same_token("tolerance", 9, tok, len)) {
                char *tolend;
                tok = next_token(&len);
                if (!tok || tok == END_OF_LINE) {
                    fail(read_pos, "expected tolerance value");
                    goto err;
                }
                suite->tolerance = strtod(read_pos, &tolend);
                if (tolend == read_pos || tolend != next_pos) {
                    fail(read_pos, "not a floating point value");
                    goto err;
                }
                next_pos = tolend;
            } else if (same_token("setup", 5, tok, len)) {
                if (suite->setup) {
                    fail(next_pos, "more than one setup routine specified");
                    goto err;
                }
                suite->setup = parse_support("setup");
                if (!suite->setup)
                    goto err;
            } else if (same_token("teardown", 8, tok, len)) {
                if (suite->teardown) {
                    fail(next_pos, "more than one teardown routine specified");
                    goto err;
                }
                suite->teardown = parse_support("teardown");
                if (!suite->teardown)
                    goto err;
            } else if (same_token("test", 4, tok, len)) {
                struct TestRoutine *test = parse_test();
                if (!test)
                    goto err;
                test->next = suite->tests;
                suite->tests = test;
                suite->n_tests++;
            } else if (same_token("end", 3, tok, len)) {
                next_pos = read_pos;
                break; // end of test suite
            } else { // fortran code
                struct Code *code;
                next_pos = read_pos = line_pos;
                code = parse_fortran();
                code->next = suite->code;
                suite->code = code;
            }
        } else { // EOF
            fail(read_pos, "expected end test_suite");
            goto err;
        }
    }

    // end test_suite name
    if (parse_end_sequence("test_suite", suite->name, suite->name_len))
        goto err;

    return suite;
 err:
    free_suites(suite);
    return NULL;
}

/* Parser entry point.  Opens and parses the test suites in the given file.
 */
struct TestSuite *parse_suite_file(const char *path)
{
    struct TestSuite *suites = NULL;
    size_t toklen;
    char *token;
    int parse_success = 0;

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Opening file %s: %s\n", path, strerror(errno));
        return NULL;
    }
    test_suite_file_name = path;

    if (fstat(fd, &statbuf)) {
        fprintf(stderr, "Stat file %s: %s\n", path, strerror(errno));
        goto close_it;
    }

    file_buf =
        (char *)mmap(NULL, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (!file_buf) {
        fprintf(stderr, "Mapping file %s: %s\n", path, strerror(errno));
        goto close_it;
    }
    file_end = file_buf + statbuf.st_size;

    if (!next_line()) {
        syntax_error();
        goto close_it;
    }
    for (;;) {
        token = next_token(&toklen);
        if (same_token("test_suite", 10, token, toklen)) {
            struct TestSuite *suite = parse_suite();
            if (!suite) // parse failure
                break;
            suite->next = suites;
            suites = suite;
        } else if (token == END_OF_LINE) {
            if (!next_line()) { // EOF
                if (!suites) {
                    fprintf(stderr, "Error: expected one or more test suites in file %s\n", path);
                } else {
                    parse_success = -1;
                }
                break;
            }
        } else {
            fail(read_pos, "expected to see \"test_suite\"");
            break;
        }
    }

    if (parse_success)
        return suites;

 close_it:
    if (suites)
        close_suite_and_free(suites);
    return NULL;
}
