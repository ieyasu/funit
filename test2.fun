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
    call assert_blah("this must not expand")
  end test case1

  test case2
    a = a - 1
    assert_true(a == 5)
    print *, 'hello'
  end test case2

  test case3
    assert_equal(5.0, 7.0 - 2)
    assert_not_equal(4, &
!4 + 1)
4 + 0)
    flunk("if that & that ( didn't stop it, &
           &this will")
  end test case3
end test_suite
