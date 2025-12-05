# Test Executable
add_executable(dfs_tests
    test_utils.cpp
    test_concurrency.cpp
)

target_link_libraries(dfs_tests PRIVATE 
    dfs_common 
    dfs_proto 
    GTest::GTest 
    GTest::Main 
    Threads::Threads
)

# Register tests
include(GoogleTest)
gtest_discover_tests(dfs_tests)