# Project Transformation Plan: DFS to Production-Grade System

This document tracks the roadmap for transforming the academic Distributed File System project into a high-competitiveness resume portfolio piece.

**Goal:** Demonstrate System Programming expertise (Modern C++, Concurrency), Engineering standards (CMake, Docker, CI/CD), and Observability (Distributed Tracing).

## Phase 1: Environment Modernization (Critical Foundation)
*Objective: Replicate and upgrade the legacy build environment to a standard, production-grade Docker container.*

- [x] **Analysis**:
    - [x] Analyze `environment-primary` to identify legacy dependencies (Ubuntu 20.04, custom gRPC PPA).
- [ ] **Containerization**:
    - [x] Create a modern `Dockerfile` (Ubuntu 22.04 LTS).
    - [x] Install standard `grpc` and `protobuf` from official repositories (removing reliance on course-specific PPAs).
    - [x] Create `docker-compose.yml` for consistent building/testing.
- [x] **Build System Modernization**:
    - [x] Create `CMakeLists.txt` to replace `Makefile`.
    - [x] Use modern CMake targets (`protobuf::libprotobuf`, `gRPC::grpc++`).
- [x] **Verification**:
    - [x] Compiling the *existing* `part2` code in the new Docker environment.
    - [x] Fix immediate compilation errors caused by library version jumps (e.g., gRPC API changes).

## Phase 2: Structural Refactoring ("De-Studentification")
*Objective: Remove "coursework" artifacts and establish a standard C++ project structure.*

- [x] **Repo Cleanup**:
    - [x] Create root-level `src/`, `include/`, and `tests/` directories.
    - [x] Merge `part2` code into the root as the main codebase (discard `part1`).
    - [x] Remove `bin/`, `mnt/`, `obj/` from version control (update `.gitignore`).
- [x] **Dependency Management**:
    - [x] (Optional) Introduce `vcpkg` or `conan` for managing dependencies (gRPC, Protobuf, spdlog), or rely on system installed libs but document clearly.

## Phase 3: Modern C++ & Code Quality
*Objective: Demonstrate mastery of C++14/17 standards and remove legacy C-style patterns.*

- [ ] **Logging Modernization**:
    - [ ] Integrate `spdlog` library (header-only or compiled).
    - [ ] Create a `Logger` wrapper class to replace the custom `dfs_log` macro.
    - [ ] Replace `std::cout`/`printf` with structured logging (`spdlog::info`, `spdlog::error`).
- [ ] **Concurrency Upgrade**:
    - [ ] Replace `pthread_create` with `std::thread`.
    - [ ] Replace `pthread_mutex_t` with `std::mutex` and `std::unique_lock`/`std::lock_guard`.
    - [ ] Replace `pthread_cond_t` with `std::condition_variable`.
- [ ] **Memory Management**:
    - [ ] Audit code for `new`/`delete`.
    - [ ] Replace raw pointers with `std::unique_ptr` (for exclusive ownership) or `std::shared_ptr`.
- [ ] **Style Consistency**:
    - [ ] Add `.clang-format` file (Google or LLVM style).
    - [ ] Apply formatting to all source files.

## Phase 3: Core Logic Optimization (Preparation for Visualization)
*Objective: Ensure the synchronization logic is robust enough for a smooth "Dropbox-like" demo.*

- [ ] **Reliability Check**:
    - [ ] Stress test the `inotify` (Linux) or `FSEvents` (macOS) watcher loop.
    - [ ] Ensure the "Last Write Wins" logic handles rapid file updates without crashing.
- [ ] **Large File Handling (Optional but Recommended)**:
    - [ ] Implement file chunking (e.g., 64MB chunks) to avoid loading entire files into RAM.
    - [ ] Update Protobuf definitions to support streaming file transfer.

## Phase 4: Visualization Type 1 ("The Dropbox Experience")
*Objective: Create an immediate visual hook for the README.*

- [ ] **Scenario Scripting**:
    - [ ] Design a simple demo script:
        1. Start Server.
        2. Start Client A (Folder A).
        3. Start Client B (Folder B).
        4. Drag image into Folder A -> Appears in Folder B.
        5. Edit text file in Folder B -> Updates in Folder A.
        6. (Bonus) Cut network (stop server), modify file, restore network, observe sync.
- [ ] **Recording**:
    - [ ] Set up split-screen view (Terminal + File Explorer windows).
    - [ ] Record screen (OBS or QuickTime).
- [ ] **Production**:
    - [ ] Convert video to high-quality, optimized GIF.
    - [ ] Add to `README.md` header.

## Phase 5: Engineering Excellence
*Objective: Show ability to work in modern DevOps environments.*

- [ ] **Containerization**:
    - [ ] Write `Dockerfile` for Server.
    - [ ] Write `Dockerfile` for Client.
    - [ ] Create `docker-compose.yml` to spin up a 1-Server / 2-Client environment with one command.
- [ ] **Testing**:
    - [ ] Integrate `GoogleTest` framework via CMake.
    - [ ] Write Unit Tests for:
        - [ ] File locking logic.
        - [ ] CRC checksum calculation.
        - [ ] Metadata serialization.

## Phase 6: Advanced Observability (Visualization Type 4)
*Objective: The "Killer Feature" - Distributed Tracing.*

- [ ] **Integration**:
    - [ ] Add `opentelemetry-cpp` dependency.
    - [ ] Configure an OTLP exporter.
- [ ] **Instrumentation**:
    - [ ] Add Traces to `StoreFile`: Measure time for Lock -> Write -> Unlock.
    - [ ] Add Traces to `FetchFile`: Measure time for Read -> Stream.
    - [ ] Add Traces to Async Notification: Measure propagation latency.
- [ ] **Infrastructure**:
    - [ ] Add Jaeger (or Zipkin) to `docker-compose.yml`.
- [ ] **Documentation**:
    - [ ] Capture screenshots of a trace waterfall (showing network latency vs disk I/O).
    - [ ] Add "Observability" section to `README.md` explaining how to debug the system using traces.
