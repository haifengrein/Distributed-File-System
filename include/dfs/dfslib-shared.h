#ifndef PR4_DFSLIB_SHARED_H
#define PR4_DFSLIB_SHARED_H

#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <thread>

#include "dfs-service.grpc.pb.h"
#include "utils/dfs-utils.h"


#define DFS_RESET_TIMEOUT 5000
#define DFS_I_EVENT_SIZE (sizeof(struct inotify_event))
#define DFS_I_BUFFER_SIZE (1024 * (DFS_I_EVENT_SIZE + 16))

/** A file descriptor type **/
typedef int FileDescriptor;

/** A watch descriptor type **/
typedef int WatchDescriptor;

/** An inotify callback method **/
typedef void (*InotifyCallback)(uint, const std::string&, void*);

/** The expected struct for the inotify setup in this project **/
struct NotifyStruct {
    FileDescriptor fd;
    WatchDescriptor wd;
    uint event_type;
    std::thread* thread;
    InotifyCallback callback;
};

/** The expected event type for the event method in this project **/
struct EventStruct {
    void* event;
    void* instance;
};



#endif
