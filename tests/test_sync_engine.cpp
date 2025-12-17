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


TEST_F(SyncEngineTest, ServerUpdate_ServerNewer_ShouldFetch) {
   
    EXPECT_EQ(engine.DetermineActionFromServerEvent(file_old, file_new), SyncAction::FETCH_FROM_SERVER);
}

TEST_F(SyncEngineTest, ServerUpdate_ServerOlder_ShouldStore) {
    
    EXPECT_EQ(engine.DetermineActionFromServerEvent(file_new, file_old), SyncAction::STORE_TO_SERVER);
}

TEST_F(SyncEngineTest, ServerUpdate_Identical_ShouldDoNothing) {
   
    EXPECT_EQ(engine.DetermineActionFromServerEvent(file_old, file_same_as_old), SyncAction::NONE);
}

TEST_F(SyncEngineTest, ServerUpdate_Conflict_SameTimeDiffContent) {
   
    EXPECT_EQ(engine.DetermineActionFromServerEvent(file_old, file_same_time_diff_content), SyncAction::FETCH_FROM_SERVER);
}



TEST_F(SyncEngineTest, ClientUpdate_ClientNewer_ShouldStore) {
    
    EXPECT_EQ(engine.DetermineActionFromClientEvent(file_new, file_old), SyncAction::STORE_TO_SERVER);
}

TEST_F(SyncEngineTest, ClientUpdate_ClientOlder_ShouldFetch) {
    
    EXPECT_EQ(engine.DetermineActionFromClientEvent(file_old, file_new), SyncAction::FETCH_FROM_SERVER);
}

TEST_F(SyncEngineTest, ClientUpdate_Identical_ShouldDoNothing) {
   
    EXPECT_EQ(engine.DetermineActionFromClientEvent(file_old, file_same_as_old), SyncAction::NONE);
}



TEST_F(SyncEngineTest, LocalCreate_ShouldStore) {
    EXPECT_EQ(engine.DetermineActionFromLocalCreate(file_new), SyncAction::STORE_TO_SERVER);
}

TEST_F(SyncEngineTest, RemoteCreate_ShouldFetch) {
    EXPECT_EQ(engine.DetermineActionFromRemoteCreate(file_new), SyncAction::FETCH_FROM_SERVER);
}
