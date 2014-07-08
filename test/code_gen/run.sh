#!/bin/sh
# runs each test case in the code_gen section
# expects the current working directory to be funit/test/code_gen/

check_case() {
    case_dir=$1

    for f in *.fun; do
        out=`basename $f .fun`.f90

        # generate .f90 code from .fun template
        ../../../funit -o $out $f
        if [ $? -ne 0 ]; then
            echo "funit exited with an error code"
            exit -1
        fi

        exp=${out}.exp
        if [ \! -f $exp ]; then
            echo "$exp not found in funit/test/code_gen/$case_dir"
            rm -f $out
            exit -1
        fi

        # compare generated code with expected
        diff -w -q $out $exp >/dev/null
        if [ $? -ne 0 ]; then
            echo
            echo "Differences found generating code for $f:"
            echo
            diff -w $out $exp
            rm -f $out
            exit -1
        fi

        # clean up
        rm -f $out
    done
}

printf "\n%s\n" "Running code generation test cases"

for d in *; do
    if [ -d "$d" ]; then
        printf "%s" "  $d..."
        cd $d
        check_case $d
        cd ..
        echo "done"
    fi
done
