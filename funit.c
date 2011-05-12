#include "funit.h"
#include <limits.h>
#include <unistd.h>
#include <string.h>

// XXX make these configurable
#define TPL_EXT ".fun" // template extension
#define FTN_EXT ".F90" // extension for fortran source code (deps)

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
 * malloc()'d.
 */
static int file_dependency(char *infile, struct TestSuite *suite)
{
    static struct TestDependency dep;
    static char buf[PATH_MAX], *test, *dot;

    test = strstr(infile, "test_");
    dot = strrchr(buf, '.');
    if (test && (!dot || dot > test + 5)) { // extract name after 'test_'
        strcpy(buf, test + 5);
        if (dot) {
            if (!strcmp(dot, TPL_EXT)) { // .fun -> .F90
                strcpy(dot, FTN_EXT);
            } // else assume correct extension already
        } else { // append .F90
            strcat(buf, FTN_EXT);
        }

        // add this dependency to the dep list of each suite
        while (suite) {
            dep.next = suite->deps;
            suite->deps = &dep;
            suite = suite->next;
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
    struct TestSuite *suites, *suite;
    FILE *fout;
    int code = 0, dep_added;

    suites = parse_suite_file(infile);
    if (!suites)
        return -1;

    dep_added = file_dependency(infile, suites);

    if (!outfile)
        outfile = make_fortran_name(infile);

    fout = fopen(outfile, "w");
    if (!fout) {
        fprintf(stderr, "could not open %s for writing\n", outfile);
        return -1;
    }

    if (generate_code_file(suites, fout))
        code = -1;

    if (fclose(fout)) {
        fprintf(stderr, "error closing %s\n", outfile);
        code = -1;
    }

    if (dep_added) {
        suite = suites;
        while (suite) {
            suite->deps = suite->deps->next;
            suite = suite->next;
        }
    }
    close_suite_and_free(suites);

    return code;
}

int main(int argc, char **argv)
{
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
            fprintf(stderr, "%s: unprogrammed option %c\n", argv[0], (char)opt);
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

    while (optind < argc) {
        return generate_code(argv[optind], outfile);
        // XXX build the code with make or sth
        optind++;
    }

    return 0;
}
