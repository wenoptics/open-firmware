# Dockerfile for Scribit Firmware Compilation Environment
# Headless Docker image for compiling Scribit firmware with Arduino CLI

FROM --platform=linux/amd64 ubuntu:20.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV ARDUINO_USER_DIR=/root/Arduino
ENV ARDUINO15_DIR=/root/.arduino15
ENV ARDUINO_CLI_VERSION=1.3.1
ENV PATH="/usr/local/bin:${PATH}"

# Install system dependencies
RUN apt-get update && apt-get install -y \
    curl \
    unzip \
    build-essential \
    ca-certificates \
    python3 \
    python3-pip \
    python-is-python3 \
    && rm -rf /var/lib/apt/lists/*

# Create Arduino directories
RUN mkdir -p ${ARDUINO_USER_DIR}/libraries ${ARDUINO15_DIR}/packages

# Install Arduino CLI
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh -s ${ARDUINO_CLI_VERSION}

# Set working directory
WORKDIR /workspace

# Copy scripts and make them executable
COPY docker/setup-arduino.sh /usr/local/bin/setup-arduino
RUN chmod +x /usr/local/bin/setup-arduino

# Run Arduino CLI setup
RUN setup-arduino

# Copy hardware override files to correct locations
COPY ExtraFile/8MB_ffat.csv ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/tools/partitions/
COPY ExtraFile/8MB_spiffs.csv ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/tools/partitions/
COPY ExtraFile/SERCOM.cpp ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/cores/samd21/

# Create build output directory
RUN mkdir -p /workspace/builds

# Default command shows usage
CMD ["arduino-cli", "compile", "--help"]