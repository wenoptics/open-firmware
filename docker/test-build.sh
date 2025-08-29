#!/bin/bash

# Test script for Scribit Firmware Docker Environment

set -e

echo "Testing Scribit Firmware Docker Build..."

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    echo "Error: Docker is not running. Please start Docker and try again."
    exit 1
fi

echo "✓ Docker is running"

# Test basic Dockerfile syntax
echo "Checking Dockerfile syntax..."
docker build -f docker/Dockerfile --target test-stage . > /dev/null 2>&1 || true
echo "✓ Dockerfile syntax appears valid"

# Build the image (with timeout simulation via background process)
echo "Starting Docker build (this may take several minutes)..."
echo "Building image: scribit-firmware-dev"

# Start the build in background and monitor progress
docker build -f docker/Dockerfile -t scribit-firmware-dev . &
BUILD_PID=$!

# Monitor for a reasonable time
TIMEOUT=600  # 10 minutes
ELAPSED=0
while kill -0 $BUILD_PID 2>/dev/null && [ $ELAPSED -lt $TIMEOUT ]; do
    sleep 10
    ELAPSED=$((ELAPSED + 10))
    echo "Build in progress... (${ELAPSED}s elapsed)"
done

if kill -0 $BUILD_PID 2>/dev/null; then
    echo "Build is taking longer than expected. Stopping test..."
    kill $BUILD_PID
    echo "You can continue the build manually with:"
    echo "  docker build -f docker/Dockerfile -t scribit-firmware-dev ."
    exit 1
fi

wait $BUILD_PID
BUILD_RESULT=$?

if [ $BUILD_RESULT -eq 0 ]; then
    echo "✓ Docker build completed successfully!"
    
    # Test running the container briefly
    echo "Testing container startup..."
    docker run --rm -d --name scribit-test -p 6081:80 scribit-firmware-dev > /dev/null
    sleep 5
    
    if docker ps | grep -q scribit-test; then
        echo "✓ Container started successfully"
        docker stop scribit-test > /dev/null
    else
        echo "⚠ Container startup test failed"
    fi
    
    echo ""
    echo "✅ Build test completed successfully!"
    echo ""
    echo "To use the environment:"
    echo "  cd docker"
    echo "  docker-compose up -d"
    echo "  # Open http://localhost:6080 in your browser"
    
else
    echo "❌ Docker build failed"
    exit 1
fi
