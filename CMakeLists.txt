cmake_minimum_required(VERSION 3.15)
project(DFS_Project CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ grpc)
find_package(Threads REQUIRED)

# Find the gRPC C++ plugin
if(NOT gRPC_CPP_PLUGIN_EXECUTABLE)
    find_program(gRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
endif()

message(STATUS "Using Protobuf Compiler: ${Protobuf_PROTOC_EXECUTABLE}")
message(STATUS "Using gRPC Plugin: ${gRPC_CPP_PLUGIN_EXECUTABLE}")

# Directory Configuration (Temporary structure based on Part 2)
set(PART2_DIR "${CMAKE_SOURCE_DIR}/part2")
set(PROTO_SRC_DIR "${CMAKE_SOURCE_DIR}/part2/proto-src")

# Ensure the output directory exists
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})

# Define the Proto file
set(PROTO_FILE "${PART2_DIR}/dfs-service.proto")

# Generated Sources
set(PROTO_SRCS "${PROTO_SRC_DIR}/dfs-service.pb.cc" "${PROTO_SRC_DIR}/dfs-service.grpc.pb.cc")
set(PROTO_HDRS "${PROTO_SRC_DIR}/dfs-service.pb.h" "${PROTO_SRC_DIR}/dfs-service.grpc.pb.h")

# Custom command to generate the files
add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out=${PROTO_SRC_DIR}
         --cpp_out=${PROTO_SRC_DIR}
         --plugin=protoc-gen-grpc=${gRPC_CPP_PLUGIN_EXECUTABLE}
         -I ${PART2_DIR}
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating gRPC and Protobuf source files from ${PROTO_FILE}"
)

# Create a library for the Protocol Buffers logic
add_library(dfs_proto ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(dfs_proto PUBLIC ${GRPC_LIBRARIES} protobuf::libprotobuf)
target_include_directories(dfs_proto PUBLIC ${PROTO_SRC_DIR} ${GRPC_INCLUDE_DIRS})

# Include Directories
include_directories(${PART2_DIR})
include_directories(${PART2_DIR}/src)
include_directories(${PROTO_SRC_DIR})

# --- DFS Client ---
add_executable(dfs-client
    ${PART2_DIR}/src/dfs-client-p2.cpp
    ${PART2_DIR}/dfslib-clientnode-p2.cpp
    ${PART2_DIR}/dfslib-shared-p2.cpp
    ${PART2_DIR}/src/dfslibx-clientnode-p2.cpp
)
target_link_libraries(dfs-client PRIVATE dfs_proto ${GRPC_LIBRARIES} protobuf::libprotobuf Threads::Threads)
target_compile_definitions(dfs-client PRIVATE DFS_MAIN)

# --- DFS Server ---
add_executable(dfs-server
    ${PART2_DIR}/src/dfs-server-p2.cpp
    ${PART2_DIR}/dfslib-servernode-p2.cpp
    ${PART2_DIR}/dfslib-shared-p2.cpp
)
target_link_libraries(dfs-server PRIVATE dfs_proto ${GRPC_LIBRARIES} protobuf::libprotobuf Threads::Threads)
target_compile_definitions(dfs-server PRIVATE DFS_MAIN)
