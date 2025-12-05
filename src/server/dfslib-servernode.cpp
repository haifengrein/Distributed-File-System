#include "dfs/dfslib-servernode.h"

#include <errno.h>
#include <getopt.h>
#include <google/protobuf/util/time_util.h>
#include <grpcpp/grpcpp.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <iostream>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <memory>

#include "dfs-service.grpc.pb.h"
#include "dfs/dfslib-shared.h"
#include "dfs/dfslibx-call-data.h"
#include "dfs/dfslibx-service-runner.h"
#include "dfs/storage/storage_engine_if.h"
#include "dfs/storage/posix_storage_engine.h"
#include "dfs/lock_manager.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::Status;
using grpc::StatusCode;

using dfs_service::DFSService;
using namespace dfs_service;
using namespace std;

using google::protobuf::Timestamp;
using google::protobuf::util::TimeUtil;

//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using in your `dfs-service.proto` file
// to indicate a file request and a listing of files from the server
//
using FileRequestType = FileListRequest;
using FileListResponseType = FileList;

extern dfs_log_level_e DFS_LOG_LEVEL;

class DFSServiceImpl final : public DFSService::WithAsyncMethod_CallbackList<DFSService::Service>,
                             public DFSCallDataManager<FileRequestType, FileListResponseType> {
private:
    /** The runner service used to start the service and manage asynchronicity **/
    DFSServiceRunner<FileRequestType, FileListResponseType> runner;

    /** The mount path for the server **/
    std::string mount_path;

    /** Storage Engine **/
    std::shared_ptr<dfs::storage::IStorageEngine> storage_engine;

    /** Lock Manager **/
    std::shared_ptr<dfs::LockManager> lock_manager;

    /** Mutex for managing the queue requests **/
    std::mutex queue_mutex;

    /** Condition Variable to wait for new requests **/
    std::condition_variable queue_cv;

    /** The vector of queued tags used to manage asynchronous requests **/
    std::vector<QueueRequest<FileRequestType, FileListResponseType>> queued_tags;

    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string& filepath) { return this->mount_path + filepath; }

    /** CRC Table kept in memory for faster calculations **/
    CRC::Table<std::uint32_t, 32> crc_table;

