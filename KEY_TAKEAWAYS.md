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

---

## Next Steps
With the foundation solid, we are ready to proceed to **Phase 2: Structural Refactoring**, where we will dismantle the `part1/part2` directory structure and enforce standard C++ project layout (`src/`, `include/`).
