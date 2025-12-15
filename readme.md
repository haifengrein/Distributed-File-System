
# Distributed File System (DFS)

A C++17 distributed file system built on gRPC/Protocol Buffers. The focus is on correctness under concurrency (locks, callbacks, file watchers) and on making the system easy to run locally via Docker and scripts.

## What’s implemented

- **Core file ops**: `mount`, `fetch`, `store`, `delete`, `list`, `stat` via gRPC/Protobuf.
- **Client-side cache + change detection**: CRC-based detection and “last write wins” conflict strategy.
- **Server-side write lock**: single-writer semantics to avoid concurrent write corruption.
- **Async callbacks**: server-to-client notifications (gRPC async) to keep caches in sync.
- **Multi-threaded runtime**: gRPC async threads + client watcher threads (`inotify`).
- **Observability hooks (optional)**: an event-driven layer (`EventBus`) and a monitor service for streaming runtime events.

## Architecture (high level)

- **Client (`dfs-client`)** mounts a local directory, watches for file changes, and turns changes into RPCs.
- **Server (`dfs-server`)** stores files under a mount directory, enforces write locks, and pushes callback events.
- **Shared logic** lives in `src/common/` and `include/`.

## Run with Docker (recommended)

This repo includes a compose setup that brings up the full dev stack: DFS server + demo agent + gateway + dashboard.

1) Build the C++ binaries once (inside the dev container image):

```bash
docker compose run --rm dev bash -lc "cmake -S . -B build && cmake --build build -j"
```

2) Start the stack:

```bash
docker compose up --build
```

- Dashboard: `http://localhost:5173`
- Gateway: `http://localhost:8000`
- gRPC server: `localhost:50051`

Production-style compose (optional):

```bash
docker compose -f docker-compose.prod.yml up --build
```

## Build (local)

Dependencies: CMake (>= 3.15), a C++17 compiler, gRPC, Protobuf, `spdlog`, pthreads. Tests are built when GTest is available.

```bash
cmake -S . -B build
cmake --build build -j
```

## Run (local)

The DFS uses mount directories under `mnt/` by default. These are runtime data directories and are intentionally not meant to be versioned.

```bash
mkdir -p mnt/server mnt/client
./build/bin/dfs-server -a 0.0.0.0:50051 -m mnt/server -n 4
```

In another terminal:

```bash
./build/bin/dfs-client -a 127.0.0.1:50051 -m mnt/client mount
```

For CLI help:

```bash
./build/bin/dfs-server -h
./build/bin/dfs-client -h
```

## Repository layout

- `src/`, `include/`: DFS core implementation (client/server/common).
- `protos/`: gRPC service definition.
- `tests/`: unit/integration tests and demos.
- `gateway/`, `dashboard/`: optional observability/demo UI stack.
