/* build_and_run.c - functions to build and run the test code.
 */
#include "funit.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int expand_env_var(struct Buffer *buf,
                          const char *build, size_t *brip)
{
    assert(build[*brip] == '$' && build[*brip + 1] == '{');
    *brip += 2;

    // find end of env var name
    size_t brj = *brip;
    while (build[brj] != '}') {
        if (build[brj] == '\0') {
            fputs("warning: no matching '}' for '${' in build rule; not expanding\n", stderr);
            *brip -= 2;
            return -1;
        }
        brj++;
    }

    // copy the name of the env var into 'name'
    size_t namelen = brj - *brip;
    if (namelen == 0) {
        fputs("warning: empty environment variable reference in build rule; not expanding\n", stderr);
        *brip -= 2;
        return -1;
    }
    struct Buffer name;
    init_buffer(&name, namelen + 1);
    buffer_ncat(&name, build+*brip, namelen);

    const char *e = getenv(name.s);
    if (e) { // copy env var value into buf
        buffer_cat(buf, e);
    }

    free_buffer(&name);

    *brip = brj + 1; // get past the '}'
    return 0;
}

static char *gather_deps(struct TestFile *tf)
{
    struct Buffer buf;
    init_buffer(&buf, 32);

    struct TestSet *set = tf->sets;
    while (set) {
        struct TestDependency *dep = set->deps;
        while (dep) {
            buffer_ncat(&buf, dep->filename, dep->len);
            buffer_cat(&buf, " ");
            dep = dep->next;
        }
        set = set->next;
    }
    if (buf.i > 0) {
        buf.i--;
        buf.s[buf.i] = '\0'; // remove trailing space
    }

    char *deps = buf.s;
    buf.s = NULL;
    free_buffer(&buf);

    return deps;
}

/* Expands the internal variables into the given buffer pointer.  The internal
 * variables are:
 *
 * - {{TEST}}: the Fortran file output by funit when generating code from
 *             the .fun template file
 * - {{EXE}}: the 
 * - {{DEPS}}:
 */
static int expand_internal_var(struct Buffer *buf,
                               const char *build, size_t *brip,
                               const char *outpath, const char *exepath,
                               struct TestFile *tf)
{
    assert(build[*brip] == '{' && build[*brip + 1] == '{');
    *brip += 2;

    // find end of internal var name
    size_t brj = *brip;
    while (build[brj] != '}' && build[brj] != '\0') brj++;

    if (build[brj] != '}' || build[brj + 1] != '}') {
        goto no_matching_curlies;
    }

    // copy the name of the env var into 'name'
    size_t namelen = brj - *brip;
    if (namelen == 0) {
        fputs("warning: empty internal variable reference in build rule; not expanding\n", stderr);
        goto err;
    }

    const char *name = build+*brip;
    if (       namelen == 3 && !strncmp(name, "EXE", namelen)) {
        buffer_cat(buf, exepath);
    } else if (namelen == 4 && !strncmp(name, "TEST", namelen)) {
        buffer_cat(buf, outpath);
    } else if (namelen == 4 && !strncmp(name, "DEPS", namelen)) {
        char *deps = gather_deps(tf);
        buffer_cat(buf, deps);
        free(deps);
    } else {
       fu_error3("warning: unrecognized internal variable name '",
                 name, namelen, "'; not expanding\n");
       goto err;
    }

    *brip = brj + 2; // get past the "}}"
    return 0;

 no_matching_curlies:
    fputs("warning: internal variable reference not closed with '}}' in build command; not expanding\n", stderr);
 err:
    *brip -= 2;
    return -1;
}

/* Given the build rule from the config, replace environment variables
 * (${env_var}) and the special internal variables.
 */
static char *expand_build_vars(const char *build, const char *outpath,
                               const char *exepath, struct TestFile *tf)
{
    struct Buffer buf;
    size_t bri = 0;

    init_buffer(&buf, 64);

    do {
        if (build[bri] == '{' && build[bri + 1] == '{' &&
            !expand_internal_var(&buf, build, &bri, outpath, exepath, tf)) {
            // success!
        } else if (build[bri] == '$' && build[bri + 1] == '{' &&
                   !expand_env_var(&buf, build, &bri)) {
            // success!
        } else { // nothing special - just copy into buf
            buffer_ncat(&buf, build+bri, 1);
            bri++;
        }
    } while (build[bri] != '\0');

    // save expanded build rule and free buffer
    char *s = buf.s;
    buf.s = NULL;
    free_buffer(&buf);

    return s;
}

int build_test(struct TestFile *tf, struct Config *conf,
               const char *outpath, const char *exepath)
{
    char *build_command = expand_build_vars(conf->build, outpath, exepath, tf);

    // execute build rule
    int ret = fu_system(build_command);
    if (ret > 0) {
        fprintf(stderr, "error: build command terminated with exit status %i\n", ret);
    }
    return ret;
}

int run_test(struct TestFile *tf, struct Config *conf, const char *exe_path)
{
    int ret = fu_system(exe_path);
    if (ret > 0) {
        fprintf(stderr, "error: run command terminated with exit status %i\n",
                ret);
        return -1;
    }
    return 0;
}
