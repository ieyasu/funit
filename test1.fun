test_suite suite name
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
end test_suite
