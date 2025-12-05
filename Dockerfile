# Use Ubuntu 22.04 LTS as the base image for a production-grade environment
FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install essential build tools and dependencies
# - build-essential: gcc, g++, make, etc.
# - cmake: Modern build system
# - git: Version control
# - protobuf-compiler, libprotobuf-dev: Protocol Buffers
# - libgrpc++-dev, protobuf-compiler-grpc: gRPC C++ libraries and compiler plugin
# - clang-format: For code formatting
# - valgrind: For memory leak detection (optional but good practice)
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    protobuf-compiler \
    libprotobuf-dev \
    libgrpc++-dev \
    protobuf-compiler-grpc \
    pkg-config \
    libspdlog-dev \
    libgtest-dev \
    clang-format \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

# Create a working directory
WORKDIR /app

# Copy the current directory contents into the container at /app
COPY . /app

# Default command to keep the container running (or you can change this to build instructions)
CMD ["/bin/bash"]
