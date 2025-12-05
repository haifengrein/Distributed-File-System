#include <regex>
#include <vector>
#include <string>
#include <thread>
#include <cstdio>
#include <chrono>
#include <errno.h>
#include <csignal>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <sys/inotify.h>
#include <grpcpp/grpcpp.h>
#include <google/protobuf/util/time_util.h>

#include "dfslib-shared-p1.h"
#include "dfslib-clientnode-p1.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;

using namespace dfs_service;
using namespace std;
using std::chrono::system_clock;
using std::chrono::milliseconds;
using google::protobuf::util::TimeUtil;

//
// STUDENT INSTRUCTION:
//
// You may want to add aliases to your namespaced service methods here.
// All of the methods will be under the `dfs_service` namespace.
//
// For example, if you have a method named MyMethod, add
// the following:
//
//      using dfs_service::MyMethod
//


DFSClientNodeP1::DFSClientNodeP1() : DFSClientNode() {}

DFSClientNodeP1::~DFSClientNodeP1() noexcept {}

StatusCode DFSClientNodeP1::Store(const std::string &filename) {
    ClientContext context;
    StoreResponse response;
    unique_ptr<ClientWriter<StoreRequest>> writer(service_stub->StoreFile(&context, &response));

    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));

    string file_path = WrapPath(filename);

    // struct stat fs;
    // if (stat(file_path.c_str(), &fs) == -1) {
    //     dfs_log(LL_ERROR) << "[Store] File not found: " << file_path;
    //     return StatusCode::NOT_FOUND;
    // }

    //streamsize file_size = fs.st_size;

    ifstream ifs(file_path);
    if (!ifs.is_open()) {
        dfs_log(LL_ERROR) << "[Store] File not found: " << file_path;
        return StatusCode::NOT_FOUND;
    }


    StoreRequest request;
    char buffer[4096]; 
    bool isFirstChunk = true;

    try {
        while (!ifs.eof()) {
            //int bytes_to_read = min(file_size, static_cast<streamsize>(sizeof(buffer)));
            ifs.read(buffer, sizeof(buffer));
            int bytes_read = ifs.gcount();
            //file_size -= bytes_read;

            if (isFirstChunk) {
                request.set_filename(filename); 
                isFirstChunk = false;
            }
            request.set_chunk(buffer, bytes_read); 

            if (!writer->Write(request)) {
                dfs_log(LL_ERROR) << "[Store] Write failed for file: " << file_path;
                return StatusCode::CANCELLED;
            }
        }
        writer->WritesDone();
        Status status = writer->Finish();
        
        if (status.ok()) {
            dfs_log(LL_SYSINFO) << "[Store] File stored successfully: " << file_path;
            return StatusCode::OK;
        } else {
            dfs_log(LL_ERROR) << "[Store] Store failed: " << status.error_message();
            return status.error_code() == grpc::DEADLINE_EXCEEDED ? StatusCode::DEADLINE_EXCEEDED : StatusCode::CANCELLED;
        }
    } catch (const std::exception &e) {
        // Log the exception and handle it accordingly
        dfs_log(LL_ERROR) << "[Store] Exception occurred: " << e.what();
        return StatusCode::CANCELLED;
    }
}



StatusCode DFSClientNodeP1::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. This method should
    // connect to your gRPC service implementation method
    // that can accept a file request and return the contents
    // of a file from the service.
    //
    // As with the store function, you'll need to stream the
    // contents, so consider the use of gRPC's ClientReader.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //
    ClientContext context;
    FetchRequest request;
    request.set_filename(filename);
    FetchResponse response;
    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));

    unique_ptr<ClientReader<FetchResponse>> reader(service_stub->FetchFile(&context, request));

    string file_path = WrapPath(filename);
    ofstream ofs(file_path);
    
    if (!ofs.is_open()) {
        dfs_log(LL_ERROR) << "[Fetch] Failed to open file: " << file_path;
        return StatusCode::NOT_FOUND;
    }

    try {
        while (reader->Read(&response)) {
            ofs.write(response.chunk().data(), response.chunk().size());
            if (ofs.fail()) {
                dfs_log(LL_ERROR) << "[Fetch] Failed to write data to file: " << file_path;
                return StatusCode::INTERNAL;
            }
        }

        Status status = reader->Finish();

        if (status.ok()) {
            dfs_log(LL_SYSINFO) << "[Fetch] File fetched successfully: " << file_path;
            return StatusCode::OK;
        } else {
            dfs_log(LL_ERROR) << "[Fetch] Fetch failed: " << status.error_message();
            return status.error_code() == grpc::StatusCode::NOT_FOUND ? StatusCode::NOT_FOUND
             : status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ? StatusCode::DEADLINE_EXCEEDED
             : StatusCode::CANCELLED;
        }
    } catch (const std::exception &e) {
        dfs_log(LL_ERROR) << "[Fetch] Exception occurred: " << e.what();
        return StatusCode::CANCELLED;
    }
}


