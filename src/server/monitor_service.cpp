#include "dfs/monitor_service.h"
#include "utils/dfs-utils.h"

namespace dfs {

using namespace dfs_service;

MonitorServiceImpl::MonitorServiceImpl(std::shared_ptr<EventBus> event_bus) 
    : event_bus(event_bus) {}

MonitorServiceImpl::~MonitorServiceImpl() {}

grpc::Status MonitorServiceImpl::StreamEvents(grpc::ServerContext* context, 
                                              const dfs_service::MonitorRequest* request, 
                                              grpc::ServerWriter<dfs_service::SystemEvent>* writer) {
    
    dfs_log(LL_SYSINFO) << "[Monitor] New client connected to StreamEvents";
    ThreadSafeQueue<SystemEvent> event_queue;
    int sub_id = event_bus->Subscribe([&](const SystemEvent& event) {
        event_queue.push(event);
    });

    try {
        while (!context->IsCancelled()) {
        
            bool write_failed = false;
            event_queue.process_all([&](SystemEvent& event) {
                if (!writer->Write(event)) {
                    write_failed = true;
                }
            });

            if (write_failed) {
                dfs_log(LL_ERROR) << "[Monitor] Failed to write to client, closing stream.";
                break;
            }
        }
    } catch (...) {
        dfs_log(LL_ERROR) << "[Monitor] Exception in stream loop";
    }
    event_bus->Unsubscribe(sub_id);
    dfs_log(LL_SYSINFO) << "[Monitor] Client disconnected";

    return grpc::Status::OK;
}

}
