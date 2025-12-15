# Distributed File System (DFS)

![C++](https://img.shields.io/badge/C++-17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)
![gRPC](https://img.shields.io/badge/gRPC-Enabled-4285F4?style=for-the-badge&logo=google&logoColor=white)
![Protobuf](https://img.shields.io/badge/Protobuf-3.0-4285F4?style=for-the-badge&logo=google&logoColor=white)
![CMake](https://img.shields.io/badge/CMake-Build-064F8C?style=for-the-badge&logo=cmake&logoColor=white)
![Docker](https://img.shields.io/badge/Docker-Enabled-2496ED?style=for-the-badge&logo=docker&logoColor=white)
![Python](https://img.shields.io/badge/Python-3.8%2B-3776AB?style=for-the-badge&logo=python&logoColor=white)
![React](https://img.shields.io/badge/React-18-61DAFB?style=for-the-badge&logo=react&logoColor=black)

## 📖 Introduction

**Distributed File System (DFS)** is a C++17 distributed file system built on gRPC and Protocol Buffers. It focuses on correctness under concurrency (locks, callbacks, file watchers) and ease of deployment via Docker.

The system implements core file operations with strong consistency guarantees, featuring a client-server architecture where the client mounts a local directory and synchronizes changes with the server in real-time.

## 🚀 Key Features

### 🔧 Core System
*   **File Operations**: `mount`, `fetch`, `store`, `delete`, `list`, `stat` via gRPC/Protobuf.
*   **Concurrency Control**: Server-side single-writer semantics to prevent corruption.
*   **Synchronization**: Client-side cache with CRC-based change detection and "last write wins" conflict strategy.
*   **Real-time Updates**: Async server-to-client notifications (gRPC async) to keep caches in sync.

### 📊 Observability & UI
*   **Dashboard**: A React-based web interface for visualizing system state and logs.
*   **Monitor Service**: Streaming runtime events via a dedicated monitor service.
*   **Event Bus**: An event-driven layer for internal system observability.

## 🛠 Tech Stack

### Core
*   **Language**: C++17
*   **Communication**: gRPC, Protocol Buffers
*   **Logging**: Spdlog
*   **Threading**: Pthreads, std::thread

### Observability & Tools
*   **Gateway**: Python (FastAPI/Scripts)
*   **Dashboard**: React 18, TypeScript, Vite, Tailwind CSS
*   **Build System**: CMake (>= 3.15)

### DevOps
*   **Containerization**: Docker & Docker Compose

## 💡 Technical Highlights & Architecture

*   **Client (`dfs-client`)**: Mounts a local directory, watches for file changes using platform-specific watchers (e.g., `inotify`), and converts changes into RPC calls.
*   **Server (`dfs-server`)**: Manages storage under a mount directory, enforces write locks, and broadcasts callback events to connected clients.
*   **Shared Logic**: Common implementations located in `src/common/` and `include/` to ensure consistency.
*   **Multi-threaded Runtime**: Utilizes gRPC async threads and dedicated client watcher threads for non-blocking performance.

## ⚡ Getting Started

The project is containerized for easy setup.

### Prerequisites
*   Docker & Docker Compose
*   CMake (>= 3.15) (for local build)
*   C++17 Compiler (for local build)

### Run with Docker (Recommended)

This brings up the full dev stack: DFS server, demo agent, gateway, and dashboard.

```bash
# 1. Build the C++ binaries (inside the dev container)
docker compose run --rm dev bash -lc "cmake -S . -B build && cmake --build build -j"

# 2. Start services
docker compose up --build
```

Access the application:
*   **Dashboard**: `http://localhost:5173`
*   **Gateway**: `http://localhost:8000`
*   **gRPC Server**: `localhost:50051`

### Build & Run Locally

Dependencies: CMake, C++17 compiler, gRPC, Protobuf, `spdlog`, pthreads.

```bash
# Build
cmake -S . -B build
cmake --build build -j

# Run Server
mkdir -p mnt/server
./build/bin/dfs-server -a 0.0.0.0:50051 -m mnt/server -n 4

# Run Client (in another terminal)
mkdir -p mnt/client
./build/bin/dfs-client -a 127.0.0.1:50051 -m mnt/client mount
```