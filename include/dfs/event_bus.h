#ifndef DFS_EVENT_BUS_H
#define DFS_EVENT_BUS_H

#include <functional>
#include <map>
#include <mutex>
#include <atomic>
#include "dfs-service.pb.h"

namespace dfs {

using EventCallback = std::function<void(const dfs_service::SystemEvent&)>;

class EventBus {
public:
    EventBus() = default;
    ~EventBus() = default;

    // Subscribe to events. Returns a subscription ID.
    int Subscribe(EventCallback callback);

    // Unsubscribe using the ID returned by Subscribe.
    void Unsubscribe(int subscription_id);

    // Publish an event to all subscribers.
    void Publish(const dfs_service::SystemEvent& event);

private:
    std::mutex mtx;
    std::map<int, EventCallback> subscribers;
    std::atomic<int> next_id{1};
};

} // namespace dfs

#endif // DFS_EVENT_BUS_H
