#ifndef FUNIT_H
#define FUNIT_H

#include <assert.h>
#include <stdlib.h>

enum CodeType {
    FORTRAN_CODE,
    MACRO_CODE,
    ARG_CODE
};

enum MacroType {
    ASSERT_TRUE,
    ASSERT_FALSE,
    ASSERT_EQUAL,
    ASSERT_NOT_EQUAL,
    ASSERT_EQUAL_WITH,
    ASSERT_ARRAY_EQUAL,
    ASSERT_ARRAY_EQUAL_WITH,
    FLUNK
};

struct Code {
    struct Code *next;
    enum CodeType type;
    long lineno;
    union {
        struct {
            char *str;
            size_t len;
        } c;
        struct {
            enum MacroType type;
            struct Code *args;
        } m;
    } u;
};

struct TestDependency {
    struct TestDependency *next;
    char *filename;
    size_t len;
};

struct TestRoutine {
    struct TestRoutine *next;
    char *name;
    size_t name_len;
    struct Code *code;
};

struct TestSuite {
    struct TestSuite *next;
    struct TestDependency *deps;
    struct Code *setup, *teardown;
    struct TestRoutine *tests;
    struct Code *code;
    int n_deps, n_tests;
    char *name;
    size_t name_len;
    double tolerance;
};

#define DEFAULT_TOLERANCE (0.00001)

// The name of the current test suite template file
extern const char *test_suite_file_name;

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

#define NEW(type)  (type *)malloc(sizeof(type))
#define NEW0(type) (type *)calloc(1, sizeof(type))

// Parser interface
struct TestSuite *parse_suite_file(const char *path);
void close_suite_and_free(struct TestSuite *suites);

#endif // FUNIT_H
