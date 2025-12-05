#include <map>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <grpcpp/grpcpp.h>
#include <google/protobuf/util/time_util.h>
#include <utime.h>

#include "proto-src/dfs-service.grpc.pb.h"
#include "src/dfslibx-call-data.h"
#include "src/dfslibx-service-runner.h"
#include "dfslib-shared-p2.h"
#include "dfslib-servernode-p2.h"

using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;

using dfs_service::DFSService;
using namespace dfs_service;
using namespace std;

using google::protobuf::util::TimeUtil;
using google::protobuf::Timestamp;


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

//
// STUDENT INSTRUCTION:
//
// As with Part 1, the DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in your `dfs-service.proto` file.
//
// You may start with your Part 1 implementations of each service method.
//
// Elements to consider for Part 2:
//
// - How will you implement the write lock at the server level?
// - How will you keep track of which client has a write lock for a file?
//      - Note that we've provided a preset client_id in DFSClientNode that generates
//        a client id for you. You can pass that to the server to identify the current client.
// - How will you release the write lock?
// - How will you handle a store request for a client that doesn't have a write lock?
// - When matching files to determine similarity, you should use the `file_checksum` method we've provided.
//      - Both the client and server have a pre-made `crc_table` variable to speed things up.
//      - Use the `file_checksum` method to compare two files, similar to the following:
//
//          std::uint32_t server_crc = dfs_file_checksum(filepath, &this->crc_table);
//
//      - Hint: as the crc checksum is a simple integer, you can pass it around inside your message types.
//
class DFSServiceImpl final :
    public DFSService::WithAsyncMethod_CallbackList<DFSService::Service>,
        public DFSCallDataManager<FileRequestType , FileListResponseType> {

private:

    /** The runner service used to start the service and manage asynchronicity **/
    DFSServiceRunner<FileRequestType, FileListResponseType> runner;

    /** The mount path for the server **/
    std::string mount_path;

    /** Mutex for managing the queue requests **/
    std::mutex queue_mutex;

    /** The vector of queued tags used to manage asynchronous requests **/
    std::vector<QueueRequest<FileRequestType, FileListResponseType>> queued_tags;


    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }

    /** CRC Table kept in memory for faster calculations **/
    CRC::Table<std::uint32_t, 32> crc_table;

    
    map<string, string>file_locks;
    mutex lock_mutex;


    bool IsFileLocked(const string & file_name, const std::string &client_id) {

        dfs_log(LL_DEBUG2) << "Checking if file is locked: " << file_name;

        auto lock_iter = file_locks.find(file_name);
        if (lock_iter == file_locks.end()) {
            dfs_log(LL_DEBUG2) << "File is not locked.";
            return false;
        }
        if (lock_iter->second == client_id) {
            dfs_log(LL_DEBUG2) << "File is already locked by this client.";
            return false;
        }
        dfs_log(LL_DEBUG2) << "File is locked by another client.";
        return true;
    }


    bool ObtainFileLock(const string &file_name, const string &client_id) {
        lock_guard<std::mutex> gurad(lock_mutex);
        dfs_log(LL_DEBUG2) << "Attempting to lock file: " << file_name;

        if (!IsFileLocked(file_name, client_id)) {
            file_locks[file_name] = client_id;
            dfs_log(LL_DEBUG2) << "File locked successfully: " << file_name;
            return true;
        }
        dfs_log(LL_DEBUG2) << "Failed to lock file (already locked by another client): " << file_name;
        return false;
    }

    bool ReleaseFileLock(const string &file_name) {
        lock_guard<std::mutex> guard (lock_mutex);
        dfs_log(LL_DEBUG2) << "Releasing lock for file: " << file_name;

        if (file_locks.erase(file_name) > 0) {
            dfs_log(LL_SYSINFO) << "File lock released: " << file_name;
            return true;
        }
        dfs_log(LL_DEBUG2) << "File was not locked: " << file_name;
        return false;
    }