StatusCode DFSClientNodeP1::Delete(const std::string& filename) {

    ClientContext context;
    DeleteRequest request;
    DeleteResponse response;
    
    context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(deadline_timeout));

    request.set_filename(filename);

    Status status = service_stub->DeleteFile(&context, request, &response);

    if (status.ok()) {
        if (response.success()) {
            
            dfs_log(LL_SYSINFO) << "[Delete] File deleted successfully: " << filename;
            return StatusCode::OK;
        } else {
            
            dfs_log(LL_ERROR) << "[Delete] Failed to delete file: " << filename;
            return StatusCode::UNKNOWN;
        }
    } else {
        
        if (status.error_code() == grpc::DEADLINE_EXCEEDED) {
            
            dfs_log(LL_ERROR) << "[Delete] Deadline exceeded for file: " << filename;
            return StatusCode::DEADLINE_EXCEEDED;
        } else if (status.error_code() == grpc::NOT_FOUND) {
            
            dfs_log(LL_ERROR) << "[Delete] File not found: " << filename;
            return StatusCode::NOT_FOUND;
        } else {
            
            dfs_log(LL_ERROR) << "[Delete] Error occurred: " << status.error_message();
            return StatusCode::CANCELLED;
        }
    }
}
StatusCode DFSClientNodeP1::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list all files here. This method
    // should connect to your service's list method and return
    // a list of files using the message type you created.
    //
    // The file_map parameter is a simple map of files. You should fill
    // the file_map with the list of files you receive with keys as the
    // file name and values as the modified time (mtime) of the file
    // received from the server.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::CANCELLED otherwise
    //
    //
    file_map->clear();

    ClientContext context;
    ListRequest request;
    ListResponse response;

   
    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));

    
    Status status = service_stub->ListFiles(&context, request, &response);

    if (status.ok()) {
        
        for (const auto& file_info : response.files()) {
            
            int64_t modified_time = google::protobuf::util::TimeUtil::TimestampToSeconds(file_info.modified_time());
            
            
            (*file_map)[file_info.name()] = modified_time;

            dfs_log(LL_SYSINFO) << "Listed file: " << file_info.name() << ", Modified Time: " << modified_time;
        }
        return StatusCode::OK;
    } else {
        
        dfs_log(LL_ERROR) << "Failed to list files: " << status.error_message();
        return status.error_code() == grpc::DEADLINE_EXCEEDED ? StatusCode::DEADLINE_EXCEEDED : StatusCode::CANCELLED;
    }

    
}

StatusCode DFSClientNodeP1::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. This method should
    // retrieve the status of a file on the server. Note that you won't be
    // tested on this method, but you will most likely find that you need
    // a way to get the status of a file in order to synchronize later.
    //
    // The status might include data such as name, size, mtime, crc, etc.
    //
    // The file_status is left as a void* so that you can use it to pass
    // a message type that you defined. For instance, may want to use that message
    // type after calling Stat from another method.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::NOT_FOUND - if the file cannot be found on the server
    // StatusCode::CANCELLED otherwise
    //
    //

    ClientContext context;
    AttrsRequest request;
    AttrsResponse response;

    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));
    request.set_filename(filename);

    
    Status status = service_stub->GetAttrs(&context, request, &response);

    
    if (status.ok()) {
        FileInfo info = response.info();
        dfs_log(LL_SYSINFO) << "[Stat] File info: Name=" << info.name() 
                           << ", Size=" << info.size() 
                           << ", Modified Time=" << info.modified_time().seconds()
                           << ", Creation Time=" << info.creation_time().seconds();

        return StatusCode::OK;
    } else {
        dfs_log(LL_ERROR) << "[Stat] Error retrieving file attributes: " << status.error_message();
        return status.error_code() == grpc::StatusCode::NOT_FOUND ? StatusCode::NOT_FOUND
             : status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ? StatusCode::DEADLINE_EXCEEDED
             : StatusCode::CANCELLED;
    }
}

//
// STUDENT INSTRUCTION:
//
// Add your additional code here, including
// implementations of your client methods
//


