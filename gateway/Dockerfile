FROM python:3.10-slim

# Install uv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /bin/uv

# Set working directory
WORKDIR /app

# Install system dependencies (if any needed for grpc)
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Copy dependency definition
COPY gateway/pyproject.toml /app/

# Install dependencies using uv
# We install into the system python environment for simplicity in Docker
RUN uv pip install --system --requirements pyproject.toml

# Copy Proto definitions
# Note: This requires the build context to be the project root
COPY protos/ /app/protos/

# Generate Python gRPC code
# We put the generated code into a 'proto_gen' package inside src
RUN mkdir -p /app/src/proto_gen && \
    python -m grpc_tools.protoc \
    -I /app/protos \
    --python_out=/app/src/proto_gen \
    --grpc_python_out=/app/src/proto_gen \
    /app/protos/dfs-service.proto

# Fix imports in generated code (common issue with protoc)
# The generated grpc file imports 'dfs_service_pb2', but now it's in proto_gen.
# We might need to adjust PYTHONPATH or use relative imports. 
# A cleaner way is to add /app/src/proto_gen to PYTHONPATH.
ENV PYTHONPATH="${PYTHONPATH}:/app/src/proto_gen"

# Copy Source Code
COPY gateway/src/ /app/src/

# Set workdir to src so imports work nicely
WORKDIR /app/src

# Expose port
EXPOSE 8000

# Run the application
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
