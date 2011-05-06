const char module_code[] = \
  "module funit\n" \
  "  implicit none\n" \
  "  save\n" \
  "\n" \
  "  integer :: suite_count, pass_count, fail_count\n" \
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
  "  subroutine start_suite(suite_name)\n" \
  "    implicit none\n" \
  "\n" \
  "    character(*),intent(in) :: suite_name\n" \
  "\n" \
  "    suite_count = suite_count + 1\n" \
  "\n" \
  "    print *, \"Running \", suite_name\n" \
  "  end subroutine start_suite\n" \
  "\n" \
  "  subroutine pass_fail(passed, message, test_name)\n" \
  "    implicit none\n" \
  "\n" \
  "    logical,intent(in) :: passed\n" \
  "    character(*),intent(in) :: message, test_name\n" \
  "\n" \
  "    if (passed) then\n" \
  "       pass_count = pass_count + 1\n" \
  "       print *, \"  test \", test_name, \" PASSED\"\n" \
  "    else\n" \
  "       fail_count = fail_count + 1\n" \
  "       print *, \"  test \", test_name, \" FAILED\"\n" \
  "       print *, trim(message)\n" \
  "    end if\n" \
  "  end subroutine pass_fail\n" \
  "\n" \
  "  subroutine clear_stats\n" \
  "    suite_count = 0\n" \
  "    pass_count = 0\n" \
  "    fail_count = 0\n" \
  "    ! XXX time counter\n" \
  "  end subroutine clear_stats\n" \
  "\n" \
  "  subroutine report_stats\n" \
  "    character*16 :: test_count_s, suite_count_s, fail_count_s\n" \
  "\n" \
  "    ! XXX print time taken: \"Finished in 2.3 seconds\"\n" \
  "\n" \
  "    ! \"3 tests in 1 suite, 1 failure\"\n" \
  "    print *, \"\"\n" \
  "    write (test_count_s,*) (pass_count + fail_count)\n" \
  "    write (suite_count_s,*) suite_count\n" \
  "    write (fail_count_s,*) fail_count\n" \
  "    print *, trim(adjustl(test_count_s)), \" tests in \", &\n" \
  "         trim(adjustl(suite_count_s)), \" suites, \", &\n" \
  "         trim(adjustl(fail_count_s)), \" failures\"\n" \
  "  end subroutine report_stats\n" \
  "end module funit\n" \
  "\n" \
;
