/* build_and_run.c - functions to build and run the test code.
 */
#include "funit.h"
#include <string.h>

typedef void (*expand_iv)(struct StringBuffer *sb,
                          const struct TestFile *tf,
                          const struct Config *conf);

struct IVar {
    const char *name;
    expand_iv expandcb;
};

enum BRFragmentType {
    BR_STRING,
    BR_IVAR
    // env vars won't change for the life of the program so are just added as strings
};

struct BRFragment {
    enum BRFragmentType type;
    union {
        char *s;
        expand_iv expandcb;
    } frag;
};

struct BRFragments {
    struct BRFragment *frags;
    size_t n, cap;
};

// ---

static void expand_exe(struct StringBuffer *sb,
                       const struct TestFile *tf,
                       const struct Config *conf)
{
    sb_add_str(sb, tf->exe);
}

static size_t len_without_ext(const char *s, const char *ext, size_t extlen,
                              const char *what, const char *for_)
{
    size_t len = strlen(s);
    if (!strcmp(s + len - extlen, ext)) {
        len -= extlen;
    } else {
        fprintf(stderr, "Warning: %s '%s' is missing the %s extension '%s'\n",
                what, s, for_, ext);
    }
    return len;
}

// name of the test source file without the template extension
static void expand_src(struct StringBuffer *sb,
                       const struct TestFile *tf,
                       const struct Config *conf)
{
    size_t srclen = len_without_ext(tf->path, conf->template_ext,
                                    conf->template_ext_len,
                                    "test file", "template");
    sb_add_nstr(sb, tf->path, srclen);
}

// name of the test source file with the fortran extension
static void expand_src_f(struct StringBuffer *sb,
                         const struct TestFile *tf,
                         const struct Config *conf)
{
    expand_src(sb, tf, conf);
    // append fortran ext
    sb_add_nstr(sb, conf->fortran_ext, conf->fortran_ext_len);
}

// the first test set name in the source file
static void expand_set(struct StringBuffer *sb,
                       const struct TestFile *tf,
                       const struct Config *conf)
{
    assert(tf->sets != NULL);
    sb_add_nstr(sb, tf->sets->name, tf->sets->namelen);
}

// the names of the test sets in the source file
static void expand_sets(struct StringBuffer *sb,
                        const struct TestFile *tf,
                        const struct Config *conf)
{
    struct TestSet *set = tf->sets;
    while (set) {
        sb_add_nstr(sb, set->name, set->namelen);
        sb_add_char(sb, ' ');
        set = set->next;
    }
    sb->len--; // remove trailing space
}

static int have_thing(char **things, size_t n_things, const char *thing)
{
    for (size_t i = 0; i < n_things; i++) {
        if (!strcmp(things[i], thing)) {
            return TRUE;
        }
    }
    return FALSE;
}

// Add all dependency files listed in the test file
static void expand_deps(struct StringBuffer *sb,
                        const struct TestFile *tf,
                        const struct Config *conf)
{
    // count deps and allocate array to keep track of already appended deps
    size_t n_deps = 0;
    struct TestSet *set = tf->sets;
    while (set) {
        n_deps += set->n_deps;
        set = set->next;
    }

    if (n_deps == 0) return; // nothing to add? done!

    char **deps = NEWA(char *, n_deps);

    n_deps = 0; // haven't added any deps yet
    for (set = tf->sets; set; set = set->next) {
        for (struct TestDependency *dep = set->deps; dep; dep = dep->next) {
            if (!have_thing(deps, n_deps, dep->filename)) {
                deps[n_deps++] = dep->filename;
                sb_add_nstr(sb, dep->filename, dep->len);
                sb_add_char(sb, ' ');
            }
        }
    }
    sb->len--; // remove trailing space

    free(deps);
}

