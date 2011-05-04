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
      end test case1
    end test_suite
 */
#include "funit.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static const char *END_OF_LINE = (char *)0x1;

static const char *filename = NULL;
static int fd = 0;
static struct stat statbuf;
static char *file_buf = NULL, *file_end = NULL;
static char *line_pos = NULL, *next_line_pos = NULL;
static char *read_pos = NULL, *next_pos = NULL;
static long lineno = 0;


static void free_code(struct Code *code)
{
    if (code->next)
        free_code(code->next);
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
        fprintf(stderr, "Unmapping file %s: %s\n", filename, strerror(errno));
        abort();
    }

    if (fd && !close(fd)) {
        fd = 0;
        filename = NULL;
    } else {
        fprintf(stderr, "Closing file %s: %s\n", filename, strerror(errno));
        abort();
    }

    free_suites(suites);
}

static void fail(const char *col, const char *message)
{
    assert(line_pos != NULL);

    // file:line:
    assert(filename != NULL);
    fprintf(stderr, "%s:%li:\n\n", filename, lineno);
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

    abort();
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
    assert(file_buf != NULL);

    if (line_pos) {
        assert(next_line_pos >= line_pos || line_pos == file_end);
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
        if (line_pos >= file_end)
            return NULL;
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

typedef void end_finder_fun(void);

static char *next_thing(size_t *len, end_finder_fun end_fun)
{
    read_pos = next_pos;
    skip_ws();

    next_pos = read_pos;
    end_fun();

printf("thing = '");
fwrite(read_pos, next_pos - read_pos, 1, stdout);
puts("'");

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
    char *start = line_pos, *tok;
    size_t len;

    // read lines until a recognized end sequence appear
    while (line_pos < file_end) {
        // look for end sequence
        tok = next_token(&len);
        assert(tok != NULL);
        if (is_suite_token(tok, len) ||
            (same_token("end", 3, tok, len) && next_is_suite_end_token())) {
            break;
        }
        // if not found, continue
        next_line();
    }
    if (line_pos < file_end) { // found non-fortran code
        // record bounds of fortran code
        code->str = start;
        code->len = line_pos - start;
        next_pos = read_pos = line_pos;
        return code;
    }
    syntax_error();
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
    next_line();
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

static struct Code *parse_support(const char *kind)
{
    struct Code *code = NULL;

    if (expect_eol())
        goto err;

    code = parse_fortran();
    if (!code)
        goto err;

    printf("code: '");
    fwrite(code->str, code->len, 1, stdout);
    printf("'\n");

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

    if (expect_eol())
        goto err;

    routine->code = parse_fortran();
    if (!routine->code)
        goto err;

    if (parse_end_sequence("test", routine->name, routine->name_len))
        goto err;

    return routine;
 err:
    free_tests(routine);
    return NULL;
}

static struct TestDependency *concat_deps(struct TestDependency *list1,
                                          struct TestDependency *list2)
{
    struct TestDependency *d = list1;

    while (d->next)
        d = d->next;
    d->next = list2;

    return list1;
}

// XXX need a way to add to the code search path for use -module- and
// XXX dep <-file-> stuff

static struct TestDependency *parse_fortran_deps(struct Code *code)
{
    struct TestDependency *deps = NULL;

    if (code->next)
        deps = parse_fortran_deps(code->next);

    // XXX do the parsing thing (looking for modules)
    // XXX if dep found, create new dep node and add to front of list

    return deps;
}

static struct TestDependency *parse_test_deps(struct TestRoutine *routine)
{
    struct TestDependency *deps = NULL, *deps2;

    if (routine->next)
        deps = parse_test_deps(routine->next);

    deps2 = parse_fortran_deps(routine->code);
    if (deps2)
        deps = concat_deps(deps2, deps);

    return deps;
}

static struct TestSuite *parse_suite(void)
{
    struct TestSuite *suite = NEW0(struct TestSuite);
    struct TestDependency *deps;
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
printf("tok = '");
fwrite(tok, len, 1, stdout);
printf("'\n");
            if (same_token("dep", 3, tok, len)) {
                struct TestDependency *dep = parse_dependency();
                if (!dep)
                    goto err;
                dep->next = suite->deps;
                suite->deps = dep;
                suite->n_deps++;
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
puts("found fortran code");
                next_pos = read_pos = line_pos;
                code = parse_fortran();
                code->next = suite->code;
                suite->code = code;
puts("got fortran code");
printf("leaving = '");
fwrite(line_pos, next_line_pos - line_pos, 1, stdout);
puts("'");
            }
        } else { // EOF
            fail(read_pos, "expected end test_suite");
            goto err;
        }
    }

    // end test_suite name
    if (parse_end_sequence("test_suite", suite->name, suite->name_len))
        goto err;

    // parse fortran source for more dependencies
    if (suite->code) {
        deps = parse_fortran_deps(suite->code);
        if (deps)
            suite->deps = concat_deps(deps, suite->deps);
    }
    deps = parse_test_deps(suite->tests);
    if (deps)
        suite->deps = concat_deps(deps, suite->deps);

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
    filename = path;

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
        if (token && token != END_OF_LINE) {
printf("root token = '");
fwrite(token, toklen, 1, stdout);
printf("'\n");
        } else {
printf("root token = %p\n", token);
        }
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
                assert(line_pos == file_end);
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
