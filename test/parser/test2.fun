test_suite suite name
  dep "../file1.F90"
  dep "../file2.F90"
  mod a_module

  tolerance 0.00001

  integer :: a, b

  a = a + 1 ! more fortran for some reason
  b = b * a  

  setup
    ! fortran code to run before each test
  end setup

  teardown
    ! fortran code to run after each test
  end teardown

  test case1
  end test case1

  test case2
    a = a - 1
    assert_true(a == 5)
    print *, 'hello'
  end test case2

  test the third case
    assert_equal(5.0, 7.0 - 2)
    assert_not_equal(4, &
!4 + 1)
4 + 0)
    flunk("if that & ""that' ( didn't stop it, &
           &this &
will")
  end test
end test_suite