public:
    DFSServiceImpl(const std::string& mount_path, const std::string& server_address, int num_async_threads,
                   std::shared_ptr<dfs::storage::IStorageEngine> storage_engine,
                   std::shared_ptr<dfs::LockManager> lock_manager)
        : mount_path(mount_path), 
          crc_table(CRC::CRC_32()), 
          storage_engine(storage_engine),
          lock_manager(lock_manager) {
        this->runner.SetService(this);
        this->runner.SetAddress(server_address);
        this->runner.SetNumThreads(num_async_threads);
        this->runner.SetQueuedRequestsCallback([&] { this->ProcessQueuedRequests(); });
    }

    ~DFSServiceImpl() { this->runner.Shutdown(); }

    void Run() { this->runner.Run(); }

    /**
     * Request callback for asynchronous requests
     */
    void RequestCallback(grpc::ServerContext* context, FileRequestType* request,
                         grpc::ServerAsyncResponseWriter<FileListResponseType>* response,
                         grpc::ServerCompletionQueue* cq, void* tag) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            this->queued_tags.emplace_back(context, request, response, cq, tag);
        }
        queue_cv.notify_one();
    }

    /**
     * Process a callback request
     */
    void ProcessCallback(ServerContext* context, FileRequestType* request, FileListResponseType* response) {
        dfs_log(LL_SYSINFO) << "Enter ProcessCallback";

        FileListRequest dummy_request;

        Status status = this->ListFiles(context, &dummy_request, response);

        if (!status.ok()) {
            dfs_log(LL_ERROR) << "ProcessCallback Failed";
            return;
        }
    }

    /**
     * Processes the queued requests in the queue thread
     */
    void ProcessQueuedRequests() {
        while (true) {
            // Guarded section for queue
            {
                dfs_log(LL_DEBUG2) << "Waiting for queue guard";
                std::unique_lock<std::mutex> lock(queue_mutex);

                // Wait until there are queued tags (avoids busy loop)
                queue_cv.wait(lock, [this] { return !this->queued_tags.empty(); });

                for (QueueRequest<FileRequestType, FileListResponseType>& queue_request : this->queued_tags) {
                    this->RequestCallbackList(queue_request.context, queue_request.request, queue_request.response,
                                              queue_request.cq, queue_request.cq, queue_request.tag);
                    queue_request.finished = true;
                }

                // any finished tags first
                this->queued_tags.erase(
                    std::remove_if(this->queued_tags.begin(), this->queued_tags.end(),
                                   [](QueueRequest<FileRequestType, FileListResponseType>& queue_request) {
                                       return queue_request.finished;
                                   }),
                    this->queued_tags.end());
            }
        }
    }

    Status RequestWriteLock(ServerContext* context, const WriteLockRequest* request,
                            WriteLockResponse* response) override {
        dfs_log(LL_DEBUG2) << "[RequestWriteLock] Received WriteLock request for file: " << request->filename();
        if (context->IsCancelled()) {
            dfs_log(LL_ERROR) << "[RequestWriteLock] Request cancelled by the client or deadline exceeded.";
            return ::grpc::Status(StatusCode::DEADLINE_EXCEEDED, "Request cancelled or deadline exceeded.");
        }

        string file_name = request->filename();
        string client_id = request->client_id();

        bool lock_obtained = lock_manager->Acquire(file_name, client_id);

        if (lock_obtained) {
            dfs_log(LL_SYSINFO) << "[RequestWriteLock] Write lock granted for file: " << file_name
                                << " and Client: " << client_id;
            response->set_success(true);
            return Status::OK;
        } else {
            dfs_log(LL_ERROR) << "[RequestWriteLock] File: " << file_name << " is already locked by another client.";
            response->set_success(false);
            return Status(StatusCode::RESOURCE_EXHAUSTED, "File is locked by another client.");
        }
    }

    Status StoreFile(ServerContext* context, ServerReader<StoreRequest>* reader, StoreResponse* response) override {
        StoreRequest request;
        string filename, filepath;
        int client_file_mtime;

        bool isFirstChunk = true;
        uint32_t client_crc = 0;

        dfs_log(LL_SYSINFO) << "[StoreFile] Starting to process file";

        try {
            while (reader->Read(&request)) {
                if (isFirstChunk) {
                    filename = request.file_name();
                    client_crc = request.crc();
                    client_file_mtime = request.mtime();
                    filepath = WrapPath(filename);
                    dfs_log(LL_DEBUG2) << "[StoreFile] FirstChunk received for file: " << filename;

                    // Check if content is identical
                    uint32_t server_crc = 0;
                    try {
                        server_crc = dfs_file_checksum(filepath, &crc_table);
                    } catch (...) {
                         // Assume file doesn't exist or error, so crc is 0 or we proceed
                    }
                    
                    if (server_crc == client_crc && server_crc != 0) {
                        dfs_log(LL_SYSINFO) << "[StoreFile] Content is same, updating mtime: " << filename;
                        try {
                            storage_engine->UpdateMTime(filepath, client_file_mtime);
                        } catch (const std::exception& e) {
                            dfs_log(LL_ERROR) << "[StoreFile] Failed to update mtime: " << e.what();
                        }
                        lock_manager->Release(filename);
                        return Status(StatusCode::ALREADY_EXISTS, "File on server is identical to client's version.");
                    }

                    try {
                        storage_engine->Write(filepath, request.chunk(), false); // Overwrite first chunk
                    } catch (const std::exception& e) {
                        lock_manager->Release(filename);
                        dfs_log(LL_ERROR) << "[StoreFile] Failed to write first chunk: " << e.what();
                        return Status(grpc::INTERNAL, "Failed to write data.");
                    }
                    
                    isFirstChunk = false;
                } else {
                     // Subsequent chunks
                    try {
                        storage_engine->Write(filepath, request.chunk(), true); // Append
                    } catch (const std::exception& e) {
                        lock_manager->Release(filename);
                        dfs_log(LL_ERROR) << "[StoreFile] Failed to write chunk: " << e.what();
                        return Status(grpc::INTERNAL, "Failed to write data.");
                    }
                }
            }

            if (context->IsCancelled()) {
                lock_manager->Release(filename);
                dfs_log(LL_ERROR) << "[StoreFile] Request cancelled by the client.";
                return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client.");
            }

        } catch (const exception& e) {
            lock_manager->Release(filename);
            dfs_log(LL_ERROR) << "[StoreFile] Exception occurred: " << e.what();
            return Status(grpc::INTERNAL, "Exception occurred: " + std::string(e.what()));
        }
        dfs_log(LL_SYSINFO) << "[StoreFile] Finished : " << filename;

        response->set_success(true);
        response->set_file_name(filename);

        lock_manager->Release(filename);

        return Status::OK;
    }

    Status FetchFile(ServerContext* context, const FetchRequest* request,
                     ServerWriter<FetchResponse>* writer) override {
        string filename = request->file_name();
        string filepath = WrapPath(filename);
        uint32_t client_crc = request->crc();  // CRC provided by client

        dfs_log(LL_SYSINFO) << "[FetchFile] Attempting to fetch file: " << filepath;

        if (!storage_engine->Exists(filepath)) {
            dfs_log(LL_ERROR) << "[FetchFile] File not found: " << filepath;
            return Status(grpc::NOT_FOUND, "File not found.");
        }

        try {
            FetchResponse response;
            uint32_t server_crc = dfs_file_checksum(filepath, &crc_table);
            
            struct stat s = storage_engine->Stat(filepath);
            size_t file_size = s.st_size;
            size_t offset = 0;
            const size_t chunk_size = 4096;

            while (offset < file_size) {
                std::string chunk = storage_engine->Read(filepath, offset, chunk_size);
                if (chunk.empty()) break;

                response.set_chunk(chunk);
                response.set_file_name(filename);
                response.set_crc(server_crc);
                response.set_client_id(request->client_id());

                if (!writer->Write(response)) {
                    dfs_log(LL_ERROR) << "[FetchFile] Failed to send data to client: " << filename;
                    return Status(grpc::UNKNOWN, "Failed to send data to client.");
                }

                if (context->IsCancelled()) {
                    dfs_log(LL_ERROR) << "[FetchFile] Request cancelled by the client.";
                    return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client.");
                }
                
                offset += chunk.length();
            }

            dfs_log(LL_SYSINFO) << "[FetchFile] File fetch successful: " << filepath;
            return Status::OK;
        } catch (const std::exception& e) {
            dfs_log(LL_ERROR) << "[FetchFile] Exception occurred: " << e.what();
            return Status(grpc::INTERNAL, "Exception occurred during fetch.");
        }
    }

    Status GetFileStatus(ServerContext* context, const StatusRequest* request, FileInfo* response) override {
        string filename = request->file_name();
        string filepath = WrapPath(filename);

        try {
             struct stat fs = storage_engine->Stat(filepath);
             response->set_name(filename);
             response->set_size(fs.st_size);
             response->set_ctime(fs.st_ctime);
             response->set_mtime(fs.st_mtime);

             uint32_t server_crc = dfs_file_checksum(filepath, &crc_table);
             response->set_crc(server_crc);
             
             dfs_log(LL_SYSINFO) << "[GetFileStatus] File status retrieved for: " << filename;
             return Status::OK;
        } catch (const std::exception& e) {
             dfs_log(LL_ERROR) << "[GetFileStatus] Failed to get file attributes: " << e.what();
             return Status(grpc::NOT_FOUND, "File not found.");
        }
    }

    Status DeleteFile(ServerContext* context, const DeleteRequest* request, DeleteResponse* response) override {
        string filename = request->file_name();
        string client_id = request->client_id();
        string filepath = WrapPath(filename);

        dfs_log(LL_SYSINFO) << "[DeleteFile] Request to delete file: " << filepath;

        if (!storage_engine->Exists(filepath)) {
             dfs_log(LL_ERROR) << "[DeleteFile] File not found: " << filepath;
             lock_manager->Release(filename);
             return Status(grpc::NOT_FOUND, "File not found.");
        }

        if (context->IsCancelled()) {
            dfs_log(LL_ERROR) << "[DeleteFile] Request cancelled by the client or deadline exceeded.";
            lock_manager->Release(filename);
            return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client or deadline exceeded.");
        }

        try {
            storage_engine->Delete(filepath);
        } catch (const std::exception& e) {
             dfs_log(LL_ERROR) << "[DeleteFile] Failed to delete file: " << e.what();
             lock_manager->Release(filename);
             return Status(grpc::INTERNAL, "Failed to delete file.");
        }

        lock_manager->Release(filename);
        response->set_success(true);
        dfs_log(LL_SYSINFO) << "[DeleteFile] File deleted successfully: " << filepath;
        return Status::OK;
    }

    Status ListFiles(ServerContext* context, const FileListRequest* request, FileList* response) override {
        string directory_path = WrapPath("");

        dfs_log(LL_SYSINFO) << "[ListFiles] Attempting to list files in directory: " << directory_path;

        try {
            std::vector<std::string> files = storage_engine->List(directory_path);
            
            for (const auto& file_name : files) {
                context->AsyncNotifyWhenDone(NULL);

                if (context->IsCancelled()) {
                    dfs_log(LL_ERROR) << "[ListFiles] Request cancelled by the client or deadline exceeded.";
                    return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client or deadline exceeded.");
                }

                string file_path = directory_path + "/" + file_name;
                
                try {
                    struct stat file_stat = storage_engine->Stat(file_path);
                    if (S_ISDIR(file_stat.st_mode)) continue;

                    FileInfo* file_info = response->add_files();
                    file_info->set_name(file_name);
                    file_info->set_size(file_stat.st_size);
                    file_info->set_ctime(file_stat.st_ctime);
                    file_info->set_mtime(file_stat.st_mtime);

                    dfs_log(LL_DEBUG2) << "[ListFiles] Found file: " << file_name;
                } catch (...) {
                    continue; 
                }
            }
             dfs_log(LL_SYSINFO) << "[ListFiles] Finished listing files.";
             return Status::OK;
        } catch (const std::exception& e) {
             dfs_log(LL_ERROR) << "[ListFiles] Failed to list directory: " << e.what();
             return Status(grpc::INTERNAL, "Failed to open directory.");
        }
    }
};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure.
//
/**
 * The main server node constructor
 *
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string& server_address, const std::string& mount_path, int num_async_threads,
                             std::function<void()> callback)
    : server_address(server_address),
      mount_path(mount_path),
      num_async_threads(num_async_threads),
      grader_callback(callback) {}
/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept { dfs_log(LL_SYSINFO) << "DFSServerNode shutting down"; }

/**
 * Start the DFSServerNode server
 */
void DFSServerNode::Start() {
    // Inject Dependencies
    auto storage_engine = std::make_shared<dfs::storage::PosixStorageEngine>();
    auto lock_manager = std::make_shared<dfs::LockManager>();
    
    DFSServiceImpl service(this->mount_path, this->server_address, this->num_async_threads, 
                           storage_engine, lock_manager);

    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    service.Run();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional definitions here
//
