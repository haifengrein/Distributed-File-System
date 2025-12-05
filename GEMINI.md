# Gemini Context: Distributed File System (DFS) Project

This document provides context for the Gemini agent interacting with this C++ project. The project is a Distributed File System (DFS) implementation using gRPC and Protocol Buffers, split into two parts.

## Project Overview

*   **Type:** C++ (C++14) Systems Programming.
*   **Core Technologies:** gRPC, Protocol Buffers (protobuf), POSIX Threads (pthread).
*   **Goal:** Implement a distributed file system with client-side caching, consistency models, and asynchronous notifications.
*   **Structure:**
    *   **Part 1:** Basic RPC services (Fetch, Store, List, Stat) using synchronous gRPC.
    *   **Part 2:** Advanced DFS features (Whole-file caching, Write locks, Async notifications, Eventual consistency using `inotify` and CRC checksums).

## Directory Structure (Standard Layout)

*   `cmake/`: CMake modules and configuration files.
*   `docs/`: Project documentation and diagrams.
*   `include/`: Public header files (`.h`).
    *   `dfs/`: Core DFS headers.
    *   `utils/`: Utility headers.
*   `protos/`: Protocol Buffer definition files (`.proto`).
*   `src/`: Source code (`.cpp`).
    *   `client/`: Client-side specific implementation.
    *   `server/`: Server-side specific implementation.
    *   `common/`: Shared logic and utilities.
*   `tests/`: Unit and integration tests.
*   `tools/`: Helper scripts (e.g., build scripts, docker helpers).
*   `build/`: (Git ignored) Build artifacts.
*   `bin/`: (Git ignored) Final executables.

## Building and Running

The project uses **CMake** for build automation and **Docker** for a consistent development environment.

### Development Environment
1.  **Start Environment:** `docker-compose up -d`
2.  **Enter Shell:** `docker-compose exec dev bash`

### Build Commands (inside container)
```bash
mkdir -p build && cd build
cmake ..
make
```

### Executables
*   **Server:** `./build/bin/dfs-server`
*   **Client:** `./build/bin/dfs-client`

## Development Conventions

*   **Student Code:** modify **ONLY** the following files (in `part1/` or `part2/`):
    *   `dfslib-servernode-p*.[cpp,h]`: Server-side logic implementation (override `DFSServerImpl`).
    *   `dfslib-clientnode-p*.[cpp,h]`: Client-side logic implementation.
    *   `dfslib-shared-p*.[cpp,h]`: Shared utilities and helper functions.
    *   `dfs-service.proto`: Protocol Buffer definitions.
*   **Restricted Files:** Do **NOT** modify:
    *   `src/dfs-client-p*.[cpp,h]` (Main entry points).
    *   `src/dfs-server-p*.[cpp]` (Main entry points).
    *   `src/dfslibx-*` (Base classes and infrastructure).
    *   `proto-src/*` (Generated code).
*   **Coding Style:** Adhere to C++14 standards. Use `// STUDENT INSTRUCTION:` comments as guides.
*   **Testing:** Manual testing using the CLI tools is expected. There is no local automated test suite provided in the repository (autograder is external).

## Key Concepts & Implementation Details

*   **gRPC Service:** Defined in `dfs-service.proto`. Needs to be recompiled (`make protos`) after any change.
*   **Synchronization:** Uses `pthread` mutexes and condition variables for thread safety.
*   **Consistency (Part 2):**
    *   **Strategy:** Last-write-wins based on modification timestamps.
    *   **Locking:** Write locks enforced by the server.
    *   **Change Detection:** CRC checksums used to detect file changes.
    *   **Notifications:** Asynchronous gRPC callbacks from server to client.
