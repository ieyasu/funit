module funit
  implicit none
  save

  integer :: suite_count, pass_count, fail_count

contains
  ! others: assert_true, assert_false, assert_equal, assert_not_equal, flunk

  ! assert_true(expr):
  !
  ! if (.not. (expr)) then
  !   print *, "expr", "FAILED"
  ! end if

  subroutine start_suite(suite_name)
    implicit none

    character(*),intent(in) :: suite_name

    suite_count = suite_count + 1

    print *, "Running ", suite_name
  end subroutine start_suite

  subroutine pass_fail(passed, message, test_name)
    implicit none

    integer,intent(in) :: passed
    character(*),intent(in) :: message, test_name

    if (passed) then
       pass_count = pass_count + 1
       print *, "  test ", test_name, "PASSED"
    else
       fail_count = fail_count + 1
       print *, "  test ", test_name, "FAILED"
       print *, message
    end if
  end subroutine pass_fail

  subroutine clear_stats
    suite_count = 0
    pass_count = 0
    fail_count = 0
    ! XXX time counter
  end subroutine clear_stats

  subroutine report_stats
    ! XXX print time taken: "Finished in 2.3 seconds"

    ! "3 tests in 1 suite, 1 failure"
    print *, (pass_count + fail_count), "tests in", suite_count, "suites", &
         ", ", fail_count, "failures"
  end subroutine report_stats
end module funit



#if 0
  subroutine a_test_case(passed, _message)
    logical, intent(out) :: passed
    character(*) :: _message

    !assert_true(expr):
    if (.not. (expr)) then
       write(_message,*) "expr", "is false"
       passed = .false.
       return
    end if

    !assert_false(expr):
    if (expr) then
       write(_message,*) "expr", "is false"
       passed = .false.
       return
    end if

    passed = .true.
  end subroutine a_test_case

  program main
    character*512 :: message
    integer :: passed

    call clear_stats

    call start_suite("suite name")

    call a_test_case(passed, message)
    call pass_fail(passed, message, "a_test_case")

    call report_stats
  end program main
#endif
