#include "dfs/dfslib-clientnode.h"

#include <errno.h>
#include <getopt.h>
#include <google/protobuf/util/time_util.h>
#include <grpcpp/grpcpp.h>
#include <limits.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <utime.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "dfs-service.grpc.pb.h"
#include "dfs/dfslib-shared.h"
#include "dfs/dfslibx-clientnode.h"
#include "utils/dfs-utils.h"
#include "dfs/sync_engine.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::Status;
using grpc::StatusCode;

using dfs_service::DFSService;
using namespace dfs_service;
using namespace std;
using google::protobuf::Timestamp;
using google::protobuf::util::TimeUtil;
using std::chrono::milliseconds;
using std::chrono::system_clock;
extern dfs_log_level_e DFS_LOG_LEVEL;


using FileRequestType = FileListRequest;
using FileListResponseType = FileList;

DFSClientNodeP2::DFSClientNodeP2() : DFSClientNode() {}
DFSClientNodeP2::~DFSClientNodeP2() {}


dfs::FileMetadata GetLocalMetadata(const std::string& filepath, const std::string& filename, CRC::Table<std::uint32_t, 32>& table) {
    struct stat fs;
    if (stat(filepath.c_str(), &fs) != 0) {
        return {filename, 0, 0, 0}; // Not found
    }
    uint32_t crc = 0;
    try {
        crc = dfs_file_checksum(filepath, &table);
    } catch(...) {}
    return {filename, fs.st_mtime, crc, static_cast<size_t>(fs.st_size)};
}

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
        dfs_log(LL_ERROR) << "[RequestWriteLock] Resource exhausted: Unable to obtain write lock for file: "
                          << filename;
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
    if (stat(file_path.c_str(), &fs) != 0) {
        dfs_log(LL_ERROR) << "[Store] File not found: " << file_path;
        return StatusCode::NOT_FOUND;
    }


    StatusCode lock_status = RequestWriteAccess(filename);
    if (lock_status != StatusCode::OK) {
        dfs_log(LL_DEBUG2) << "[Store]: Can't get write lock";
        return lock_status;
    }

  
    uint32_t client_crc = dfs_file_checksum(file_path, &crc_table);

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
                request.set_crc(client_crc);  
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
            if (status.error_code() == StatusCode::ALREADY_EXISTS) {
                return StatusCode::ALREADY_EXISTS;
            } else {
                return status.error_code() == grpc::DEADLINE_EXCEEDED ? StatusCode::DEADLINE_EXCEEDED
                                                                      : StatusCode::CANCELLED;
            }
        }
    } catch (const std::exception &e) {
        dfs_log(LL_ERROR) << "[Store] Exception occurred: " << e.what();
        return StatusCode::CANCELLED;
    }
    return StatusCode::UNKNOWN;
}

grpc::StatusCode DFSClientNodeP2::Fetch(const std::string &filename) {

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
            return StatusCode::NOT_FOUND;
        }

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


    try {
        while (reader->Read(&response)) {

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
            } else {
                return (status.error_code() == StatusCode::NOT_FOUND)           ? StatusCode::NOT_FOUND
                       : (status.error_code() == StatusCode::DEADLINE_EXCEEDED) ? StatusCode::DEADLINE_EXCEEDED
                                                                                : StatusCode::CANCELLED;
            }
        }
    } catch (const exception &e) {
        dfs_log(LL_ERROR) << "[Fetch] Exception occurred: " << e.what();
        return StatusCode::CANCELLED;
    }
}

