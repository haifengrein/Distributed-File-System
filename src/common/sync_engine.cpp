#include "dfs/sync_engine.h"

namespace dfs {

SyncAction SyncEngine::DetermineActionFromServerEvent(const FileMetadata& local_meta, const FileMetadata& server_meta) {
   
    
    if (server_meta.crc == local_meta.crc) {
        return SyncAction::NONE;
    }

    if (server_meta.mtime > local_meta.mtime) {
        return SyncAction::FETCH_FROM_SERVER;
    } else if (server_meta.mtime < local_meta.mtime) {
        return SyncAction::STORE_TO_SERVER;
    }
    return SyncAction::FETCH_FROM_SERVER; 
}

SyncAction SyncEngine::DetermineActionFromClientEvent(const FileMetadata& local_meta, const FileMetadata& server_meta) {
    if (local_meta.crc == server_meta.crc) {
        return SyncAction::NONE;
    }

    if (local_meta.mtime > server_meta.mtime) {
        return SyncAction::STORE_TO_SERVER;
    } else {
        return SyncAction::FETCH_FROM_SERVER;
    }
}

SyncAction SyncEngine::DetermineActionFromLocalCreate(const FileMetadata& local_meta) {
    return SyncAction::STORE_TO_SERVER;
}

SyncAction SyncEngine::DetermineActionFromRemoteCreate(const FileMetadata& server_meta) {
    return SyncAction::FETCH_FROM_SERVER;
}

} 
