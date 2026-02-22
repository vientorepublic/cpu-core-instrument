#!/bin/bash

# Build the Docker image for cross-compilation
docker build -t cpu-core-instrument-linux-builder -f Dockerfile.cross .

# Run the container to build the project
docker run --rm -v "$(pwd):/app" cpu-core-instrument-linux-builder

echo "Build complete. The Linux executable is located at build_linux/cpu_core_instrument"
