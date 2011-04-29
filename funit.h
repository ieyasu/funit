#ifndef FUNIT_H
#define FUNIT_H

#include <stdlib.h>

struct Code {
    struct Code *next;
    char *str;
    size_t len;
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
