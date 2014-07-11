set case2
  real :: a, b, c

  setup
    print *, "setting up"
    a = 1.1
    b = 2.2
  end setup

  test first
    real :: d

    d = a / b
    assert_true(d < a)
    assert_true(d < b)

    print *, "aww yeah"
  end test

  test second
    flunk("lacks success")
  end test

  teardown
    c = 3.3
    print *, "tearing down"
  end teardown
end set
