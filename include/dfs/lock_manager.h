#ifndef DFS_LOCK_MANAGER_H
#define DFS_LOCK_MANAGER_H

#include <string>
#include <mutex>
#include <unordered_map>
#include <optional>

namespace dfs {

class LockManager {
public:
    LockManager() = default;
    ~LockManager() = default;

    /**
     * Attempts to acquire a write lock on a file for a specific client.
     * 
     * @param filename The file to lock.
     * @param client_id The client requesting the lock.
     * @return true if the lock was successfully acquired (or already held by this client).
     * @return false if the file is already locked by a different client.
     */
    bool Acquire(const std::string& filename, const std::string& client_id);

    /**
     * Releases the lock on a file.
     * 
     * @param filename The file to unlock.
     * @return true if the lock existed and was removed.
     * @return false if the file was not locked.
     */
    bool Release(const std::string& filename);

    /**
     * Check if a file is locked by a specific client.
     */
    bool IsLockedBy(const std::string& filename, const std::string& client_id) const;

    /**
     * Get the current owner of the lock, if any.
     */
    std::optional<std::string> GetOwner(const std::string& filename) const;

private:
    mutable std::mutex mtx;
    std::unordered_map<std::string, std::string> locks; // filename -> client_id
};

} // namespace dfs

#endif // DFS_LOCK_MANAGER_H
