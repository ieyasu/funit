test_suite all macros
  tolerance 0.0007

  test all macros
    logical :: an, expression
    integer :: thing1, thing2
    real, dimension(3) :: ary1, ary2

    assert_true(an .and. expression)

    assert_false(an .or. expression)

    assert_equal(thing1, thing2)

    assert_equal_with(thing1, thing2)

    assert_equal_with(thing1, thing2, 0.002)

    assert_not_equal(thing1, thing2)

    assert_array_equal(ary1, ary2)

    assert_array_equal_with(ary1, ary2)

    assert_array_equal_with(ary1, ary2, 0.003)

    flunk("OH NOES")
  end test
end test_suite