grpc::StatusCode DFSClientNodeP2::Delete(const std::string &filename) {

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

grpc::StatusCode DFSClientNodeP2::List(std::map<std::string, int> *file_map, bool display) {

    file_map->clear();

    ClientContext context;
    FileListRequest request;
    FileList response;

    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));

    Status status = service_stub->ListFiles(&context, request, &response);

    if (status.ok()) {
        for (const auto &file_info : response.files()) {
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

grpc::StatusCode DFSClientNodeP2::Stat(const std::string &filename, void *file_status) {

    ClientContext context;
    StatusRequest request;
    FileInfo response;

    context.set_deadline(system_clock::now() + milliseconds(deadline_timeout));
    request.set_file_name(filename);
    request.set_client_id(this->client_id);

    Status status = service_stub->GetFileStatus(&context, request, &response);

    if (status.ok()) {
        FileInfo info = response;
        dfs_log(LL_SYSINFO) << "[Stat] File info: Name=" << info.name() << ", Size=" << info.size()
                            << ", Modified Time=" << info.mtime() << ", Creation Time=" << info.ctime()
                            << ", File SERVER CRC=" << info.crc();

        return StatusCode::OK;
    } else {
        dfs_log(LL_ERROR) << "[Stat] Error retrieving file attributes: " << status.error_message();
        return status.error_code() == grpc::StatusCode::NOT_FOUND           ? StatusCode::NOT_FOUND
               : status.error_code() == grpc::StatusCode::DEADLINE_EXCEEDED ? StatusCode::DEADLINE_EXCEEDED
                                                                            : StatusCode::CANCELLED;
    }

    return StatusCode::OK;
}

void DFSClientNodeP2::InotifyWatcherCallback(std::function<void()> callback) {


    lock_guard<mutex> lock(async_mutex);
    
    callback();
}


void DFSClientNodeP2::HandleCallbackList() {
    void *tag;
    bool ok = false;
    dfs::SyncEngine sync_engine; 

    while (completion_queue.Next(&tag, &ok)) {
        {

            std::unique_ptr<AsyncClientData<FileListResponseType>> call_data(
                static_cast<AsyncClientData<FileListResponseType> *>(tag));

            dfs_log(LL_DEBUG2) << "Received completion queue callback";

            if (!ok) {
                dfs_log(LL_ERROR) << "Completion queue callback not ok.";
            }

            if (ok && call_data->status.ok()) {
                dfs_log(LL_DEBUG3) << "Handling async callback ";

                lock_guard<mutex> lock(async_mutex);

                for (const FileInfo &server_fs : call_data->reply.files()) {
                    string file_name = server_fs.name();
                    string file_path = WrapPath(file_name);
                    
                    dfs::FileMetadata server_meta {
                        server_fs.name(),
                        (time_t)server_fs.mtime(),
                        server_fs.crc(),
                        (size_t)server_fs.size()
                    };
                    
                    dfs::FileMetadata local_meta = GetLocalMetadata(file_path, file_name, this->crc_table);
                    
                    dfs::SyncAction action;
                    if (local_meta.mtime == 0 && local_meta.size == 0 && local_meta.crc == 0) {
                        // Local file doesn't exist
                        action = sync_engine.DetermineActionFromRemoteCreate(server_meta);
                    } else {
                        action = sync_engine.DetermineActionFromServerEvent(local_meta, server_meta);
                    }

                    switch (action) {
                        case dfs::SyncAction::FETCH_FROM_SERVER:
                            dfs_log(LL_SYSINFO) << "SyncEngine: Fetching " << file_name;
                            this->Fetch(file_name);
                            break;
                        case dfs::SyncAction::STORE_TO_SERVER:
                            dfs_log(LL_SYSINFO) << "SyncEngine: Storing " << file_name;
                            this->Store(file_name);
                            break;
                        case dfs::SyncAction::NONE:
                            dfs_log(LL_DEBUG3) << "SyncEngine: No action for " << file_name;
                            break;
                        default:
                            break;
                    }
                }

            } else {
                dfs_log(LL_ERROR) << "Status was not ok. Will try again in " << DFS_RESET_TIMEOUT << " milliseconds.";
                dfs_log(LL_ERROR) << call_data->status.error_message();
                std::this_thread::sleep_for(std::chrono::milliseconds(DFS_RESET_TIMEOUT));
            }

 
        }

        dfs_log(LL_DEBUG3) << "Calling InitCallbackList";
        InitCallbackList();
    }
}


void DFSClientNodeP2::InitCallbackList() { CallbackList<FileRequestType, FileListResponseType>(); }

