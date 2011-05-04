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
  "    integer,intent(in) :: passed\n" \
  "    character(*),intent(in) :: message, test_name\n" \
  "\n" \
  "    if (passed) then\n" \
  "       pass_count = pass_count + 1\n" \
  "       print *, \"  test \", test_name, \"PASSED\"\n" \
  "    else\n" \
  "       fail_count = fail_count + 1\n" \
  "       print *, \"  test \", test_name, \"FAILED\"\n" \
  "       print *, message\n" \
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
  "    ! XXX print time taken: \"Finished in 2.3 seconds\"\n" \
  "\n" \
  "    ! \"3 tests in 1 suite, 1 failure\"\n" \
  "    print *, (pass_count + fail_count), \"tests in\", suite_count, \"suites\", &\n" \
  "         \", \", fail_count, \"failures\"\n" \
  "  end subroutine report_stats\n" \
  "end module funit\n" \
  "\n" \
  "\n" \
  "\n" \
  "#if 0\n" \
  "  subroutine a_test_case(passed, _message)\n" \
  "    logical, intent(out) :: passed\n" \
  "    character(*) :: _message\n" \
  "\n" \
  "    !assert_true(expr):\n" \
  "    if (.not. (expr)) then\n" \
  "       write(_message,*) \"expr\", \"is false\"\n" \
  "       passed = .false.\n" \
  "       return\n" \
  "    end if\n" \
  "\n" \
  "    !assert_false(expr):\n" \
  "    if (expr) then\n" \
  "       write(_message,*) \"expr\", \"is false\"\n" \
  "       passed = .false.\n" \
  "       return\n" \
  "    end if\n" \
  "\n" \
  "    passed = .true.\n" \
  "  end subroutine a_test_case\n" \
  "\n" \
  "  program main\n" \
  "    character*512 :: message\n" \
  "    integer :: passed\n" \
  "\n" \
  "    call clear_stats\n" \
  "\n" \
  "    call start_suite(\"suite name\")\n" \
  "\n" \
  "    call a_test_case(passed, message)\n" \
  "    call pass_fail(passed, message, \"a_test_case\")\n" \
  "\n" \
  "    call report_stats\n" \
  "  end program main\n" \
  "#endif\n" \
;
