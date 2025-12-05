# Gemini Context: Distributed File System (DFS) Project

This document provides context for the Gemini agent interacting with this C++ project. The project is a Distributed File System (DFS) implementation using gRPC and Protocol Buffers, split into two parts.

## Project Overview

*   **Type:** C++ (C++14) Systems Programming.
*   **Core Technologies:** gRPC, Protocol Buffers (protobuf), POSIX Threads (pthread).
*   **Goal:** Implement a distributed file system with client-side caching, consistency models, and asynchronous notifications.
*   **Structure:**
    *   **Part 1:** Basic RPC services (Fetch, Store, List, Stat) using synchronous gRPC.
    *   **Part 2:** Advanced DFS features (Whole-file caching, Write locks, Async notifications, Eventual consistency using `inotify` and CRC checksums).

## Directory Structure

*   `bin/`: Compiled executables (`dfs-server-p1`, `dfs-client-p1`, etc.) will be placed here.
*   `docs/`: Documentation and diagrams for Part 1 and Part 2.
*   `mnt/`: Mount points for the client/server storage (used in Part 2).
*   `part1/`: Source code and configuration for Part 1.
*   `part2/`: Source code and configuration for Part 2.
*   `tmp/`: Temporary object files.

## Building and Running

The project uses `make` for build automation. The root `Makefile` delegates to `part1` and `part2` Makefiles.

### Common Commands

*   **Build All:** `make part1` or `make part2` (from root).
*   **Generate Protobufs:** `make protos` (Crucial step after modifying `.proto` files).
*   **Clean:** `make clean_all` or `make clean_part1` / `make clean_part2`.

### Part 1 (Basic RPC)

*   **Directory:** `part1/`
*   **Key Executables:**
    *   Server: `./bin/dfs-server-p1`
    *   Client: `./bin/dfs-client-p1 <command> <args>`
*   **Client Commands:** `fetch`, `store`, `list`, `stat`.
    *   Example: `./bin/dfs-client-p1 fetch filename.jpg`

### Part 2 (Full DFS)

*   **Directory:** `part2/`
*   **Key Executables:**
    *   Server: `./bin/dfs-server-p2`
    *   Client: `./bin/dfs-client-p2 <command>`
*   **Usage:**
    *   Server: `./bin/dfs-server-p2`
    *   Client Mount: `./bin/dfs-client-p2 mount` (Starts watcher and sync threads).

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
