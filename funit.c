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

/* Given the "test_THING.fun" test file, compute the name of the output file
 * to write the Fortran code to.
 * bufp should be at least (PATH_MAX + 1) in length.
 */
static void make_fortran_name(char *infile, char *buf, struct Config *conf)
{
    if (strlen(infile) > PATH_MAX - strlen(conf->fortran_ext) - 7) {
        fprintf(stderr, "error: the input file name '%s' is too long\n",
                infile);
        abort();
    }

    char *dot = strrchr(infile, '.');
    if (dot && !strcmp(dot, conf->template_ext)) {
        strncpy(buf, infile, dot - infile);
        strcpy(buf + (dot - infile), conf->fortran_ext);
    } else { // unrecognized or missing extension, just append extension
        strcpy(buf, infile);
        strcat(buf, conf->fortran_ext);
    }

    // XXX skip into bufp so don't have to scan as far for '\0'
    strcat(buf, ".XXXXXX");
    if (mkstemp(buf)) {
        // XXX print error from errno
        abort();
    }
}

static char *make_exe_name(struct Config *conf)
{
    size_t templen = strlen(conf->tempdir);
    size_t exepathlen = templen + 13 + 1;
    char *exepath = fu_alloc(exepathlen);
    fu_pathcat(exepath, exepathlen, conf->tempdir, "funit-XXXXXX");
    if (mkstemp(exepath) == -1) {
        fprintf(stderr, "error in mkstemp() for exe name: %s\n",
                strerror(errno));
        abort();
    }
    return exepath;
}

static struct TestFile *
generate_code(char *infile, char *outfile, const struct Config *conf)
{
    struct TestFile *tf = parse_test_file(infile);
    if (!tf) return NULL;
    if (!tf->sets) {
        close_testfile(tf);
        return NULL;
    }
    FILE *fout = fopen(outfile, "w");
    if (!fout) {
        fprintf(stderr, "error: could not open %s for writing\n", outfile);
        return NULL;
    }

    if (generate_code_file(tf->sets, fout)) {
        close_testfile(tf);
        return NULL;
    }

    if (fclose(fout)) {
        fprintf(stderr, "error closing %s\n", outfile);
        close_testfile(tf);
        return NULL;
    }

    return tf;
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
            just_output_fortran = TRUE;
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
        char *infile = argv[optind];

        char buf[PATH_MAX + 1];
        char *fortran_out = outfile;
        if (!fortran_out) {
            make_fortran_name(infile, buf, &conf);
            fortran_out = buf;
        }

        struct TestFile *tf = generate_code(infile, fortran_out, &conf);
        if (tf) {
            if (!just_output_fortran) {
                char *exename = make_exe_name(&conf);
                build_test(tf, fortran_out, exename, &conf);
                //run_test(tf, fortran_out, &conf);
            }
            close_testfile(tf);
        } else {
            fprintf(stderr, "error generating code for %s\n", infile);
            // XXX exit with error?
        }
        optind++;
    }

    free_config(&conf);
    return 0;
}
