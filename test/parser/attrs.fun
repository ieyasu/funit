set foo
  dep "a dependency"
  dep "dep 2"
  use this_module
  use another_module

  tolerance 0.0054

  setup
    fix = 7 ! some code before tests
  end setup

  teardown
    fix = 0
  end teardown

end set

