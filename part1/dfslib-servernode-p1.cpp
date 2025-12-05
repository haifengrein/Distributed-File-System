#include <map>
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


#include "src/dfs-utils.h"
#include "dfslib-shared-p1.h"
#include "dfslib-servernode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Server;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::ServerWriter;
using grpc::ServerContext;
using grpc::ServerBuilder;

using namespace dfs_service;
using namespace std;

using google::protobuf::util::TimeUtil;
using google::protobuf::Timestamp;

//
// STUDENT INSTRUCTION:
//
// DFSServiceImpl is the implementation service for the rpc methods
// and message types you defined in the `dfs-service.proto` file.
//
// You should add your definition overrides here for the specific
// methods that you created for your GRPC service protocol. The
// gRPC tutorial described in the readme is a good place to get started
// when trying to understand how to implement this class.
//
// The method signatures generated can be found in `proto-src/dfs-service.grpc.pb.h` file.
//
// Look for the following section:
//
//      class Service : public ::grpc::Service {
//
// The methods returning grpc::Status are the methods you'll want to override.
//
// In C++, you'll want to use the `override` directive as well. For example,
// if you have a service method named MyMethod that takes a MyMessageType
// and a ServerWriter, you'll want to override it similar to the following:
//
//      Status MyMethod(ServerContext* context,
//                      const MyMessageType* request,
//                      ServerWriter<MySegmentType> *writer) override {
//
//          /** code implementation here **/
//      }
//
class DFSServiceImpl final : public DFSService::Service {

private:

    /** The mount path for the server **/
    std::string mount_path;

    /**
     * Prepend the mount path to the filename.
     *
     * @param filepath
     * @return
     */
    const std::string WrapPath(const std::string &filepath) {
        return this->mount_path + filepath;
    }

    int getStat(const string file_path, FileInfo* fs) { 
        struct stat sys_stat;
        if (stat(file_path.c_str(), &sys_stat) != 0){ 
            return -1;
        }
        Timestamp* creation_time = new Timestamp(TimeUtil::TimeTToTimestamp(sys_stat.st_ctime));
        Timestamp* mod_time = new Timestamp(TimeUtil::TimeTToTimestamp(sys_stat.st_mtime)); 
        fs->set_name(file_path);
        fs->set_size(sys_stat.st_size);
        fs->set_allocated_creation_time(creation_time); 
        fs->set_allocated_modified_time(mod_time); 
        return 0;}


public:

    DFSServiceImpl(const std::string &mount_path): mount_path(mount_path) {
    }

    ~DFSServiceImpl() {}

