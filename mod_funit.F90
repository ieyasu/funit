module funit
  implicit none
  save

  integer :: set_count, pass_count, fail_count
  real :: cpu_start, cpu_finish

contains
  ! others: assert_true, assert_false, assert_equal, assert_not_equal, flunk

  ! assert_true(expr):
  !
  ! if (.not. (expr)) then
  !   print *, "expr", "FAILED"
  ! end if

  subroutine start_set(set_name)
    implicit none

    character(*),intent(in) :: set_name

    set_count = set_count + 1

    print *, "Running ", set_name
  end subroutine start_set

  subroutine pass_fail(passed, message, test_name, max_name_width)
    implicit none

    logical,intent(in) :: passed
    character(*),intent(in) :: message, test_name
    integer,intent(in) :: max_name_width
    character(len=max_name_width) :: wide_name

    wide_name = adjustl(test_name)
    if (passed) then
       pass_count = pass_count + 1
       write (*,'("  test ",A,A,"[32m"," PASSED",A,"[39m")') wide_name, &
            char(27), char(27)
    else
       fail_count = fail_count + 1
       write (*,'("  test ",A,A,"[31m"," FAILED",A,"[39m")') wide_name, &
            char(27), char(27)
       print *, trim(message)
    end if
  end subroutine pass_fail

  subroutine clear_stats
    set_count = 0
    pass_count = 0
    fail_count = 0
    call cpu_time(cpu_start);
  end subroutine clear_stats

  subroutine report_stats
    character*16 :: test_count_s, set_count_s, fail_count_s
    character*2 :: color_code

    print *, ""

    ! "Finished in 3.02 seconds"
    call cpu_time(cpu_finish)
    print '("Finished in ",F4.2," seconds")', cpu_finish - cpu_start

    ! "3 tests in 1 set, 1 failure"
    write (test_count_s,*) (pass_count + fail_count)
    write (set_count_s,*) set_count
    write (fail_count_s,*) fail_count
    write (*,'(A," tests in ",A," sets, ")',advance='no') &
         trim(adjustl(test_count_s)), trim(adjustl(set_count_s))

    if (fail_count > 0) then
       color_code = "31" ! red
    else
       color_code = "32" ! green
    end if
    write (*,'(A,"[",A,"m")',advance='no') char(27), color_code
    write (*,'(A," failures")',advance='no') trim(adjustl(fail_count_s))
    write (*,'(A,"[39m")') char(27)
  end subroutine report_stats
end module funit

