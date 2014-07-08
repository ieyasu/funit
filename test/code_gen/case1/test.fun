set my_set
  dep "./code.f90"
  use testing

  test my_test
    assert_equal(2,2)
  end test my_test

  test my_sum
    assert_equal(4.0, soma(2.0,2.0))
  end test my_sum
end set

