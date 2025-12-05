#include <regex>
#include <mutex>
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
#include <utime.h>
#include <google/protobuf/util/time_util.h>


#include "src/dfs-utils.h"
#include "src/dfslibx-clientnode-p2.h"
#include "dfslib-shared-p2.h"
#include "dfslib-clientnode-p2.h"
#include "proto-src/dfs-service.grpc.pb.h"

using grpc::Status;
using grpc::Channel;
using grpc::StatusCode;
using grpc::ClientWriter;
using grpc::ClientReader;
using grpc::ClientContext;

using dfs_service::DFSService;
using namespace dfs_service;
using namespace std;
using std::chrono::system_clock;
using std::chrono::milliseconds;
using google::protobuf::util::TimeUtil;
using google::protobuf::Timestamp;
extern dfs_log_level_e DFS_LOG_LEVEL;

//
// STUDENT INSTRUCTION:
//
// Change these "using" aliases to the specific
// message types you are using to indicate
// a file request and a listing of files from the server.
//
using FileRequestType = FileListRequest;
using FileListResponseType = FileList;

DFSClientNodeP2::DFSClientNodeP2() : DFSClientNode() {}
DFSClientNodeP2::~DFSClientNodeP2() {}



grpc::StatusCode DFSClientNodeP2::RequestWriteAccess(const std::string &filename) {

    ClientContext context;
    WriteLockRequest request;
    WriteLockResponse response;

    request.set_filename(filename);
    request.set_client_id(this->ClientId());
    context.set_deadline(system_clock::now() + milliseconds(this->deadline_timeout));

    Status status = service_stub->RequestWriteLock(&context, request, &response);

    if (status.ok() && response.success()) {
        dfs_log(LL_SYSINFO) << "[RequestWriteLock] Write lock obtained for file: " << filename;
        return StatusCode::OK;
    } else if (status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED) {
        dfs_log(LL_ERROR) << "[RequestWriteLock] Deadline exceeded for write lock request on file: " << filename;
        return StatusCode::DEADLINE_EXCEEDED;
    } else if (status.error_code() == grpc::StatusCode::RESOURCE_EXHAUSTED) {
        dfs_log(LL_ERROR) << "[RequestWriteLock] Resource exhausted: Unable to obtain write lock for file: " << filename;
        return StatusCode::RESOURCE_EXHAUSTED;
    } else {
        dfs_log(LL_ERROR) << "[RequestWriteLock] Write lock request cancelled or failed for file: " << filename;
        return StatusCode::CANCELLED;
    }
}






grpc::StatusCode DFSClientNodeP2::Store(const std::string &filename) {

    ClientContext context;
    StoreResponse response;

    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));
    string file_path = WrapPath(filename);

    struct stat fs;
    if(stat (file_path.c_str(),&fs) != 0) {
        dfs_log(LL_ERROR) << "[Store] File not found: " << file_path;
        return StatusCode::NOT_FOUND;
    }

    // Request a write lock before proceeding
    StatusCode lock_status = RequestWriteAccess(filename);
    if (lock_status != StatusCode::OK) {
         dfs_log(LL_DEBUG2) << "[Store]: Can't get write lock";
        return lock_status; 
    }

    // Check local file's CRC
    uint32_t client_crc = dfs_file_checksum(file_path, &crc_table);

    // Open the file for reading
    ifstream ifs(file_path, ios::binary);
    if (!ifs.is_open()) {
        dfs_log(LL_ERROR) << "[Store] Unable to open file: " << file_path;
        return StatusCode::INTERNAL;
    }
    
    unique_ptr<ClientWriter<StoreRequest>> writer(service_stub->StoreFile(&context, &response));

    StoreRequest request;
    char buffer[4096];
    bool isFirstChunk = true;

    try {
        while (!ifs.eof()) {
            ifs.read(buffer, sizeof(buffer));
            int bytes_read = ifs.gcount();

            if (isFirstChunk) {
                request.set_file_name(filename);
                request.set_client_id(this->client_id);
                request.set_crc(client_crc); // Set the CRC value
                request.set_mtime(fs.st_mtime);
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
            if (response.success()) {
                dfs_log(LL_SYSINFO) << "[Store] File stored successfully: " << file_path;
                return StatusCode::OK;
            }
        } else {
            dfs_log(LL_ERROR) << "[Store] Store failed: " << status.error_message();
            if(status.error_code() == StatusCode::ALREADY_EXISTS) { 
                 return StatusCode::ALREADY_EXISTS;
            } else {
                 return status.error_code() == grpc::DEADLINE_EXCEEDED ? StatusCode::DEADLINE_EXCEEDED : StatusCode::CANCELLED;
            }
        }
    } catch (const std::exception &e) {
        dfs_log(LL_ERROR) << "[Store] Exception occurred: " << e.what();
        return StatusCode::CANCELLED;
    }
    return StatusCode::UNKNOWN;
}



