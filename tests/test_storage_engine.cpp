#include <gtest/gtest.h>
#include "dfs/storage/posix_storage_engine.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class PosixStorageEngineTest : public ::testing::Test {
protected:
    dfs::storage::PosixStorageEngine engine;
    fs::path test_dir;

    void SetUp() override {
        // Create a unique temporary directory for each test
        test_dir = fs::temp_directory_path() / ("dfs_test_" + std::to_string(std::rand()));
        fs::create_directories(test_dir);
    }

    void TearDown() override {
        // Cleanup
        fs::remove_all(test_dir);
    }
};

TEST_F(PosixStorageEngineTest, WriteAndRead) {
    std::string filename = "test_file.txt";
    std::string full_path = (test_dir / filename).string();
    std::string content = "Hello, World!";

    // Test Write
    EXPECT_NO_THROW(engine.Write(full_path, content));

    // Test Exists
    EXPECT_TRUE(engine.Exists(full_path));

    // Test Read
    std::string read_content = engine.Read(full_path, 0, content.size());
    EXPECT_EQ(read_content, content);
}

TEST_F(PosixStorageEngineTest, Append) {
    std::string filename = "append_test.txt";
    std::string full_path = (test_dir / filename).string();
    std::string part1 = "Hello";
    std::string part2 = " World";

    engine.Write(full_path, part1);
    engine.Write(full_path, part2, true); // Append = true

    std::string read_content = engine.Read(full_path, 0, 100);
    EXPECT_EQ(read_content, "Hello World");
}

TEST_F(PosixStorageEngineTest, Delete) {
    std::string filename = "delete_test.txt";
    std::string full_path = (test_dir / filename).string();
    
    engine.Write(full_path, "data");
    EXPECT_TRUE(engine.Exists(full_path));

    engine.Delete(full_path);
    EXPECT_FALSE(engine.Exists(full_path));
}

TEST_F(PosixStorageEngineTest, List) {
    engine.Write((test_dir / "file1.txt").string(), "1");
    engine.Write((test_dir / "file2.txt").string(), "2");
    fs::create_directory(test_dir / "subdir"); // List should assume no dirs or handle them? 
    // The current implementation uses readdir which returns everything. 
    // The server logic filters DT_DIR. The StorageEngine returns everything except . and ..
    
    std::vector<std::string> files = engine.List(test_dir.string());
    
    EXPECT_GE(files.size(), 2);
    
    bool found1 = false;
    bool found2 = false;
    for (const auto& f : files) {
        if (f == "file1.txt") found1 = true;
        if (f == "file2.txt") found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

TEST_F(PosixStorageEngineTest, StatAndMTime) {
    std::string filename = "stat_test.txt";
    std::string full_path = (test_dir / filename).string();
    engine.Write(full_path, "data");

    struct stat s = engine.Stat(full_path);
    EXPECT_GT(s.st_size, 0);

    // Test UpdateMTime
    // Set mtime to 1000 seconds ago
    time_t new_mtime = time(nullptr) - 1000;
    engine.UpdateMTime(full_path, new_mtime);

    struct stat s2 = engine.Stat(full_path);
    EXPECT_EQ(s2.st_mtime, new_mtime);
}

TEST_F(PosixStorageEngineTest, WriteError) {
    // Try to write to a directory path (should fail)
    // Or a non-existent directory
    std::string bad_path = (test_dir / "non_existent_dir" / "file.txt").string();
    EXPECT_THROW(engine.Write(bad_path, "data"), std::runtime_error);
}
