FUnit Fortran Unit Testing Framework
====================================

This unit testing framework aims to be nicer and more flexible than existing unit testing frameworks for Fortran.


Installation
============

    $ gem install funit

    $ ./configure && make && make install


Getting Started
===============

    $ fu init

- creates test/ structure with fixtures/, example test suite, config file, etc.


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

    $ fu test/test_XXX.fun

    Building suite-name...
    Running suite-name
      test test-name        PASSED
      test name2            PASSED
      test third            FAILED
    
    Finished in 2.3 seconds
    3 tests in 1 suite, 1 failures
