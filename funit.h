#ifndef FUNIT_H
#define FUNIT_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#define TRUE (-1)
#define FALSE (0)


#define NEW(type)  (type *)fu_alloc(sizeof(type))
#define NEW0(type) (type *)fu_alloc0(sizeof(type))

/* Wrappers for malloc() and realloc() that abort on failure.
 */
void *fu_alloc(size_t bytes);
void *fu_alloc0(size_t bytes);
void *fu_realloc(void *ptr, size_t bytes);

// XXX for now...
#define fu_alloca(bytes) fu_alloc(bytes)
#define fu_freea(ptr) free(ptr)


char *fu_strndup(char *str, size_t len);
char *fu_strdup(char *str);


struct Buffer {
    char *s;     // the malloc()'d buffer space
    size_t size; // size of the buffer
    size_t i;    // current write index in the buffer
};

void init_buffer(struct Buffer *buf, size_t initsize);
void grow_buffer(struct Buffer *buf);
void buffer_ncat(struct Buffer *buf, const char *app, size_t applen);
void buffer_cat(struct Buffer *buf, const char *app);
void free_buffer(struct Buffer *buf);


int fu_stat(const char *path, struct stat *bufp);
int fu_isdir(const char *path);
int fu_pathcat(char *buf, size_t bufsize, const char *path1, const char *path2);


struct Config {
    char *build;
    char *tempdir;
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
void parse_fail3(struct ParseState *ps, char *prefix,
                 char *s, size_t len, char *postfix);
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

// Config file
int read_config(struct Config *conf);
void free_config(struct Config *conf);

// Parser interface
struct TestFile *parse_test_file(const char *path);
void close_testfile(struct TestFile *tf);

// Code generator
int generate_code_file(struct TestSet *file, FILE *fout);

#endif // FUNIT_H
