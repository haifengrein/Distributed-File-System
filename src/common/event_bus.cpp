#include "dfs/event_bus.h"

namespace dfs {

int EventBus::Subscribe(EventCallback callback) {
    std::lock_guard<std::mutex> lock(mtx);
    int id = next_id++;
    subscribers[id] = std::move(callback);
    return id;
}

void EventBus::Unsubscribe(int subscription_id) {
    std::lock_guard<std::mutex> lock(mtx);
    subscribers.erase(subscription_id);
}

void EventBus::Publish(const dfs_service::SystemEvent& event) {
    std::lock_guard<std::mutex> lock(mtx);
    for (const auto& pair : subscribers) {
        
        try {
            pair.second(event);
        } catch (...) {
            
        }
    }
}

} 