    //
    // STUDENT INSTRUCTION:
    //
    // Add your additional code here, including
    // implementations of your protocol service methods
    //
    Status StoreFile (ServerContext* context,
                    ServerReader<StoreRequest>* reader, 
                    StoreResponse* response ) override {
        StoreRequest request;
        ofstream ofs_obj;
        string filename, filepath;
        FileInfo fileinfo;

        bool isFirstChunk = true;

        try {
            while(reader->Read(&request)) {
                if (isFirstChunk) {
                    filename = request.filename();
                    filepath = WrapPath(filename);
                    ofs_obj.open(filepath, ios::trunc); // todo： ios::binary
                    dfs_log(LL_SYSINFO) << "[StoreFile] FirstChunk received, Writing date chunk to file: " << filename;
                    if (!ofs_obj.is_open()) {
                        int err = errno; 
                        const char* errMsg = strerror(err); 
                        dfs_log(LL_ERROR) << "[StoreFile] Failed to open file: " << filepath << ", Error: " << errMsg;
                        return Status(grpc::INTERNAL, "Failed to open file for writing. Error: " + std::string(errMsg));
                        
                }
                isFirstChunk = false;
                dfs_log(LL_SYSINFO) << "[StoreFile] Opened file for writing: " << filepath;
            }

            ofs_obj.write(request.chunk().data(), request.chunk().size());
            //const string chunk = request.chunk(); 
            //ofs_obj << chunk;

            if (ofs_obj.fail()) {
                dfs_log(LL_ERROR) << "[StoreFile] Failed to write data to file: " << filename;
                return Status(grpc::INTERNAL, "Failed to write data to file.");
            }
            //dfs_log(LL_SYSINFO) << "[StoreFile] Wrote chunk to file: " << filename;
            
            if (context->IsCancelled()) {
                dfs_log(LL_ERROR) << "[StoreFile] Request cancelled by the client.";
                return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client.");
            }

        }
    } catch (const std::exception& e) {
        dfs_log(LL_ERROR) << "[StoreFile] Exception occurred: " << e.what();
        return Status(grpc::INTERNAL, "Exception occurred: " + std::string(e.what()));
    }

    dfs_log(LL_SYSINFO) << "[StoreFile] Finished : " << filename;
    ofs_obj.close(); 


    struct stat fs;
    
    if (stat(WrapPath(filename).c_str(), &fs) != 0) {
        dfs_log(LL_ERROR) << "[StoreFile] Failed to get file info: " << filename;
        return Status(grpc::INTERNAL, "Failed to get file info after write.");
    }
    Timestamp* creation_time = new Timestamp(TimeUtil::TimeTToTimestamp(fs.st_ctime));
    Timestamp* mod_time = new Timestamp(TimeUtil::TimeTToTimestamp(fs.st_mtime)); 
    fileinfo.set_name(filepath);
    fileinfo.set_size(fs.st_size);
    fileinfo.set_allocated_creation_time(creation_time); 
    fileinfo.set_allocated_modified_time(mod_time); 
    
    response->set_success(true);
    response->set_filename(filename);
    Timestamp* mtime = new Timestamp(fileinfo.modified_time());
    response->set_allocated_modified_time(mtime);
    return Status::OK;

    }


