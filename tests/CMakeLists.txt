include(FetchContent)

# get location of google test suite
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# allow use of google test suite
FetchContent_MakeAvailable(googletest)

# make a target called harp_tests and an executable comprised of 2 cpp files
add_executable(harp_tests
    test_main.cpp
    test_basic.cpp
)

# linking harp_tests to gtest_main harp makes sure  
# gtest_main and harp are compiled before harp_tests.
# harp_tests will inherit any dependencies from gtest_main & harp.
target_link_libraries(harp_tests
    gtest_main
    harp
)

# make ctests aware of our tests
add_test(NAME harp_tests COMMAND harp_tests)
