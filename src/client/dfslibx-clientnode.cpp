#include "dfs/dfslibx-clientnode.h"

#include <errno.h>
#include <getopt.h>
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
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "utils/dfs-utils.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientWriter;
using grpc::Status;
using grpc::StatusCode;

extern dfs_log_level_e DFS_LOG_LEVEL;

DFSClientNode::DFSClientNode() : mount_path("mnt/client/"), unmounting(false), crc_table(CRC::CRC_32()) {
    char host[HOST_NAME_MAX];
    std::ostringstream ss_id;
    gethostname(host, HOST_NAME_MAX);
    auto t_id = std::this_thread::get_id();
    ss_id << "T" << t_id;
    client_id = std::string(host + ss_id.str());
}

DFSClientNode::~DFSClientNode() noexcept {}

void DFSClientNode::Unmount() { this->unmounting = true; }

bool DFSClientNode::Unmounting() { return this->unmounting; }

const std::string DFSClientNode::ClientId() { return this->client_id; }

void DFSClientNode::CreateStub(std::shared_ptr<Channel> channel) {
    this->service_stub = dfs_service::DFSService::NewStub(channel);
}

void DFSClientNode::SetMountPath(const std::string &path) { this->mount_path = path; }

void DFSClientNode::SetDeadlineTimeout(int deadline) { this->deadline_timeout = deadline; }

void DFSClientNode::SetClientId(const std::string &id) { this->client_id = id; }

const std::string DFSClientNode::MountPath() { return this->mount_path; };

std::string DFSClientNode::WrapPath(const std::string &filepath) { return this->mount_path + filepath; }
