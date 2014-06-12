const char module_code[] = \
  "module funit\n" \
  "  implicit none\n" \
  "  save\n" \
  "\n" \
  "  integer :: set_count, pass_count, fail_count\n" \
  "  real :: cpu_start, cpu_finish\n" \
  "\n" \
  "contains\n" \
  "  ! others: assert_true, assert_false, assert_equal, assert_not_equal, flunk\n" \
  "\n" \
  "  ! assert_true(expr):\n" \
  "  !\n" \
  "  ! if (.not. (expr)) then\n" \
  "  !   print *, \"expr\", \"FAILED\"\n" \
  "  ! end if\n" \
  "\n" \
  "  subroutine start_set(set_name)\n" \
  "    implicit none\n" \
  "\n" \
  "    character(*),intent(in) :: set_name\n" \
  "\n" \
  "    set_count = set_count + 1\n" \
  "\n" \
  "    print *, \"Running \", set_name\n" \
  "  end subroutine start_set\n" \
  "\n" \
  "  subroutine pass_fail(passed, message, test_name, max_name_width)\n" \
  "    implicit none\n" \
  "\n" \
  "    logical,intent(in) :: passed\n" \
  "    character(*),intent(in) :: message, test_name\n" \
  "    integer,intent(in) :: max_name_width\n" \
  "    character(len=max_name_width) :: wide_name\n" \
  "\n" \
  "    wide_name = adjustl(test_name)\n" \
  "    if (passed) then\n" \
  "       pass_count = pass_count + 1\n" \
  "       write (*,'(\"  test \",A,A,\"[32m\",\" PASSED\",A,\"[39m\")') wide_name, &\n" \
  "            char(27), char(27)\n" \
  "    else\n" \
  "       fail_count = fail_count + 1\n" \
  "       write (*,'(\"  test \",A,A,\"[31m\",\" FAILED\",A,\"[39m\")') wide_name, &\n" \
  "            char(27), char(27)\n" \
  "       print *, trim(message)\n" \
  "    end if\n" \
  "  end subroutine pass_fail\n" \
  "\n" \
  "  subroutine clear_stats\n" \
  "    set_count = 0\n" \
  "    pass_count = 0\n" \
  "    fail_count = 0\n" \
  "    call cpu_time(cpu_start);\n" \
  "  end subroutine clear_stats\n" \
  "\n" \
  "  subroutine report_stats\n" \
  "    character*16 :: test_count_s, set_count_s, fail_count_s\n" \
  "    character*2 :: color_code\n" \
  "\n" \
  "    print *, \"\"\n" \
  "\n" \
  "    ! \"Finished in 3.02 seconds\"\n" \
  "    call cpu_time(cpu_finish)\n" \
  "    print '(\"Finished in \",F4.2,\" seconds\")', cpu_finish - cpu_start\n" \
  "\n" \
  "    ! \"3 tests in 1 set, 1 failure\"\n" \
  "    write (test_count_s,*) (pass_count + fail_count)\n" \
  "    write (set_count_s,*) set_count\n" \
  "    write (fail_count_s,*) fail_count\n" \
  "    write (*,'(A,\" tests in \",A,\" sets, \")',advance='no') &\n" \
  "         trim(adjustl(test_count_s)), trim(adjustl(set_count_s))\n" \
  "\n" \
  "    if (fail_count > 0) then\n" \
  "       color_code = \"31\" ! red\n" \
  "    else\n" \
  "       color_code = \"32\" ! green\n" \
  "    end if\n" \
  "    write (*,'(A,\"[\",A,\"m\")',advance='no') char(27), color_code\n" \
  "    write (*,'(A,\" failures\")',advance='no') trim(adjustl(fail_count_s))\n" \
  "    write (*,'(A,\"[39m\")') char(27)\n" \
  "  end subroutine report_stats\n" \
  "end module funit\n" \
  "\n" \
;
