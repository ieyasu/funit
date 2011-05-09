#include "funit.h"

int main(int argc, char **argv)
{
    struct TestSuite *suites;
    FILE *fout;
    int code = 0;

    suites = parse_suite_file(argv[1]);
    if (!suites)
        return -1;

    fout = fopen(argv[2], "w");
    if (!fout) {
        fprintf(stderr, "could not open %s for writing", argv[2]);
        return -1;
    }

    if (generate_code_file(suites, fout))
        code = -1;

    close_suite_and_free(suites);

    return code;
}
