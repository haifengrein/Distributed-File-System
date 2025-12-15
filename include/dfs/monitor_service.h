#ifndef DFS_MONITOR_SERVICE_H
#define DFS_MONITOR_SERVICE_H

#include <grpcpp/grpcpp.h>
#include "dfs-service.grpc.pb.h"
#include "dfs/event_bus.h"
#include "utils/thread_safe_queue.h"
#include <memory>

namespace dfs {

class MonitorServiceImpl final : public dfs_service::MonitorService::Service {
public:
    explicit MonitorServiceImpl(std::shared_ptr<EventBus> event_bus);
    ~MonitorServiceImpl() override;

    grpc::Status StreamEvents(grpc::ServerContext* context, 
                              const dfs_service::MonitorRequest* request, 
                              grpc::ServerWriter<dfs_service::SystemEvent>* writer) override;

private:
    std::shared_ptr<EventBus> event_bus;
};

} // namespace dfs

#endif // DFS_MONITOR_SERVICE_H
