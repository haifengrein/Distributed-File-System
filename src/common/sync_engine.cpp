#include "dfs/sync_engine.h"

namespace dfs {

SyncAction SyncEngine::DetermineActionFromServerEvent(const FileMetadata& local_meta, const FileMetadata& server_meta) {
    // Basic Last-Write-Wins logic based on mtime
    
    if (server_meta.crc == local_meta.crc) {
        return SyncAction::NONE;
    }

    if (server_meta.mtime > local_meta.mtime) {
        return SyncAction::FETCH_FROM_SERVER;
    } else if (server_meta.mtime < local_meta.mtime) {
        // Server is older, but server sent an event? 
        // This usually means someone else wrote an old file, or clock skew.
        // If we stick to LWW, we should keep our version.
        // But we might want to push our version if server is stale?
        return SyncAction::STORE_TO_SERVER;
    }

    // mtime equal but crc diff? Collision or just modified in same second.
    // Prefer Server usually in distributed sys, or rename.
    return SyncAction::FETCH_FROM_SERVER; 
}

SyncAction SyncEngine::DetermineActionFromClientEvent(const FileMetadata& local_meta, const FileMetadata& server_meta) {
    if (local_meta.crc == server_meta.crc) {
        return SyncAction::NONE;
    }

    if (local_meta.mtime > server_meta.mtime) {
        return SyncAction::STORE_TO_SERVER;
    } else {
        // We touched a file but it's actually older than server?
        // Or we reverted it?
        return SyncAction::FETCH_FROM_SERVER;
    }
}

SyncAction SyncEngine::DetermineActionFromLocalCreate(const FileMetadata& local_meta) {
    return SyncAction::STORE_TO_SERVER;
}

SyncAction SyncEngine::DetermineActionFromRemoteCreate(const FileMetadata& server_meta) {
    return SyncAction::FETCH_FROM_SERVER;
}

} // namespace dfs