grpc::StatusCode DFSClientNodeP2::Fetch(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to fetch a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation. However, you will
    // need to adjust this method to recognize when a file trying to be
    // fetched is the same on the client (i.e. the files do not differ
    // between the client and server and a fetch would be unnecessary.
    //
    // The StatusCode response should be:
    //
    // OK - if all went well
    // DEADLINE_EXCEEDED - if the deadline timeout occurs
    // NOT_FOUND - if the file cannot be found on the server
    // ALREADY_EXISTS - if the local cached file has not changed from the server version
    // CANCELLED otherwise
    //
    // Hint: You may want to match the mtime on local files to the server's mtime
    //
    ClientContext context;
    FetchRequest request;
    FetchResponse response;
    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));
    request.set_file_name(filename);
    request.set_client_id(this->client_id);
    string file_path = WrapPath(filename);

    struct stat local_fs;
    bool local_file_exists = (stat(file_path.c_str(), &local_fs) == 0);
    uint32_t client_crc = dfs_file_checksum(WrapPath(filename), &this->crc_table);

    FileInfo x;

    if (local_file_exists) {
        ClientContext status_context;
        StatusRequest status_request;
        FileInfo file_info;
        
        status_request.set_file_name(filename);
        status_request.set_client_id(this->client_id);

        Status status = service_stub->GetFileStatus(&status_context, status_request, &file_info);

        if (status.error_code() == StatusCode::NOT_FOUND) {
            return StatusCode::NOT_FOUND;} 
        
        int64_t server_mtime = file_info.mtime();
        int64_t client_mtime = local_fs.st_mtime;
        uint32_t server_crc = file_info.crc();

        if (server_crc == client_crc) {
            if (server_mtime > client_mtime) {
                struct utimbuf mtime;
                mtime.modtime = server_mtime;
                utime(file_path.c_str(), &mtime);
                dfs_log(LL_SYSINFO) << "[Fetch] Updated client mod time to match server mod time.";
            } else if (server_mtime < client_mtime) {
                dfs_log(LL_SYSINFO) << "[Fetch] Client has latest version of file :" << filename;
            }

        dfs_log(LL_SYSINFO) << "[Fetch] Only fetch file stats since content is the same!";
        return StatusCode::ALREADY_EXISTS;
        }      
    }


    request.set_crc(client_crc);

    unique_ptr<ClientReader<FetchResponse>> reader(service_stub->FetchFile(&context, request));
    ofstream ofs;
    ofs.open(file_path);

    // if (!ofs.is_open()) {
    //     dfs_log(LL_ERROR) << "[Fetch] Failed to open file: " << filename;
    //     return StatusCode::NOT_FOUND;
    // }

    try {
        while (reader->Read(&response)) {
            // if (response.crc() == client_crc) {
            //     dfs_log(LL_SYSINFO) << "[Fetch] No changes in file: " << filename;
            //     return StatusCode::ALREADY_EXISTS;
            // }

            // ofs.write(response.chunk().data(), response.chunk().size());
            // if (ofs.fail()) {
            //     dfs_log(LL_ERROR) << "[Fetch] Failed to write data to file: " << filename;
            //     return StatusCode::INTERNAL;
            // }
            const string &chunk = response.chunk();
            ofs << chunk;
        }

        ofs.close();
        Status status = reader->Finish();

        if (status.ok()) {
            dfs_log(LL_SYSINFO) << "[Fetch] File fetched successfully: " << filename;
            return StatusCode::OK;
        } else {
            dfs_log(LL_ERROR) << "[Fetch] Fetch failed: " << status.error_message();
            if (status.error_code() == StatusCode::ALREADY_EXISTS) {
                return StatusCode::ALREADY_EXISTS;
            } else{
                return (status.error_code() == StatusCode::NOT_FOUND) ? StatusCode::NOT_FOUND :
                       (status.error_code() == StatusCode::DEADLINE_EXCEEDED) ? StatusCode::DEADLINE_EXCEEDED :
                        StatusCode::CANCELLED;
            }
            
        }
    } catch (const exception &e) {
        dfs_log(LL_ERROR) << "[Fetch] Exception occurred: " << e.what();
        return StatusCode::CANCELLED;
    }
}

