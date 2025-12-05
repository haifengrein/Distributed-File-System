# Project Transformation Plan: DFS to Production-Grade System

This document tracks the roadmap for transforming the academic Distributed File System project into a high-competitiveness resume portfolio piece.

**Goal:** Demonstrate System Programming expertise (Modern C++, Concurrency, Architecture), Engineering standards (CMake, Docker, TDD), and Observability.

**Methodology:** Strict **Test-Driven Development (TDD)**. No feature or refactoring is considered complete without a corresponding passing unit test.

---

## Phase 1: Environment Modernization (Critical Foundation)
*Objective: Replicate and upgrade the legacy build environment to a standard, production-grade Docker container.*

- [x] **Analysis**:
    - [x] Analyze `environment-primary` to identify legacy dependencies.
- [x] **Containerization**:
    - [x] Create a modern `Dockerfile` (Ubuntu 22.04 LTS).
    - [x] Install standard `grpc` and `protobuf` from official repositories.
    - [x] Create `docker-compose.yml`.
- [x] **Build System Modernization**:
    - [x] Create `CMakeLists.txt` to replace `Makefile`.
    - [x] Use modern CMake targets.
- [x] **Verification**:
    - [x] Compiling the *existing* `part2` code in the new Docker environment.

## Phase 2: Structural Refactoring ("De-Studentification")
*Objective: Remove "coursework" artifacts and establish a standard C++ project structure.*

- [x] **Repo Cleanup**:
    - [x] Create root-level `src/`, `include/`, and `tests/` directories.
    - [x] Merge `part2` code into the root as the main codebase.
    - [x] Remove `bin/`, `mnt/`, `obj/` from version control.
- [x] **Dependency Management**:
    - [x] Use system installed libs but document clearly.

## Phase 3: Modern C++ & Concurrency
*Objective: Demonstrate mastery of C++14/17 standards and remove legacy C-style patterns.*

- [x] **Logging Modernization**:
    - [x] Integrate `spdlog` library.
    - [x] Create a `Logger` wrapper class.
    - [x] Replace `std::cout`/`printf` with structured logging.
- [x] **Concurrency Upgrade**:
    - [x] Replace `pthread_create` with `std::thread`.
    - [x] Replace `pthread_mutex_t` with `std::mutex` and `std::unique_lock`.
    - [x] Replace `pthread_cond_t` with `std::condition_variable` (Fixed busy wait).
    - [x] **TDD**: Added `test_concurrency.cpp` to verify Producer-Consumer pattern.

---

## Phase 4: Architectural Refactoring (The "Great Decomposition")
*Objective: Break the monolithic "God Classes" (`DFSServiceImpl`, `DFSClientNode`) into testable, single-responsibility components. This is the core "Scale" refactoring.*

### 4.1 Code Style & Consistency (Prerequisite)
*Why: Standardize formatting now to prevent massive diff noise during structural changes.*
- [ ] **Setup**:
    - [ ] Create `.clang-format` (Google Style).
    - [ ] Apply formatting to all files.

### 4.2 Storage Layer Abstraction
*Why: Decouple File I/O from Business Logic to enable mocking.*
- [ ] **TDD - Interface Definition**:
    - [ ] Define `IStorageEngine` interface (Read, Write, Stat, Delete).
    - [ ] Create `MockStorageEngine` using GMock.
- [ ] **Implementation**:
    - [ ] Implement `PosixStorageEngine` (moves actual I/O code from `DFSServiceImpl`).
    - [ ] **Test**: Unit test `PosixStorageEngine` with temporary files.
- [ ] **Integration**:
    - [ ] Inject `IStorageEngine` into `DFSServiceImpl`.

### 4.3 Lock Manager Extraction
*Why: Centralize concurrency control, making it testable and replaceable (e.g., for distributed locks later).*
- [ ] **TDD - Locking Logic**:
    - [ ] Create `test_lock_manager.cpp`.
    - [ ] Write tests for: Acquire, Release, Conflict detection, Timeout (if applicable).
- [ ] **Implementation**:
    - [ ] Extract `LockManager` class from `DFSServiceImpl`.
    - [ ] Use `std::shared_mutex` (C++17) for Read/Write lock optimization if appropriate.
- [ ] **Integration**:
    - [ ] Replace raw mutex/map logic in `DFSServiceImpl` with `LockManager`.

### 4.4 Server Service Decomposition
*Why: The gRPC Service should only act as a "Controller", delegating logic to domain objects.*
- [ ] **TDD - Service Logic**:
    - [ ] Create `test_dfs_service.cpp` using `MockStorageEngine` and `MockLockManager`.
    - [ ] Verify: Request flow -> Lock Acquire -> Storage Write -> Lock Release.
- [ ] **Refactoring**:
    - [ ] Strip `DFSServiceImpl` down to just gRPC mapping.
    - [ ] Move core logic into a `DFSController` or domain classes.

### 4.5 Client-Side Synchronization Engine
*Why: The client logic (Watcher vs gRPC vs Local File) is currently a tangle of race conditions.*
- [ ] **TDD - Sync Logic**:
    - [ ] Define `ISyncStrategy` (e.g., `LastWriteWinsStrategy`).
    - [ ] Write tests for conflict scenarios (Server newer, Client newer, Same).
- [ ] **Implementation**:
    - [ ] Extract `SyncEngine` from `DFSClientNode`.
    - [ ] Decouple `InotifyWatcher` from the main loop (Event Bus pattern?).

---

## Phase 5: Memory Management & Safety
*Objective: Ensure Zero-Leak policy and strict ownership semantics.*

- [ ] **Smart Pointer Migration**:
    - [ ] **TDD**: Verify destructor calls in Mocks during component tests.
    - [ ] Replace `new`/`delete` with `std::unique_ptr` for `StorageEngine`, `LockManager`, etc.
    - [ ] Use `std::shared_ptr` only where ownership is truly shared.
- [ ] **Leak Detection**:
    - [ ] Run full test suite under `valgrind` or `ASan` (AddressSanitizer) in CI/Docker.

---

## Phase 6: Advanced Logic & Optimization
*Objective: Enhance reliability and performance.*

- [ ] **Streaming & Chunking**:
    - [ ] **TDD**: Test `StoreFile` with file size > RAM size.
    - [ ] Refactor `StoreFile`/`FetchFile` to use streaming iterators instead of loading full buffers.
- [ ] **Checksum Strategy**:
    - [ ] Extract `CRC` logic into a `ChecksumService`.
    - [ ] **Test**: Verify collisions/mismatches.

---

## Phase 7: Visualization & Observability
*Objective: The "Resume Hooks".*

- [ ] **Visualization**:
    - [ ] Script a "Dropbox-like" sync scenario.
    - [ ] Record GIF for README.
- [ ] **Distributed Tracing (OpenTelemetry)**:
    - [ ] Integrate `opentelemetry-cpp`.
    - [ ] Instrument `LockManager` (wait times) and `StorageEngine` (IO times).
    - [ ] Generate Trace Waterfall screenshots.