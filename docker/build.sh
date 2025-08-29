#!/bin/bash

# Build script for Scribit Firmware Development Environment

set -e

echo "Building Scribit Firmware Development Environment..."

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    echo "Error: Docker is not running. Please start Docker and try again."
    exit 1
fi

# Build the Docker image
echo "Building Docker image..."
docker-compose -f docker/docker-compose.yml build

echo "Build completed successfully!"
echo ""
echo "To start the development environment, run:"
echo "  cd docker && docker-compose up -d"
echo ""
echo "Then access the Arduino IDE via web browser at:"
echo "  http://localhost:6080"
echo ""
echo "VNC Password: scribit123"
echo ""
echo "To test compilation, run inside the container:"
echo "  /root/test_compilation.sh"
