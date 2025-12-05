# Key Takeaways: Phase 1 (Environment Modernization)

This document records the technical challenges, decisions, and solutions encountered during the transformation of the legacy DFS academic project into a modern, production-grade system.

## 1. Environment Migration: From "Coursework" to "Standard"

### Challenge
The original project relied on a specific, outdated environment (`Ubuntu 20.04` + Georgia Tech's private `ppa/gt-cs6200` repository). This created a **Vendor Lock-in** scenario where the code could only be built inside a specific Vagrant VM provided by the course.

### Solution
We successfully migrated to a **Standard Docker Environment** based on `Ubuntu 22.04 LTS`.
- **Removed:** Reliance on `ppa/gt-cs6200` and custom `setup.sh` scripts.
- **Added:** Official Ubuntu repositories for `libgrpc++-dev` and `libprotobuf-dev`.
- **Outcome:** The project is now portable. Anyone with Docker can build it without setting up a complex VM.

## 2. Build System Overhaul: Makefile vs. CMake

### Challenge
The project used a monolithic `Makefile` with hardcoded paths (`pkg-config protobuf grpc`). While functional, it lacked:
- Integration with modern IDEs (VS Code, CLion).
- Automatic dependency resolution for headers.
- Scalability for adding tests and new modules.

### Solution
We replaced the build system with **Modern CMake (3.15+)**.
- **Why:** CMake is the industry standard for C++. It handles the complexity of generating `protobuf` source files (`.pb.cc`, `.grpc.pb.cc`) automatically via `add_custom_command`.
- **Outcome:** A maintainable `CMakeLists.txt` that clearly defines targets (`dfs-client`, `dfs-server`) and dependencies.

## 3. The "Dependency Hell" of gRPC

### The Problem
After switching to CMake, the build failed with:
```cmake
Could not find a package configuration file provided by "gRPC"
```
This happened because the standard Ubuntu `libgrpc++-dev` package **does not provide CMake Config files** (`gRPCConfig.cmake`), unlike the upstream build from source.

### The Fix
We adapted the `CMakeLists.txt` to use **PkgConfig** as a fallback:
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(GRPC REQUIRED grpc++ grpc)
```
This bridges the gap between CMake's expectation and the Linux distribution's package structure.

## 4. Verification Strategy

We adhered to the principle: **"Compilation != Working Software"**.
After getting the code to compile, we wrote a verification script (`verify_env.sh`) to run inside the Docker container.
- **Action:** It starts the server in the background, waits 2 seconds, and runs the client `list` command.
- **Result:** Confirmed that the Client-Server handshake (gRPC) works correctly in the new environment.

# Key Takeaways: Phase 3 (Modern C++ & Code Quality)

## 5. Logging Modernization: From Macros to spdlog

### Challenge
The project originally relied on a custom `dfs_log` macro that manually wrote to `std::cerr`. This approach was:
- **Inflexible:** Difficult to route logs to files or other sinks.
- **Non-Standard:** Lacked standard features like timestamping, thread IDs, and color coding.
- **Hard to Maintain:** Required manual formatting of log messages.

### Solution
We integrated **spdlog**, a fast, header-only/compiled C++ logging library.
- **Implementation:** We created a wrapper class around `spdlog` that mimics the existing `dfs_log` interface. This allowed us to modernize the underlying logging engine without rewriting thousands of lines of existing logging code.
- **Key Feature:** We added a "Verification Mode" controlled by an environment variable (`DFS_VERIFY_LOGS`), allowing us to validate the logging system's behavior (levels, formatting) without recompiling the application.
- **Outcome:** The project now features professional-grade, thread-safe logging with minimal code disruption.

## 6. Concurrency Optimization: Fixing Busy Waits

### Challenge
The server's queue processing logic (`ProcessQueuedRequests`) was implemented as a `while(true)` busy-wait loop.
- **Impact:** This consumed unnecessary CPU cycles, constantly locking and unlocking the mutex even when no requests were pending.
- **Legacy Code:** Despite the documentation claiming usage of `pthread`, the codebase had already been partially migrated to `std::thread` and `std::mutex`, but lacked proper signaling mechanisms.

### Solution
We implemented a **Producer-Consumer pattern** using `std::condition_variable`.
- **Producer (`RequestCallback`):** Notifies the condition variable (`queue_cv.notify_one()`) whenever a new request is added to the queue.
- **Consumer (`ProcessQueuedRequests`):** Uses `queue_cv.wait(lock, predicate)` to sleep until the queue is non-empty.
- **Outcome:** The server now sleeps efficiently when idle, significantly reducing CPU usage while maintaining high responsiveness.
