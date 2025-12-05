#ifndef DFS_SYNC_ENGINE_H
#define DFS_SYNC_ENGINE_H

#include <string>
#include <functional>
#include <optional>

namespace dfs {

struct FileMetadata {
    std::string name;
    time_t mtime;
    uint32_t crc;
    size_t size;
};

enum class SyncAction {
    NONE,
    FETCH_FROM_SERVER,
    STORE_TO_SERVER,
    CONFLICT_RESOLVE // For future expansion
};

/**
 * SyncEngine
 * 
 * Determines the synchronization action required based on 
 * the state of a file on the Client vs the Server.
 * 
 * This purely logic class is easy to test because it doesn't 
 * perform IO, it just decides *what* IO should be performed.
 */
class SyncEngine {
public:
    /**
     * Decides the action when a Server-side event occurs (e.g., Async Notification).
     */
    SyncAction DetermineActionFromServerEvent(const FileMetadata& local_meta, const FileMetadata& server_meta);

    /**
     * Decides the action when a Client-side event occurs (e.g., Inotify).
     */
    SyncAction DetermineActionFromClientEvent(const FileMetadata& local_meta, const FileMetadata& server_meta);

    /**
     * Determines action when we don't have server metadata cached yet.
     * (e.g. file created locally, not yet known to server)
     */
    SyncAction DetermineActionFromLocalCreate(const FileMetadata& local_meta);
    
    /**
     * Determines action when server has a file we don't have.
     */
    SyncAction DetermineActionFromRemoteCreate(const FileMetadata& server_meta);
};

} // namespace dfs

#endif // DFS_SYNC_ENGINE_H