grpc::StatusCode DFSClientNodeP2::Delete(const std::string &filename) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to delete a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You will also need to add a request for a write lock before attempting to delete.
    //
    // If the write lock request fails, you should return a status of RESOURCE_EXHAUSTED
    // and cancel the current operation.
    //
    // The StatusCode response should be:
    //
    // StatusCode::OK - if all went well
    // StatusCode::DEADLINE_EXCEEDED - if the deadline timeout occurs
    // StatusCode::RESOURCE_EXHAUSTED - if a write lock cannot be obtained
    // StatusCode::CANCELLED otherwise
    //
    //
    ClientContext context;
    DeleteRequest request;
    DeleteResponse response;
    
    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));
    request.set_file_name(filename);
    request.set_client_id(this->client_id);

    StatusCode lock_status = RequestWriteAccess(filename);
    if (lock_status != StatusCode::OK) {
         dfs_log(LL_DEBUG2) << "[Store]: Can't get write lock";
        return lock_status; 
    }

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

grpc::StatusCode DFSClientNodeP2::List(std::map<std::string,int>* file_map, bool display) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to list files here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // listing details that would be useful to your solution to the list response.
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
    FileListRequest request;
    FileList response;

   
    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));

    
    Status status = service_stub->ListFiles(&context, request, &response);

    if (status.ok()) {
        
        for (const auto& file_info : response.files()) {
            
            time_t modified_time = (time_t)file_info.mtime();
            (*file_map)[file_info.name()] = modified_time;

            if (display) {
                char time_buffer[80];
                struct tm *tm_info = localtime(&modified_time);
                strftime(time_buffer, 80, "%Y-%m-%d %H:%M:%S", tm_info);
                cout << "File: " << file_info.name() << ", Modified Time: " << time_buffer << endl;
            }
        }
        return StatusCode::OK;
    } else {
        dfs_log(LL_ERROR) << "Failed to list files: " << status.error_message();
        return status.error_code() == grpc::DEADLINE_EXCEEDED ? StatusCode::DEADLINE_EXCEEDED : StatusCode::CANCELLED;
    }
}

grpc::StatusCode DFSClientNodeP2::Stat(const std::string &filename, void* file_status) {

    //
    // STUDENT INSTRUCTION:
    //
    // Add your request to get the status of a file here. Refer to the Part 1
    // student instruction for details on the basics.
    //
    // You can start with your Part 1 implementation and add any additional
    // status details that would be useful to your solution.
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
    StatusRequest request;
    FileInfo response;

    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));
    request.set_file_name(filename);
    request.set_client_id(this->client_id);

    Status status = service_stub->GetFileStatus(&context, request, &response);

    if (status.ok()) {
        FileInfo info = response;
        dfs_log(LL_SYSINFO) << "[Stat] File info: Name=" << info.name() 
                           << ", Size=" << info.size() 
                           << ", Modified Time=" << info.mtime()
                           << ", Creation Time=" << info.ctime()
                           << ", File SERVER CRC=" << info.crc();

        return StatusCode::OK;
    } else {
        dfs_log(LL_ERROR) << "[Stat] Error retrieving file attributes: " << status.error_message();
        return status.error_code() == grpc::StatusCode::NOT_FOUND ? StatusCode::NOT_FOUND
             : status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ? StatusCode::DEADLINE_EXCEEDED
             : StatusCode::CANCELLED;
    }

    return StatusCode::OK;
}

