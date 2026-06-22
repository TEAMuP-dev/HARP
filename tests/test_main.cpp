#include <gtest/gtest.h>

// use google test namespace and run any tests in the same executable
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
