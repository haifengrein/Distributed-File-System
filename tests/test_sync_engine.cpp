#include <gtest/gtest.h>
#include "dfs/sync_engine.h"

using namespace dfs;

class SyncEngineTest : public ::testing::Test {
protected:
    SyncEngine engine;
    
    FileMetadata file_old{ "test.txt", 1000, 0xAAAA, 1024 };
    FileMetadata file_new{ "test.txt", 2000, 0xBBBB, 2048 };
    FileMetadata file_same_as_old{ "test.txt", 1000, 0xAAAA, 1024 };
    FileMetadata file_same_time_diff_content{ "test.txt", 1000, 0xCCCC, 1024 };
};

// --- Server Event Scenarios (Server pushes update) ---

TEST_F(SyncEngineTest, ServerUpdate_ServerNewer_ShouldFetch) {
    // Server says it has a newer version
    EXPECT_EQ(engine.DetermineActionFromServerEvent(file_old, file_new), SyncAction::FETCH_FROM_SERVER);
}

TEST_F(SyncEngineTest, ServerUpdate_ServerOlder_ShouldStore) {
    // Server says it updated, but its timestamp is actually OLDER than ours.
    // This implies we have pending changes that are newer.
    EXPECT_EQ(engine.DetermineActionFromServerEvent(file_new, file_old), SyncAction::STORE_TO_SERVER);
}

TEST_F(SyncEngineTest, ServerUpdate_Identical_ShouldDoNothing) {
    // Spurious notification or redundant event
    EXPECT_EQ(engine.DetermineActionFromServerEvent(file_old, file_same_as_old), SyncAction::NONE);
}

TEST_F(SyncEngineTest, ServerUpdate_Conflict_SameTimeDiffContent) {
    // Timestamp collision. Fallback to Fetch (Server Wins)
    EXPECT_EQ(engine.DetermineActionFromServerEvent(file_old, file_same_time_diff_content), SyncAction::FETCH_FROM_SERVER);
}

// --- Client Event Scenarios (Inotify triggers) ---

TEST_F(SyncEngineTest, ClientUpdate_ClientNewer_ShouldStore) {
    // We edited the file locally
    EXPECT_EQ(engine.DetermineActionFromClientEvent(file_new, file_old), SyncAction::STORE_TO_SERVER);
}

TEST_F(SyncEngineTest, ClientUpdate_ClientOlder_ShouldFetch) {
    // We touched an old file, but server has newer.
    EXPECT_EQ(engine.DetermineActionFromClientEvent(file_old, file_new), SyncAction::FETCH_FROM_SERVER);
}

TEST_F(SyncEngineTest, ClientUpdate_Identical_ShouldDoNothing) {
    // Inotify triggered but content didn't actually change (e.g. touch command or false alarm)
    EXPECT_EQ(engine.DetermineActionFromClientEvent(file_old, file_same_as_old), SyncAction::NONE);
}

// --- Creation Scenarios ---

TEST_F(SyncEngineTest, LocalCreate_ShouldStore) {
    EXPECT_EQ(engine.DetermineActionFromLocalCreate(file_new), SyncAction::STORE_TO_SERVER);
}

TEST_F(SyncEngineTest, RemoteCreate_ShouldFetch) {
    EXPECT_EQ(engine.DetermineActionFromRemoteCreate(file_new), SyncAction::FETCH_FROM_SERVER);
}
