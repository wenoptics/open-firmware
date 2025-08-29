#!/bin/bash

# Build script for Scribit Firmware Compilation Environment

set -e

echo "Building Scribit Firmware Compilation Environment..."

# Check if Docker is running
if ! docker info > /dev/null 2>&1; then
    echo "Error: Docker is not running. Please start Docker and try again."
    exit 1
fi

# Build the Docker image
echo "Building headless Docker image for Arduino CLI compilation..."
docker-compose -f docker/docker-compose.yml build

echo "Build completed successfully!"
echo ""
echo "🚀 Usage Examples:"
echo ""
echo "# Compile ESP32 firmware:"
echo "  docker-compose -f docker/docker-compose.yml run --rm scribit-firmware esp32"
echo ""
echo "# Compile SAMD firmware:"
echo "  docker-compose -f docker/docker-compose.yml run --rm scribit-firmware samd"
echo ""
echo "# Compile both firmware types:"
echo "  docker-compose -f docker/docker-compose.yml run --rm scribit-firmware both"
echo ""
echo "# Verify setup:"
echo "  docker-compose -f docker/docker-compose.yml run --rm scribit-firmware bash -c verify-setup"
echo ""
echo "# Interactive shell:"
echo "  docker-compose -f docker/docker-compose.yml run --rm scribit-firmware bash"
echo ""
echo "📁 Build outputs will be saved to Docker volume 'firmware-builds'"
echo "   You can access them by mounting the volume or copying from the container."