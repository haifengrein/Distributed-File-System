
add_executable(dfs_tests
    test_utils.cpp
    test_concurrency.cpp
    test_storage_engine.cpp
    test_lock_manager.cpp
    test_sync_engine.cpp
    test_event_bus.cpp
)

target_link_libraries(dfs_tests PRIVATE 
    dfs_common 
    dfs_proto 
    GTest::GTest 
    GTest::Main 
    Threads::Threads
)


include(GoogleTest)
gtest_discover_tests(dfs_tests)