static void expand_mods_with(struct StringBuffer *sb,
                             const struct TestFile *tf,
                             const char *ext)
{
    // count modules and allocate array to keep track of already appended mods
    size_t n_mods = 0;
    struct TestSet *set = tf->sets;
    while (set) {
        n_mods += set->n_mods;
        set = set->next;
    }

    if (n_mods == 0) return; // no modules - done!

    char **mods = NEWA(char *, n_mods);

    n_mods = 0; // haven't added any modules yet
    for (set = tf->sets; set != NULL; set = set->next) {
        for (struct TestModule *mod = set->mods; mod!=NULL; mod = mod->next) {
            if (!have_thing(mods, n_mods, mod->name)) {
                mods[n_mods++] = mod->name;
                sb_add_nstr(sb, mod->name, mod->len);
                if (ext) sb_add_str(sb, ext);
                sb_add_char(sb, ' ');
            }
        }
    }
    sb->len--; // remove trailing space

    free(mods);
}

// all module files list in the test file
static void expand_mods(struct StringBuffer *sb,
                        const struct TestFile *tf,
                        const struct Config *conf)
{
    expand_mods_with(sb, tf, NULL);
}

// all model files list in the test file with +fortran_ext+
static void expand_mods_f(struct StringBuffer *sb,
                          const struct TestFile *tf,
                          const struct Config *conf)
{
    expand_mods_with(sb, tf, conf->fortran_ext);
}

// SRC.F + DEPS + MODS.F
static void expand_prereqs(struct StringBuffer *sb,
                           const struct TestFile *tf,
                           const struct Config *conf)
{
    expand_src_f(sb, tf, conf);
    sb_add_char(sb, ' ');
    expand_deps(sb, tf, conf);
    sb_add_char(sb, ' ');
    expand_mods_f(sb, tf, conf);
}

static struct IVar ivars[] = {
    {"DEPS",   expand_deps},
    {"EXE",    expand_exe},
    {"MODS",   expand_mods},
    {"MODS.F", expand_mods_f},
    {"PREREQ", expand_prereqs},
    {"SET",    expand_set},
    {"SETS",   expand_sets},
    {"SRC",    expand_src},
    {"SRC.F",  expand_src_f},
};
static const size_t n_ivars = sizeof(ivars) / sizeof(struct IVar);


// ---


// make sure there's at least one more space available
static void ensure_fragments(struct BRFragments *f)
{
    if (f->cap >= f->n + 1) return;

    f->cap += 2; // probably okay to grow slowly
    f->frags = realloc(f->frags, f->cap * sizeof(struct BRFragment));
    if (!f->frags) abort(); // XXX or handle allocation better?
}

static void close_string_fragment(struct BRFragments *f,
                                  struct StringBuffer *sb)
{
    ensure_fragments(f);
    f->frags[f->n].type = BR_STRING;
    f->frags[f->n].frag.s = fu_strndup(sb->s, sb->len);
    f->n++;

    sb->len = 0; // re-use buffer
}

static void add_ivar_fragment(struct BRFragments *f, struct IVar *ivar)
{
    ensure_fragments(f);
    f->frags[f->n].type = BR_IVAR;
    f->frags[f->n].frag.expandcb = ivar->expandcb;
    f->n++;
}

static void warn_ivar(char *what, char *var)
{
    static char consider[] =
        "    Consider escaping the opening braces ('{{' -> '\\{\\{')";

    fprintf(stderr, "Warning: %s '%s'\n%s\n", what, var, consider);
}

/* Attempt to parse build variable reference.  Returns the index the caller
 * should continue at.
 */
static size_t parse_internal_var(struct BRFragments *f,
                                 struct StringBuffer *sb,
                                 char *build, size_t i)
{
    assert(build[i] == '{' && build[i + 1] == '{');

    // find end of var name
     char *name = build + i + 2;
     size_t j = 0;
     while (name[j] != '\0' && name[j] != '}') j++;

     if (j == 0) {
         warn_ivar("no name given in build var reference at", build + i);
         goto recover;
     } else if (name[j] != '}' || name[j + 1] != '}') { // no "}}" terminator
         warn_ivar("no closing braces ('}}') after", build + i);
         goto recover;
     }

     name[j] = '\0'; // temporarily nul-terminate for strcmp and print purposes

     // name found, match against names in ivars[]
     struct IVar *ivar = NULL;
     for (size_t i = 0; i < n_ivars; i++) {
         if (!strcmp(name, ivars[i].name)) { // matching name
             ivar = &ivars[i];
             break;
         }
     }

     if (!ivar) {
         warn_ivar("no such build var", name);
         name[j] = '}';
         goto recover;
     }

     name[j] = '}'; // redo close brace

     // match success - append variable's value
     close_string_fragment(f, sb);
     add_ivar_fragment(f, ivar);

     return i + 2 + j + 2; // continue after closing braces

 recover: // error matching name or close braces
     sb_add_nstr(sb, "{{", 2);
     return i + 2; // continue after opening braces
}

