#include "dfs/lock_manager.h"

namespace dfs {

bool LockManager::Acquire(const std::string& filename, const std::string& client_id) {
    std::lock_guard<std::mutex> lock(mtx);
    
    auto it = locks.find(filename);
    if (it != locks.end()) {
        // Already locked
        if (it->second == client_id) {
            // Locked by us - re-entrant success
            return true;
        } else {
            // Locked by someone else
            return false;
        }
    }

    // Not locked, acquire it
    locks[filename] = client_id;
    return true;
}

bool LockManager::Release(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mtx);
    return locks.erase(filename) > 0;
}

bool LockManager::IsLockedBy(const std::string& filename, const std::string& client_id) const {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = locks.find(filename);
    return (it != locks.end() && it->second == client_id);
}

std::optional<std::string> LockManager::GetOwner(const std::string& filename) const {
    std::lock_guard<std::mutex> lock(mtx);
    auto it = locks.find(filename);
    if (it != locks.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace dfs