    Status FetchFile(ServerContext* context, const FetchRequest* request,
                 ServerWriter<FetchResponse>* writer) override {
        string filename = request->filename();
        string filepath = WrapPath(filename);
        ifstream file_stream(filepath);

        dfs_log(LL_SYSINFO) << "[FetchFile] Attempting to fetch file: " << filepath;

        if (!file_stream.is_open()) {
            int err = errno;
            const char* errMsg = strerror(err);
            dfs_log(LL_ERROR) << "[FetchFile] Failed to open file: " << filepath << ", Error: " << errMsg;
            return Status(grpc::NOT_FOUND, "File not found. Error: " + std::string(errMsg));
        }

        try {
            FetchResponse response;
            char buffer[4096];

            while (!file_stream.eof()) {
            file_stream.read(buffer, sizeof(buffer));
            std::streamsize bytes_read = file_stream.gcount();

    
            if (file_stream.fail() && !file_stream.eof()) {
                dfs_log(LL_ERROR) << "[FetchFile] Failed to read data from file: " << filepath;
                file_stream.close();
                return Status(grpc::INTERNAL, "Failed to read data from file.");
            }
            
            response.set_chunk(buffer, bytes_read);
            
            if (!writer->Write(response)) {
                dfs_log(LL_ERROR) << "[FetchFile] Failed to send chunk to client for file: " << filepath;
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
        dfs_log(LL_ERROR) << "[FetchFile] Exception occurred while fetching file: " << e.what();
        return Status(grpc::INTERNAL, "Exception occurred while fetching file.");
    }
}

    Status DeleteFile(ServerContext* context, const DeleteRequest* request,
                DeleteResponse* response) override {

        string filename = request->filename();
        string filepath = WrapPath(filename);
        
        dfs_log(LL_SYSINFO) << "[DeleteFile] Attempting to delete file: " << filepath;
        
    
        struct stat file_stat;
        if (stat(filepath.c_str(), &file_stat) != 0) {
            int err = errno;
            const char* errMsg = strerror(err);
            dfs_log(LL_ERROR) << "[DeleteFile] File not found: " << filepath << ", Error: " << errMsg;
            return Status(grpc::NOT_FOUND, "File not found. Error: " + std::string(errMsg));
        }

        if (context->IsCancelled()) {
            dfs_log(LL_ERROR) << "[DeleteFile] Request cancelled by the client or deadline exceeded.";
            return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client or deadline exceeded.");
        }

        if (remove(filepath.c_str()) != 0) {
            int err = errno;
            const char* errMsg = strerror(err);
            dfs_log(LL_ERROR) << "[DeleteFile] Failed to delete file: " << filepath << ", Error: " << errMsg;
            return Status(grpc::INTERNAL, "Failed to delete file. Error: " + std::string(errMsg));
        }
        
        response->set_success(true);
        dfs_log(LL_SYSINFO) << "[DeleteFile] File deleted successfully: " << filepath;
        
        return Status::OK;
    }



    Status ListFiles(ServerContext* context, const ListRequest* request, ListResponse* response) override {
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
            Timestamp* creation_time = new Timestamp(TimeUtil::TimeTToTimestamp(file_stat.st_ctime));
            Timestamp* modified_time = new Timestamp(TimeUtil::TimeTToTimestamp(file_stat.st_mtime));
            file_info->set_allocated_creation_time(creation_time);
            file_info->set_allocated_modified_time(modified_time);

            
            dfs_log(LL_SYSINFO) << "[ListFiles] Found file: " << file_name;
        }

        closedir(dir);

        dfs_log(LL_SYSINFO) << "[ListFiles] Finished listing files.";

        return Status::OK;
    }

    Status GetAttrs( ServerContext* context, const AttrsRequest* request,
            AttrsResponse* response ) override {
        string filename = request->filename();
        string filepath = WrapPath(filename);

        struct stat file_stat;
        if (stat(filepath.c_str(), &file_stat) != 0) {
            dfs_log(LL_ERROR) << "[GetAttrs] Failed to get file attributes: " << filepath;
            return Status(grpc::NOT_FOUND, "File not found.");
        }

        if (context->IsCancelled()) {
            dfs_log(LL_ERROR) << "[GetAttrs] Request cancelled by the client.";
            return Status(grpc::DEADLINE_EXCEEDED, "Request cancelled by the client.");
        }

        FileInfo* info = response->mutable_info();
        info->set_name(filename);
        info->set_size(file_stat.st_size);
        info->mutable_modified_time()->set_seconds(file_stat.st_mtime);
        info->mutable_creation_time()->set_seconds(file_stat.st_ctime);

        dfs_log(LL_SYSINFO) << "[GetAttrs] File attributes retrieved for: " << filepath;
        return Status::OK;
    }

};




    
//
// STUDENT INSTRUCTION:
//
// The following three methods are part of the basic DFSServerNode
// structure. You may add additional methods or change these slightly,
// but be aware that the testing environment is expecting these three
// methods as-is.
//
/**
 * The main server node constructor
 *
 * @param server_address
 * @param mount_path
 */
DFSServerNode::DFSServerNode(const std::string &server_address,
        const std::string &mount_path,
        std::function<void()> callback) :
    server_address(server_address), mount_path(mount_path), grader_callback(callback) {}

/**
 * Server shutdown
 */
DFSServerNode::~DFSServerNode() noexcept {
    dfs_log(LL_SYSINFO) << "DFSServerNode shutting down";
    this->server->Shutdown();
}

/** Server start **/
void DFSServerNode::Start() {
    DFSServiceImpl service(this->mount_path);
    ServerBuilder builder;
    builder.AddListeningPort(this->server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    this->server = builder.BuildAndStart();
    dfs_log(LL_SYSINFO) << "DFSServerNode server listening on " << this->server_address;
    this->server->Wait();
}

//
// STUDENT INSTRUCTION:
//
// Add your additional DFSServerNode definitions here
//
