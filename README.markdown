*Note:* I abandoned this project ages ago when it became clear that no one besides myself here at work was interested in using this, and I haven't been writing any new Fortran lately.  It is incomplete; use it at your own risk.


FUnit Fortran Unit Testing Framework
====================================

This unit testing framework aims to be nicer and more flexible than existing unit testing frameworks for Fortran.


Installation
============

Edit the Makefile for your system.  Type +make+.  If this succeeds it will produce a command +funit+.  Place this somewhere in your path.

I tried to learn autotools once and gave up.  I may write a configure script at some point, but if you know how, I would accept a contribution to the project.


Getting Started
===============

Write your tests (described below) in a file ending in '.fun'.  For now, you will have to generate the Fortran test code file with funit, then compile it and run the test program yourself, e.g.:

    $ funit -o test.f90 test.fun
    $ gfortran -o test test.f90
    $ ./test
     Running all macros
      test all macros   FAILED
      'an .and. expression' is false
    
    Finished in 0.00 seconds
    1 tests in 1 sets, 1 failures

In the future, this will be handled more automatically.


Writing Tests
=============

Tests are written with special syntax around Fortran code implementing each test.  The special syntax also specifies dependencies which need to be built, setup and teardown routines, etc.  By convention, these are saved in files ending in ".fun".

    set set-name
      dep "../file1.F90"
      dep "../file2.F90"
      use a_module
    
      tolerance 0.00001

      setup
        ! fortran code to run before each test
      end setup
    
      teardown
        ! fortran code to run after each test
      end teardown
    
      test case1
        ...
      end test case1
    
      test case2
        ...
      end test case2
    end set

Note: dependencies must be quoted and unlike Fortran, the strings must not be continued with an ampersand (&).


Assertions
----------

- assert_true(expression[, msg])
- assert_false(expression[, msg])
- assert_equal(a, b[, msg])
- assert_not_equal(a, b[, msg])
- assert_equal_with(a, b[, tol][, msg])
- assert_array_equal(a, b[, msg])
- assert_array_equal_with(a, b[, tol][, msg])
- flunk(msg)

The array assertions should be given array variable names rather than complex expressions because their arguments are evaluated several times.


Config File
===========

FUnit looks for config files in three locations:

1. +.funit+ in the current directory,
2. +.funit+ in the user's home directory (via the +HOME+ environment variable),
3. and finally +/etc/funitrc+.

The first time a config value is seen, that is its value.  This way, the current directory's +.funit+ will override values in +$HOME/.funit+ which in turn will overrid values in /etc/funitrc.

The syntax is like many Unix-style rc files: comments starts with '#' and continue to the end of the line; configuration settings are made with unquoted values like

    key = value

or quoted values like

    key = "value"

or

    key = 'value'

Unquoted values cannot contain spaces, tabs or the comment character ('#').  Quoted values may contain spaces, tabs or '#', but they have no way of escaping the character that opened the quotation (a quotation mark or apostrophe).  They may contain the other quotation character however.

The following configuration keys are recognized:

tempdir = /TEMP/PATH

  default: ${TMPDIR}, ${TEMP}, /tmp or /var/tmp

build = "BUILD COMMAND"

  default: TDB
  example:

fortran_ext = .EXT

  default: .f90
  example:

template_ext = .EXT

  default: .fun
  example:


Building Tests
==============

(.funit and the build command, fortran_ext...)


Running Tests
=============

*Note:* this is not working yet!

    $ funit test/test_XXX.fun

    Building set-name...
    Running set-name
      test test-name        PASSED
      test name2            PASSED
      test third            FAILED
    
    Finished in 2.3 seconds
    3 tests in 1 set, 1 failures
