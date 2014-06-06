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
    1 tests in 1 suites, 1 failures

In the future, this will be handled more automatically.


Writing Tests
=============

Tests are written with special syntax around Fortran code implementing each test.  The special syntax also specifies dependencies which need to be built, setup and teardown routines, etc.

    test_suite suite-name
      dep "../file1.F90"
      dep "../file2.F90"
      mod a_module
    
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
    end test_suite

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


Running Tests
=============

*Note:* this is not working yet!

    $ funit test/test_XXX.fun

    Building suite-name...
    Running suite-name
      test test-name        PASSED
      test name2            PASSED
      test third            FAILED
    
    Finished in 2.3 seconds
    3 tests in 1 suite, 1 failures
