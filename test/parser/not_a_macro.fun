set foo
  test blah
    a = a + 1
    assert_not_a_macro("do not expand")
    b = a * 6
  end test
end set

