module funit
  implicit none
  save

  integer :: suite_count, pass_count, fail_count
  real :: cpu_start, cpu_finish

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

    logical,intent(in) :: passed
    character(*),intent(in) :: message, test_name

    if (passed) then
       pass_count = pass_count + 1
       print *, "  test ", test_name, " PASSED"
    else
       fail_count = fail_count + 1
       print *, "  test ", test_name, " FAILED"
       print *, trim(message)
    end if
  end subroutine pass_fail

  subroutine clear_stats
    suite_count = 0
    pass_count = 0
    fail_count = 0
    call cpu_time(cpu_start);
  end subroutine clear_stats

  subroutine report_stats
    character*16 :: test_count_s, suite_count_s, fail_count_s

    print *, ""

    ! "Finished in 3.2 seconds"
    call cpu_time(cpu_finish)
    print '("Finished in ",F4.1," seconds")', cpu_finish - cpu_start

    ! "3 tests in 1 suite, 1 failure"
    write (test_count_s,*) (pass_count + fail_count)
    write (suite_count_s,*) suite_count
    write (fail_count_s,*) fail_count
    print *, trim(adjustl(test_count_s)), " tests in ", &
         trim(adjustl(suite_count_s)), " suites, ", &
         trim(adjustl(fail_count_s)), " failures"
  end subroutine report_stats
end module funit

