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
 * buf must be at least (PATH_MAX + 1) in length.
 */
static void make_fortran_path(char *infile, char *buf, struct Config *conf)
{
    if (fu_pathcat(buf, PATH_MAX, conf->tempdir, infile))
        abort();

    strcat(buf, ".XXXXXX");

    if (mkstemp(buf) == -1) {
        fprintf(stderr, "mkstemp() for fortran name: %s\n", strerror(errno));
        abort();
    }

    strcat(buf, ".F90");
}

static void make_exe_path(struct Config *conf, char *buf)
{
    size_t templen = strlen(conf->tempdir);
    size_t pathlen = templen + 13 + 1;
    fu_pathcat(buf, pathlen, conf->tempdir, "funit-XXXXXX");
    if (mkstemp(buf) == -1) {
        fprintf(stderr, "mkstemp() for exe name: %s\n", strerror(errno));
        abort();
    }
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
            make_fortran_path(infile, buf, &conf);
            fortran_out = buf;
        }

        struct TestFile *tf = generate_code(infile, fortran_out, &conf);
        if (tf) {
            if (!just_output_fortran) {
                char exe_path[PATH_MAX + 1];
                make_exe_path(&conf, exe_path);

                if (build_test(tf, &conf, fortran_out, exe_path) == 0) {
                    if (run_test(tf, &conf, exe_path) == 0) {
                        unlink(exe_path);
                    }
                    unlink(fortran_out);
                }
            }
            close_testfile(tf);
        } else {
            return -1; // error generating code - exit with error
        }
        optind++;
    }

    free_config(&conf);
    return 0;
}