void DFSClientNodeP2::InotifyWatcherCallback(std::function<void()> callback) {

    //
    // STUDENT INSTRUCTION:
    //
    // This method gets called each time inotify signals a change
    // to a file on the file system. That is every time a file is
    // modified or created.
    //
    // You may want to consider how this section will affect
    // concurrent actions between the inotify watcher and the
    // asynchronous callbacks associated with the server.
    //
    // The callback method shown must be called here, but you may surround it with
    // whatever structures you feel are necessary to ensure proper coordination
    // between the async and watcher threads.
    //
    // Hint: how can you prevent race conditions between this thread and
    // the async thread when a file event has been signaled?
    //

    lock_guard<mutex> lock(async_mutex);
    callback();

}

//
// STUDENT INSTRUCTION:
//
// This method handles the gRPC asynchronous callbacks from the server.
// We've provided the base structure for you, but you should review
// the hints provided in the STUDENT INSTRUCTION sections below
// in order to complete this method.
//

void DFSClientNodeP2::HandleCallbackList() {

    void* tag;

    bool ok = false;

   
    while (completion_queue.Next(&tag, &ok)) {
        {
            //
            // STUDENT INSTRUCTION:
            //
            // Consider adding a critical section or RAII style lock here
            //

            // The tag is the memory location of the call_data object
            AsyncClientData<FileListResponseType> *call_data = static_cast<AsyncClientData<FileListResponseType> *>(tag);

            dfs_log(LL_DEBUG2) << "Received completion queue callback";

            // Verify that the request was completed successfully. Note that "ok"
            // corresponds solely to the request for updates introduced by Finish().
            // GPR_ASSERT(ok);
            if (!ok) {
                dfs_log(LL_ERROR) << "Completion queue callback not ok.";
            }

            if (ok && call_data->status.ok()) {

                dfs_log(LL_DEBUG3) << "Handling async callback ";

                //
                // STUDENT INSTRUCTION:
                //
                // Add your handling of the asynchronous event calls here.
                // For example, based on the file listing returned from the server,
                // how should the client respond to this updated information?
                // Should it retrieve an updated version of the file?
                // Send an update to the server?
                // Do nothing?
                //
                lock_guard<mutex> lock(async_mutex);
                
                for (const FileInfo &server_fs : call_data->reply.files()) {
                    FileInfo local_fs;
                    string file_name = server_fs.name();
                    string file_path = WrapPath(file_name);
                    
                    int64_t server_mtime = server_fs.mtime();
                    int64_t local_mtime = local_fs.mtime();

                    struct stat fs;
                    bool file_exists = (stat(file_path.c_str(), &fs) == 0);

                    if (!file_exists) {
                        this->Fetch(file_name);
                    }
                    else if (server_mtime < local_mtime) {
                        this->Store(file_name);
                    }
                    else if (server_mtime > local_mtime) {
                        StatusCode status_code = this->Fetch(file_name);
                        if (status_code == StatusCode::ALREADY_EXISTS) {
                            struct utimbuf new_times;
                            new_times.actime = fs.st_atime;
                            new_times.modtime = server_mtime;   
                            utime(file_path.c_str(), &new_times);
                        }
                    }
                }

            } else {
                dfs_log(LL_ERROR) << "Status was not ok. Will try again in " << DFS_RESET_TIMEOUT << " milliseconds.";
                dfs_log(LL_ERROR) << call_data->status.error_message();
                std::this_thread::sleep_for(std::chrono::milliseconds(DFS_RESET_TIMEOUT));
            }

            // Once we're complete, deallocate the call_data object.
            delete call_data;

            //
            // STUDENT INSTRUCTION:
            //
            // Add any additional syncing/locking mechanisms you may need here

        }


        // Start the process over and wait for the next callback response
        dfs_log(LL_DEBUG3) << "Calling InitCallbackList";
        InitCallbackList();

    }
}
/**
 * This method will start the callback request to the server, requesting
 * an update whenever the server sees that files have been modified.
 *
 * We're making use of a template function here, so that we can keep some
 * of the more intricate workings of the async process out of the way, and
 * give you a chance to focus more on the project's requirements.
 */
void DFSClientNodeP2::InitCallbackList() {
    CallbackList<FileRequestType, FileListResponseType>();
}

//
// STUDENT INSTRUCTION:
//
// Add any additional code you need to here
//

