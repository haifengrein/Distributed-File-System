cmake_minimum_required(VERSION 3.15)
project(DFS_Project CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Dependencies
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ grpc)
find_package(spdlog REQUIRED)
find_package(GTest)
find_package(Threads REQUIRED)

# Find the gRPC C++ plugin
if(NOT gRPC_CPP_PLUGIN_EXECUTABLE)
    find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
endif()

message(STATUS "Using Protobuf Compiler: ${Protobuf_PROTOC_EXECUTABLE}")
message(STATUS "Using gRPC Plugin: ${gRPC_CPP_PLUGIN_EXECUTABLE}")

# --- Proto Configuration ---
set(PROTO_DIR "${CMAKE_SOURCE_DIR}/protos")
set(PROTO_SRC_DIR "${CMAKE_BINARY_DIR}/proto-src")
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})

set(PROTO_FILE "${PROTO_DIR}/dfs-service.proto")
set(PROTO_SRCS "${PROTO_SRC_DIR}/dfs-service.pb.cc" "${PROTO_SRC_DIR}/dfs-service.grpc.pb.cc")
set(PROTO_HDRS "${PROTO_SRC_DIR}/dfs-service.pb.h" "${PROTO_SRC_DIR}/dfs-service.grpc.pb.h")

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out=${PROTO_SRC_DIR}
         --cpp_out=${PROTO_SRC_DIR}
         --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN_EXECUTABLE}
         -I ${PROTO_DIR}
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating gRPC and Protobuf source files from ${PROTO_FILE}"
)

add_library(dfs_proto ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(dfs_proto PUBLIC ${GRPC_LIBRARIES} protobuf::libprotobuf)
target_include_directories(dfs_proto PUBLIC ${PROTO_SRC_DIR} ${GRPC_INCLUDE_DIRS})

# --- Include Directories ---
# Make headers visible globally for simplicity in this phase
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/dfs)
include_directories(${CMAKE_SOURCE_DIR}/include/utils)
include_directories(${PROTO_SRC_DIR})

# --- Common Library ---
add_library(dfs_common
    src/common/dfslib-shared.cpp
    src/common/storage/posix_storage_engine.cpp
    src/common/lock_manager.cpp
    src/common/sync_engine.cpp
    src/common/event_bus.cpp
)
target_link_libraries(dfs_common PUBLIC dfs_proto spdlog::spdlog)

# --- DFS Client ---
add_executable(dfs-client
    src/client/dfs-client.cpp
    src/client/dfslib-clientnode.cpp
    src/client/dfslibx-clientnode.cpp
)
target_link_libraries(dfs-client PRIVATE dfs_common dfs_proto ${GRPC_LIBRARIES} protobuf::libprotobuf Threads::Threads)
target_compile_definitions(dfs-client PRIVATE DFS_MAIN)

# --- DFS Server ---
add_executable(dfs-server
    src/server/dfs-server.cpp
    src/server/dfslib-servernode.cpp
    src/server/monitor_service.cpp
)
target_link_libraries(dfs-server PRIVATE dfs_common dfs_proto ${GRPC_LIBRARIES} protobuf::libprotobuf Threads::Threads)
target_compile_definitions(dfs-server PRIVATE DFS_MAIN)

# --- Testing ---
if(CMAKE_PROJECT_NAME STREQUAL "DFS_Project")
    enable_testing()
    if(GTEST_FOUND)
        add_subdirectory(tests)
    else()
        message(WARNING "GoogleTest not found. Tests will not be built.")
    endif()
endif()