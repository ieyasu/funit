#ifndef FUNIT_H
#define FUNIT_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct Config {
    char *build;
    char *fortran_ext;
    char *template_ext;
};


struct ParseState {
    const char *path;
    int fd;
    size_t bufsize;

    char *file_buf, *file_end;
    char *line_pos, *next_line_pos;
    char *read_pos, *next_pos;
    long lineno;
};

typedef void end_finder_fun(struct ParseState *ps);

#define END_OF_LINE ((char *)0x1)

void parse_fail(struct ParseState *ps, const char *col, const char *message);
void parse_vfail(struct ParseState *ps, const char *col, const char *format, ...);
void syntax_error(struct ParseState *ps);
char *next_line(struct ParseState *ps);
char *skip_ws(struct ParseState *ps);
char *skip_next_ws(struct ParseState *ps);
char *next_thing(struct ParseState *ps, size_t *len, end_finder_fun end_fun);
int open_file_for_parsing(const char *path, struct ParseState *ps);
void close_parse_file(struct ParseState *ps);


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

struct TestModule {
    struct TestModule *next;
    char *name;
    char *extra;
    size_t len, elen;
};

struct TestCase {
    struct TestCase *next;
    char *name;
    size_t name_len;
    int need_array_iterator;
    struct Code *code;
};

struct TestSet {
    struct TestSet *next;
    struct TestDependency *deps;
    struct TestModule *mods;
    struct Code *setup, *teardown;
    struct TestCase *tests;
    struct Code *code;
    int n_deps, n_mods, n_tests;
    char *name;
    size_t name_len;
    double tolerance;
};

struct TestFile {
    struct TestSet *sets;
    // private
    struct ParseState ps;
};

#define DEFAULT_TOLERANCE (0.00001)

// The name of the current test set template file
// XXX do I really want a global var for this?
extern const char *test_set_file_name;

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

#define NEW(type)  (type *)malloc(sizeof(type))
#define NEW0(type) (type *)calloc(1, sizeof(type))

// Parser interface
struct TestFile *parse_test_file(const char *path);
void close_testfile(struct TestFile *tf);

// Code generator
int generate_code_file(struct TestSet *file, FILE *fout);

#endif // FUNIT_H
