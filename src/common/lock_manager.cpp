#include "dfs/lock_manager.h"
#include "utils/dfs-utils.h" 

namespace dfs {

using namespace dfs_service;

LockManager::LockManager(std::shared_ptr<EventBus> event_bus) : event_bus(event_bus) {}

bool LockManager::Acquire(const std::string& filename, const std::string& client_id) {
    std::lock_guard<std::mutex> lock(mtx);
    
    auto it = locks.find(filename);
    if (it != locks.end()) {
        if (it->second == client_id) {
            return true;
        } else {
            if (event_bus) {
                SystemEvent e;
                e.set_type(EventType::LOCK_DENIED);
                e.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
                e.set_source_node(client_id);
                e.set_resource(filename);
                e.set_target_node("server");
                (*e.mutable_metadata())["holder"] = it->second;
                event_bus->Publish(e);
            }
            return false;
        }
    }


    locks[filename] = client_id;
    
    if (event_bus) {
        SystemEvent e;
        e.set_type(EventType::LOCK_ACQUIRED);
        e.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        e.set_source_node(client_id);
        e.set_resource(filename);
        e.set_target_node("server");
        event_bus->Publish(e);
    }
    
    return true;
}

bool LockManager::Release(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mtx);
    auto count = locks.erase(filename);
    
    if (count > 0 && event_bus) {
        SystemEvent e;
        e.set_type(EventType::LOCK_RELEASED);
        e.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        e.set_resource(filename);
        e.set_target_node("server");
        event_bus->Publish(e);
    }
    
    return count > 0;
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

} 