#ifndef FUNIT_H
#define FUNIT_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct Config {
    char *build;
    void *build_fragments;
    char *fortran_ext;
    char *template_ext;
    size_t build_len;
    size_t fortran_ext_len;
    size_t template_ext_len;
};

struct StringBuffer {
    char *s;         // the start of the string buffer
    size_t cap, len;
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

/* A fragment of Fortran, funit test macro, or funit macro argument code.
 */
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

/* A source file name denoted by the "dep" macro.
 */
struct TestDependency {
    struct TestDependency *next;
    char *filename;
    size_t len;
};

/* A module name extracted from a Fortran "use" directive.
 */
struct TestModule {
    struct TestModule *next;
    char *name;
    char *extra;
    size_t len, elen;
};

/* One of the test case functions denoted by the "test" macro.
 */
struct TestCase {
    struct TestCase *next;
    char *name;
    size_t namelen;
    int need_array_iterator;
    struct Code *code;
};

/* A set of test cases denoted by the "set" macro.
 */
struct TestSet {
    struct TestSet *next;
    struct TestDependency *deps;
    struct TestModule *mods;
    struct Code *setup, *teardown;
    struct TestCase *tests;
    struct Code *code;
    size_t n_deps, n_mods, n_tests;
    char *name;
    size_t namelen;
    double tolerance;
};

struct TestFile {
    const char *path;
    const char *exe;
    struct TestSet *sets;
    // private:
    struct ParseState ps;
};

#define DEFAULT_TOLERANCE (0.00001)

// The name of the current test set template file
// XXX do I really want a global var for this?
extern const char *test_set_file_name;

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (!FALSE)
#endif

#ifndef MIN
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

#define NEW(type)  (type *)malloc(sizeof(type))
#define NEW0(type) (type *)calloc(1, sizeof(type))
#define NEWA(type,count) (type *)malloc(sizeof(type) * (count))

// Config file
int read_config(struct Config *conf);
void free_config(struct Config *conf);

// Parser interface
struct TestFile *parse_test_file(const char *path);
void close_testfile(struct TestFile *tf);

// Code generator
int generate_code_file(struct TestSet *file, FILE *fout);

// build rules
void *parse_build_rule(char *build);
void make_build_command(struct StringBuffer *sb,
                        const struct TestFile *tf,
                        const struct Config *conf);
void free_build_fragments(void *p);

// utility
char *fu_strndup(const char *str, size_t len);
char *fu_strdup(const char *str);
void sb_init(struct StringBuffer *sb, size_t length);
void sb_free(struct StringBuffer *sb);
void sb_ensure(struct StringBuffer *sb, size_t at_least);
void sb_add_char(struct StringBuffer *sb, char c);
void sb_add_nstr(struct StringBuffer *sb, const char *s, size_t len);
void sb_add_str(struct StringBuffer *sb, const char *s);

#endif // FUNIT_H