/* Attempt to parse environment variable reference.  Returns the index the
 * caller should continue at.
 */
static size_t parse_env_var(struct StringBuffer *sb, char *build,
                            const size_t i)
{
    assert(build[i] == '$' && build[i + 1] == '{');

    // scan through to end sequence ('}') or illegal name chars
    char *name = build + i + 2;
    size_t j = strcspn(name, "} \t$\\{");
    if (name[j] != '}') {
        fprintf(stderr, "Warning: no closing brace ('}') found in build "
                "command after `%s'\n  Consider escaping the characters "
                "'${' like so: '\\$\\{'\n", build + i);
        goto recover;
    } else if (j == 0) { // empty var name
        fprintf(stderr, "Warning: no name in env var reference in build "
                "rule at `%s'\n  Unless you are missing a variable name, "
                "consider escaping the '${' like so: '\\$\\{'\n", build + i);
        goto recover;
    }

    assert(name[j] == '}');
    // end brace found - look up env var
    name[j] = '\0';
    char *value = getenv(name);
    name[j] = '}';

    if (value) {
        sb_add_str(sb, value);
    } // else empty env var - nothing to copy

    return i + 2 + j + 1;
 recover:
    sb_add_char(sb, '$');
    return i + 1;
}

/* Parses the build rule from the config file (or default).  Returns an opaque
 * pointer of type BRFragments to be stored in the struct Config and passed
 * back at build command execution time.
 */
void *parse_build_rule(char *build)
{
    struct BRFragments *f = NEW0(struct BRFragments);
    struct StringBuffer sb;
    sb_init(&sb, 128);

    // parse one character at a time, handling special forms as they appear
    size_t i = 0;
    while (build[i] != '\0') {
        switch (build[i]) {
        case '$':
            if (build[i + 1] != '{') goto just_copy;
            i = parse_env_var(&sb, build, i);
            break;
        case '{':
            if (build[i + 1] != '{') goto just_copy;
            i = parse_internal_var(f, &sb, build, i);
            break;
        case '\\':
            switch (build[i + 1]) {
            case '$':
            case '{':
            case '}':
            case '\\':
                i++; // skip the escaping backslash
                goto just_copy;
            default:
                break; // unknown sequence - no escape - fall through
            }
            // FALL THROUGH
        default:
 just_copy:
            sb_add_char(&sb, build[i++]);
            break;
        }
    }

    if (sb.len > 0) { // one last string to add
        close_string_fragment(f, &sb);
    }

    sb_free(&sb);

    return f;
}

void make_build_command(struct StringBuffer *sb,
                        const struct TestFile *tf,
                        const struct Config *conf)
{
    struct BRFragments *f = (struct BRFragments *)conf->build_fragments;
    struct BRFragment *frags = f->frags;

    for (size_t i = 0; i < f->n; i++) {
        switch (frags[i].type) {
        case BR_STRING:
            sb_add_str(sb, frags[i].frag.s);
            break;
        case BR_IVAR:
            (frags[i].frag.expandcb)(sb, tf, conf);
            break;
        }
    }
    sb_add_char(sb, '\0');
}

void free_build_fragments(void *p)
{
    struct BRFragments *f = (struct BRFragments *)p;

    for (size_t i = 0; i < f->n; i++) {
        if (f->frags[i].type == BR_STRING) {
            free(f->frags[i].frag.s);
        }
    }

    free(f->frags);

    free(f);
}
