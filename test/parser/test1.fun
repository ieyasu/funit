test_suite suite name
  dep "../file1.F90"
  dep "../file2.F90"

  use a_module

  tolerance 0.00001

  a = a + 1 ! more fortran for some reason
  b = b * a  

  setup
    ! fortran code to run before each test
  end setup

  teardown
    ! fortran code to run after each test
  end teardown
  test case1
    ...
    test_blah("this must not expand")
  end test case1
end test_suite

