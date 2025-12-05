#include <gtest/gtest.h>
#include "utils/dfs-utils.h"

// Simple test for the path cleaner utility
TEST(UtilsTest, CleanPathAddsSlash) {
    std::string input = "/tmp/path";
    std::string expected = "/tmp/path/";
    EXPECT_EQ(dfs_clean_path(input), expected);
}

TEST(UtilsTest, CleanPathKeepsSlash) {
    std::string input = "/tmp/path/";
    std::string expected = "/tmp/path/";
    EXPECT_EQ(dfs_clean_path(input), expected);
}

TEST(UtilsTest, CleanPathRoot) {
    std::string input = "/";
    std::string expected = "/";
    EXPECT_EQ(dfs_clean_path(input), expected);
}
