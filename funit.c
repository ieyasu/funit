#include "funit.h"
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>

static const char usage[] = 
"Usage: funit [-E] [-o file] [test_file.fun...|testdir]\n"
"             [-h]\n"
"\n"
"  -E       stop after emitting Fortran code from the template .fun files\n"
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
 * fu_alloc()'d.
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
static char *make_fortran_name(char *infile)
{
    static char buf[PATH_MAX + 1], *dot;

    if (strlen(infile) > PATH_MAX - 4) {
        fprintf(stderr, "The input file name '%s' is too long\n", infile);
        abort();
    }

    dot = strrchr(infile, '.');
    if (dot && !strcmp(dot, TPL_EXT)) { // .fun -> .F90
        strncpy(buf, infile, dot - infile);
        strcpy(buf + (dot - infile), FTN_EXT);
    } else { // unrecognized or missing extension, just append .F90
        strcpy(buf, infile);
        strcat(buf, FTN_EXT);
    }
    return buf;
}

static int generate_code(char *infile, char *outfile)
{
    struct TestFile *tf;
    FILE *fout;
    int code = 0, dep_added;

    tf = parse_test_file(infile);
    if (!tf)
        return -1;
    if (!tf->sets) {
        close_testfile(tf);
        return -1;
    }

    dep_added = file_dependency(infile, tf->sets);

    if (!outfile)
        outfile = make_fortran_name(infile);

    fout = fopen(outfile, "w");
    if (!fout) {
        fprintf(stderr, "could not open %s for writing\n", outfile);
        return -1;
    }

    if (generate_code_file(tf->sets, fout))
        code = -1;

    if (fclose(fout)) {
        fprintf(stderr, "error closing %s\n", outfile);
        code = -1;
    }

    if (dep_added) { // XXX wtf is this doing?
        struct TestSet *set = tf->sets;
        while (set) {
            set->deps = set->deps->next;
            set = set->next;
        }
    }
    close_testfile(tf);

    return code;
}

int main(int argc, char **argv)
{
    struct Config conf;
    int just_output_fortran = 0;
    char *outfile = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "Eho:")) != -1) {
        switch(opt) {
        case 'E':
            just_output_fortran = -1;
            break;
        case 'h':
            fputs(usage, stderr);
            return 0;
        case 'o':
            outfile = optarg;
            break;
        case '?': // unrecognized option or missing argument
            fputs(usage, stderr);
            return -1;
        default:
            fprintf(stderr, "%s: unknown option %c\n", argv[0], (char)opt);
            abort();
        }
    }
    if (optind == argc) {
        fprintf(stderr, "%s: missing test files\n", argv[0]);
        fputs(usage, stderr);
        return -1;
    }
    if (outfile && optind + 1 < argc) {
        fprintf(stderr, "%s: only one input file can be given when "
                "specifying the output file\n", argv[0]);
        return -1;
    }

    if (read_config(&conf)) {
        free_config(&conf);
        return -1;
    }

    while (optind < argc) {
        if (generate_code(argv[optind], outfile) == 0) {
            // XXX build the code with make or sth
        }
        optind++;
    }

    free_config(&conf);
    return 0;
}