public:

    DFSServiceImpl(const std::string& mount_path, const std::string& server_address, int num_async_threads):
        mount_path(mount_path), crc_table(CRC::CRC_32()) {

        this->runner.SetService(this);
        this->runner.SetAddress(server_address);
        this->runner.SetNumThreads(num_async_threads);
        this->runner.SetQueuedRequestsCallback([&]{ this->ProcessQueuedRequests(); });

    }

    ~DFSServiceImpl() {
        this->runner.Shutdown();
    }

    void Run() {
        this->runner.Run();
    }

    /**
     * Request callback for asynchronous requests
     *
     * This method is called by the DFSCallData class during
     * an asynchronous request call from the client.
     *
     * Students should not need to adjust this.
     *
     * @param context
     * @param request
     * @param response
     * @param cq
     * @param tag
     */
    void RequestCallback(grpc::ServerContext* context,
                         FileRequestType* request,
                         grpc::ServerAsyncResponseWriter<FileListResponseType>* response,
                         grpc::ServerCompletionQueue* cq,
                         void* tag) {

        std::lock_guard<std::mutex> lock(queue_mutex);
        this->queued_tags.emplace_back(context, request, response, cq, tag);

    }

    /**
     * Process a callback request
     *
     * This method is called by the DFSCallData class when
     * a requested callback can be processed. You should use this method
     * to manage the CallbackList RPC call and respond as needed.
     *
     * See the STUDENT INSTRUCTION for more details.
     *
     * @param context
     * @param request
     * @param response
     */

    void ProcessCallback(ServerContext* context, FileRequestType* request, FileListResponseType* response) {

        //
        // STUDENT INSTRUCTION:
        //
        // You should add your code here to respond to any CallbackList requests from a client.
        // This function is called each time an asynchronous request is made from the client.
        //
        // The client should receive a list of files or modifications that represent the changes this service
        // is aware of. The client will then need to make the appropriate calls based on those changes.
        //

        dfs_log(LL_SYSINFO) << "Enter ProcessCallback";

        FileListRequest dummy_request;

        Status status = this->ListFiles(context, &dummy_request, response);

        if(!status.ok()){
            dfs_log(LL_ERROR) << "ProcessCallback Failed";
            return;
        }
    }

    /**
     * Processes the queued requests in the queue thread
     */
    void ProcessQueuedRequests() {
        while(true) {

            //
            // STUDENT INSTRUCTION:
            //
            // You should add any synchronization mechanisms you may need here in
            // addition to the queue management. For example, modified files checks.
            //
            // Note: you will need to leave the basic queue structure as-is, but you
            // may add any additional code you feel is necessary.
            //


            // Guarded section for queue
            {
                dfs_log(LL_DEBUG2) << "Waiting for queue guard";
                std::lock_guard<std::mutex> lock(queue_mutex);


                for(QueueRequest<FileRequestType, FileListResponseType>& queue_request : this->queued_tags) {
                    this->RequestCallbackList(queue_request.context, queue_request.request,
                        queue_request.response, queue_request.cq, queue_request.cq, queue_request.tag);
                    queue_request.finished = true;
                }

                // any finished tags first
                this->queued_tags.erase(std::remove_if(
                    this->queued_tags.begin(),
                    this->queued_tags.end(),
                    [](QueueRequest<FileRequestType, FileListResponseType>& queue_request) { return queue_request.finished; }
                ), this->queued_tags.end());

            }
        }
    }

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // the implementations of your rpc protocol methods.
    //

    Status RequestWriteLock(ServerContext* context, const WriteLockRequest* request, 
                            WriteLockResponse* response) override {

        dfs_log(LL_DEBUG2) << "[RequestWriteLock] Received WriteLock request for file: " << request->filename();
        if (context->IsCancelled()) {
        dfs_log(LL_ERROR) << "[RequestWriteLock] Request cancelled by the client or deadline exceeded.";
            return ::grpc::Status(StatusCode::DEADLINE_EXCEEDED, "Request cancelled or deadline exceeded.");
        }

        string file_name = request->filename();
        string client_id = request->client_id();

        bool lock_obtained = ObtainFileLock(request->filename(), request->client_id());
        
        if (lock_obtained) {
            dfs_log(LL_SYSINFO) << "[RequestWriteLock] Write lock granted for file: " << file_name << " and Client: " << client_id;
            response->set_success(true);
            return Status::OK;
        } else {
            dfs_log(LL_ERROR) << "[RequestWriteLock] File: " << file_name << " is already locked by another client.";
            response->set_success(false);
            return Status(StatusCode::RESOURCE_EXHAUSTED, "File is locked by another client.");
        }

        dfs_log(LL_ERROR) << "[RequestWriteLock] Failed to obtain lock for file: " << file_name;
        response->set_success(false);
        response->set_message("Failed to obtain lock for file.");
        return Status(StatusCode::INTERNAL, "Failed to obtain lock for file.");
    }

    Status StoreFile(ServerContext* context,
                 ServerReader<StoreRequest>* reader,
                 StoreResponse* response) override {

        StoreRequest request;
        ofstream ofs_obj;
        string filename, filepath;
        int client_file_mtime;

        bool isFirstChunk = true;
        uint32_t client_crc = 0; 

        dfs_log(LL_SYSINFO) << "[StoreFile] Starting to process file: " << filename;

        try {
            while(reader->Read(&request)) {
                if (isFirstChunk) {
                    filename = request.file_name();
                    client_crc = request.crc(); 
                    client_file_mtime = request.mtime();
                    filepath = WrapPath(filename);
                    dfs_log(LL_DEBUG2) << "[StoreFile] FirstChunk received, Writing date chunk to file: " << filename;

                    uint32_t server_crc = dfs_file_checksum(filepath, &crc_table);
                    if (server_crc == client_crc) {
                        dfs_log(LL_SYSINFO) << "[StoreFile] Content is same, updating mtime: " << filename;
                        struct utimbuf mtime;
                        mtime.modtime = client_file_mtime;
                        utime(filepath.c_str(), &mtime);
                        ReleaseFileLock(filename); 
                        return Status(StatusCode::ALREADY_EXISTS, "File on server is identical to client's version.");
                    }
                    
                    ofs_obj.open(filepath, ios::trunc); 
                    if (!ofs_obj.is_open()) {
                        int err = errno;
                        const char* errMsg = strerror(err);
                        ReleaseFileLock(filename); 
                        dfs_log(LL_ERROR) << "[StoreFile] Failed to open file: " << filepath << ", Error: " << errMsg;
                        return Status(grpc::INTERNAL, "Failed to open file for writing. Error: " + string(errMsg));
                    }
                    isFirstChunk = false;
                }

                ofs_obj.write(request.chunk().data(), request.chunk().size());
                if (ofs_obj.fail()) {
                    ReleaseFileLock(filename); 
                    dfs_log(LL_ERROR) << "[StoreFile] Failed to write data to file: " << filename;
                    return Status(grpc::INTERNAL, "Failed to write data to file.");
                }
            }

            if (context->IsCancelled()) {
                ReleaseFileLock(filename); 
                dfs_log(LL_ERROR) << "[StoreFile] Request cancelled by the client.";
                return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client.");
            }

        } catch (const exception& e) {
            ReleaseFileLock(filename); // Release lock due to exception
            dfs_log(LL_ERROR) << "[StoreFile] Exception occurred: " << e.what();
            return Status(grpc::INTERNAL, "Exception occurred: " + std::string(e.what()));
        }
        dfs_log(LL_SYSINFO) << "[StoreFile] Finished : " << filename;
        ofs_obj.close();
        

        struct stat fs;
        if (stat(filepath.c_str(), &fs) != 0) {
            dfs_log(LL_ERROR) << "[StoreFile] Failed to get file info: " << filename;
            return Status(grpc::INTERNAL, "Failed to get file info after write.");
        }

        
        response->set_success(true);
        response->set_file_name(filename);

        ReleaseFileLock(filename); 
        
        return Status::OK;
    }
        
        Status FetchFile(ServerContext* context, const FetchRequest* request,
                    ServerWriter<FetchResponse>* writer) override {

            string filename = request->file_name();
            string filepath = WrapPath(filename);
            ifstream file_stream(filepath, ios::binary);
            uint32_t client_crc = request->crc(); // CRC provided by client
            printf("This is crc: %d", client_crc);

            dfs_log(LL_SYSINFO) << "[FetchFile] Attempting to fetch file: " << filepath;

            if (!file_stream.is_open()) {
                dfs_log(LL_ERROR) << "[FetchFile] File not found: " << filepath;
                return Status(grpc::NOT_FOUND, "File not found.");
            }

            try {

                FetchResponse response;
                char buffer[4096];
                uint32_t server_crc = dfs_file_checksum(filepath, &crc_table);

                while (!file_stream.eof()) {
                    file_stream.read(buffer, sizeof(buffer));
                    int bytes_read = file_stream.gcount();
                    response.set_chunk(buffer, bytes_read);
                    response.set_file_name(filename);
                    response.set_crc(server_crc);
                    printf("This is crc: %d", server_crc);
                    response.set_client_id(request->client_id());

                    if (!writer->Write(response)) {
                        dfs_log(LL_ERROR) << "[FetchFile] Failed to send data to client: " << filename;
                       
                        return Status(grpc::UNKNOWN, "Failed to send data to client.");
                    }

                    if (context->IsCancelled()) {
                        dfs_log(LL_ERROR) << "[FetchFile] Request cancelled by the client.";
                    
                        return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client.");
                    }
                }

                file_stream.close();
                
                dfs_log(LL_SYSINFO) << "[FetchFile] File fetch successful: " << filepath;
                return Status::OK;
            } catch (const std::exception& e) {
                
                dfs_log(LL_ERROR) << "[FetchFile] Exception occurred: " << e.what();
                return Status(grpc::INTERNAL, "Exception occurred during fetch.");
            }
    }

    Status GetFileStatus(ServerContext* context, 
            const StatusRequest* request, 
            FileInfo* response) override {

        string filename = request->file_name();
        string filepath = WrapPath(filename);
        
        struct stat fs;
        if (stat(filepath.c_str(), &fs) != 0) {
            dfs_log(LL_ERROR) << "[GetFileStatus] Failed to get file attributes: " << filepath;
            
            return Status(grpc::NOT_FOUND, "File not found.");
        }

        if (context->IsCancelled()) {
            dfs_log(LL_ERROR) << "[GetFileStatus] Request cancelled by the client.";
            
            return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client.");
        }

        response->set_name(filename);
        response->set_size(fs.st_size);
        response->set_ctime(fs.st_ctime);
        response->set_mtime(fs.st_mtime);

        uint32_t server_crc = dfs_file_checksum(filepath, &crc_table);
        response->set_crc(server_crc);
        
        dfs_log(LL_SYSINFO) << "[GetFileStatus] File status retrieved for: " << filename;
        return Status::OK;
        }


    Status DeleteFile(ServerContext* context, 
        const DeleteRequest* request, 
        DeleteResponse* response) override {

        string filename = request->file_name();
        string client_id = request->client_id();
        string filepath = WrapPath(filename);

        dfs_log(LL_SYSINFO) << "[DeleteFile] Request to delete file: " << filepath;

        struct stat file_stat;
        if (stat(filepath.c_str(), &file_stat) != 0) {
            int err = errno;
            const char* errMsg = strerror(err);
            dfs_log(LL_ERROR) << "[DeleteFile] File not found: " << filepath << ", Error: " << errMsg;
            ReleaseFileLock(filename);
            return Status(grpc::NOT_FOUND, "File not found. Error: " + std::string(errMsg));
        }


        if (context->IsCancelled()) {
            dfs_log(LL_ERROR) << "[DeleteFile] Request cancelled by the client or deadline exceeded.";
            ReleaseFileLock(filename);
            return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client or deadline exceeded.");
        }

        if (remove(filepath.c_str()) != 0) {
            int err = errno;
            const char* errMsg = strerror(err);
            dfs_log(LL_ERROR) << "[DeleteFile] Failed to delete file: " << filepath << ", Error: " << errMsg;
            ReleaseFileLock(filename);
            return Status(grpc::INTERNAL, "Failed to delete file. Error: " + std::string(errMsg));
        }

        ReleaseFileLock(filename);
        response->set_success(true);
        dfs_log(LL_SYSINFO) << "[DeleteFile] File deleted successfully: " << filepath;
        return Status::OK;
    }

    Status ListFiles(ServerContext* context, const FileListRequest* request, FileList* response) override {
        string directory_path = WrapPath("");

        
        dfs_log(LL_SYSINFO) << "[ListFiles] Attempting to list files in directory: " << directory_path;

        
        DIR* dir = opendir(directory_path.c_str());
        if (dir == nullptr) {
            int err = errno;
            const char* errMsg = strerror(err);
            dfs_log(LL_ERROR) << "[ListFiles] Failed to open directory: " << directory_path << ", Error: " << errMsg;
            return Status(grpc::INTERNAL, "Failed to open directory. Error: " + std::string(errMsg));
        }

        dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {

            context->AsyncNotifyWhenDone(NULL);
            
            if (context->IsCancelled()) {
                dfs_log(LL_ERROR) << "[ListFiles] Request cancelled by the client or deadline exceeded.";
                closedir(dir);
                return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client or deadline exceeded.");
            }

            // Skip directories
            if (entry->d_type == DT_DIR) {
                continue;
            }

            string file_name = entry->d_name;
            string file_path = directory_path + "/" + file_name;

            struct stat file_stat;
            if (stat(file_path.c_str(), &file_stat) != 0) {
                dfs_log(LL_ERROR) << "[ListFiles] Failed to get stats for file: " << file_name;
                continue; 
            }

            
            FileInfo* file_info = response->add_files();
            file_info->set_name(file_name);
            file_info->set_size(file_stat.st_size);
            file_info->set_ctime(file_stat.st_ctime);
            file_info->set_mtime(file_stat.st_mtime);

            dfs_log(LL_DEBUG2) << "[ListFiles] Found file: " << file_name;
        }

        closedir(dir);

        dfs_log(LL_SYSINFO) << "[ListFiles] Finished listing files.";

        return Status::OK;
    }


};

//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly
// to add additional startup/shutdown routines inside, but be aware that
// the basic structure should stay the same as the testing environment
// will be expected this structure.
//
/**
 * The main server node constructor
 *
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        int num_async_threads,
        std::function<void()> callback) :
        server_address(server_address),
        mount_path(mount_path),
        num_async_threads(num_async_threads),
        grader_callback(callback) {}
/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
}

/**
 * Start the DFSServerNode server
 */
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path, this->server_address, this->num_async_threads);


    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    service.Run();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional definitions here
//
