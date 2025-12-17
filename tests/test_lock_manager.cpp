#include <gtest/gtest.h>
#include "dfs/lock_manager.h"
#include <thread>
#include <vector>

class LockManagerTest : public ::testing::Test {
protected:
    dfs::LockManager lock_manager;
};

TEST_F(LockManagerTest, AcquireAndRelease) {
    std::string file = "file1.txt";
    std::string client = "clientA";

    EXPECT_TRUE(lock_manager.Acquire(file, client));
    EXPECT_EQ(lock_manager.GetOwner(file), client);

    EXPECT_TRUE(lock_manager.Release(file));
    EXPECT_FALSE(lock_manager.GetOwner(file).has_value());
}

TEST_F(LockManagerTest, ReentrantLock) {
    std::string file = "file1.txt";
    std::string client = "clientA";

    EXPECT_TRUE(lock_manager.Acquire(file, client));
    EXPECT_TRUE(lock_manager.Acquire(file, client));
}

TEST_F(LockManagerTest, Conflict) {
    std::string file = "file1.txt";
    std::string clientA = "clientA";
    std::string clientB = "clientB";

    EXPECT_TRUE(lock_manager.Acquire(file, clientA));

    EXPECT_FALSE(lock_manager.Acquire(file, clientB));
    
    EXPECT_EQ(lock_manager.GetOwner(file), clientA);

    lock_manager.Release(file);

    EXPECT_TRUE(lock_manager.Acquire(file, clientB));
    EXPECT_EQ(lock_manager.GetOwner(file), clientB);
}

TEST_F(LockManagerTest, ReleaseNonExistent) {
    EXPECT_FALSE(lock_manager.Release("non_existent_file"));
}

TEST_F(LockManagerTest, Concurrency) {

    std::string file = "shared_file";
    std::atomic<int> success_count{0};
    int num_threads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            std::string client = "client" + std::to_string(i);
            if (lock_manager.Acquire(file, client)) {
                success_count++;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                lock_manager.Release(file);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(success_count, 0);
    EXPECT_FALSE(lock_manager.GetOwner(file).has_value());
}
