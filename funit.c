#include "funit.h"
#include <limits.h>
#include <unistd.h>
#include <string.h>

/* Command line options.
 */
struct Options {
    int just_output_fortran;
    int stop_after_build;
    char *outfile;
};

static const char usage[] = 
"Usage: funit [-E] [-o file] [test_file.fun...|testdir]\n"
"             [-h]\n"
"\n"
"  -E       stop after emitting Fortran code from the template .fun files\n"
"  -c       stop after building the generated test code\n"
"  -h       print this help message\n"
"  -o FILE  write Fortran code to FILE instead of the default name\n"
"\n"
"Generates Fortran code from the test template file(s) (or all templates\n"
"in the given directory), then compiles and runs the tests.\n"
;

/* Compute the name of the main dependency for this test based on the
 * "test_THING.fun" naming convention.
 * Returns 0 if no such file dependency was set. If 1 is returned, the caller
 * is responsible for removing the added TestDependency, as it is not
 * malloc()'d.
 */
static int file_dependency(char *infile, struct TestSet *set,
                           const struct Config *conf)
{
    static struct TestDependency dep;
    static char buf[PATH_MAX], *test, *dot;

    test = strstr(infile, "test_");
    dot = strrchr(buf, '.');
    if (test && (!dot || dot > test + 5)) { // extract name after 'test_'
        strcpy(buf, test + 5);
        if (dot) {
            if (!strcmp(dot, conf->template_ext)) { // .fun -> .F90
                strcpy(dot, conf->fortran_ext);
            } // else assume correct extension already
        } else { // append fortran extension
            strcat(buf, conf->fortran_ext);
        }

        // add this dependency to the dep list of each set
        while (set) {
            dep.next = set->deps;
            set->deps = &dep;
            set = set->next;
        }
        return 1;
    }
    return 0;
}

/* Given the "test_THING.fun" input file, compute the name of the output file
 * to write the Fortran code to.
 */
static char *make_fortran_name(char *infile, const struct Config *conf)
{
    static char buf[PATH_MAX + 1], *dot;

    if (strlen(infile) > PATH_MAX - strlen(conf->fortran_ext)) {
        fprintf(stderr, "FUnit: the input file name '%s' is too long\n",
                infile);
        abort();
    }

    dot = strrchr(infile, '.');
    if (dot && !strcmp(dot, conf->template_ext)) {
        strncpy(buf, infile, dot - infile);
        strcpy(buf + (dot - infile), conf->fortran_ext);
    } else { // unrecognized or missing extension, just append extension
        strcpy(buf, infile);
        strcat(buf, conf->fortran_ext);
    }
    return buf;
}

static struct TestFile *
generate_code(char *infile, char *outfile, const struct Config *conf)
{
    struct TestFile *tf;
    FILE *fout;
    int dep_added;

    tf = parse_test_file(infile);
    if (!tf) return NULL;
    if (!tf->sets) {
        close_testfile(tf);
        return NULL;
    }

    dep_added = file_dependency(infile, tf->sets, conf);

    // XXX if we're generating code just to run a test, use a mkstemp
    if (!outfile) {
        outfile = make_fortran_name(infile, conf);
    }
    tf->exe = outfile;

    fout = fopen(outfile, "w");
    if (!fout) {
        fprintf(stderr, "FUnit: could not open %s for writing\n", outfile);
        return NULL;
    }

    if (generate_code_file(tf->sets, fout)) {
        close_testfile(tf);
        return NULL;
    }

    if (fclose(fout)) {
        fprintf(stderr, "FUnit: error closing %s\n", outfile);
        close_testfile(tf);
        return NULL;
    }

    if (dep_added) { // XXX wtf is this doing?
        struct TestSet *set = tf->sets;
        while (set) {
            set->deps = set->deps->next;
            set = set->next;
        }
    }

    return tf;
}

static int checked_system(const char *command)
{
    int ret = system(command);

    if (ret == 127) {
        ret = -1;
        goto report_err;
    } else if (ret < 0) {
 report_err:
        fprintf(stderr, "system(): error executing '%s'\n", command);
    } // else success

    return ret;
}

static int build_test(struct TestFile *tf, struct Config *conf)
{
    struct StringBuffer sb;
    sb_init(&sb, 128);

    make_build_command(&sb, tf, conf);

    int ret = checked_system(sb.s);

    sb_free(&sb);

    return ret;
}

static int run_test(const char *testfile)
{
    if (!fu_file_exists(testfile)) {
        fprintf(stderr, "Test executable '%s' not found\n", testfile);
        return -1;
    }

    return checked_system(testfile);
}

static int parse_args(int argc, char **argv, struct Options *opts)
{
    memset(opts, 0, sizeof(struct Options));

    int opt;
    while ((opt = getopt(argc, argv, "Echo:")) != -1) {
        switch (opt) {
        case 'E':
            if (opts->stop_after_build) {
                fputs("FUnit: overriding -c with -E\n", stderr);
                opts->stop_after_build = FALSE;
            }
            opts->just_output_fortran = TRUE;
            break;
        case 'c':
            if (opts->just_output_fortran) {
                fputs("Funit: overriding -E with -c\n", stderr);
                opts->just_output_fortran = TRUE;
            }
            opts->stop_after_build = TRUE;
            break;
        case 'h':
            fputs(usage, stderr);
            return 0;
        case 'o':
            opts->outfile = optarg;
            break;
        case '?': // unrecognized option or missing argument
            fputs(usage, stderr);
            return -1;
        default:
            fprintf(stderr, "FUnit: unknown option %c\n", (char)opt);
            exit(-1);
        }
    }

    if (optind == argc) {
        fprintf(stderr, "%s: missing test files\n", argv[0]);
        fputs(usage, stderr);
        return -1;
    }

    if (opts->outfile && optind + 1 < argc) {
        fprintf(stderr, "%s: only one input file can be given when "
                "specifying the output file\n", argv[0]);
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    struct Config conf;
    struct Options opts;

    if (parse_args(argc, argv, &opts)) {
        return -1;
    }

    if (read_config(&conf)) {
        free_config(&conf);
        return -1;
    }

    while (optind < argc) {
printf("generating code from %s to %s\n", argv[optind], opts.outfile);

        struct TestFile *tf = generate_code(argv[optind], opts.outfile, &conf);
        if (tf) {
            if (opts.just_output_fortran) goto pass;
printf("building test for %s\n", opts.outfile);
            build_test(tf, &conf);

            if (opts.stop_after_build) goto pass;
printf("running test %s\n", tf->exe);
            run_test(tf->exe);

 pass:
            close_testfile(tf);
        } // XXX what does it mean if tf == NULL?

        optind++;
    }

    free_config(&conf);

    return 0;
}
