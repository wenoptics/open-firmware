# Dockerfile for Scribit Firmware Compilation Environment
# Headless Docker image for compiling Scribit firmware with Arduino CLI

FROM --platform=linux/amd64 ubuntu:20.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
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

    
ENV ARDUINO_USER_DIR=/root/Arduino
ENV ARDUINO15_DIR=/root/.arduino15
ENV ARDUINO_CLI_VERSION=1.3.1

# Create Arduino directories
RUN mkdir -p ${ARDUINO_USER_DIR}/libraries ${ARDUINO15_DIR}/packages

# Install Arduino CLI
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh -s ${ARDUINO_CLI_VERSION}

# Install briki:mbc-wb board package
RUN arduino-cli config init && \
    arduino-cli config add board_manager.additional_urls https://www.briki.org/download/resources/package_briki_index.json && \
    arduino-cli config add board_manager.additional_urls https://dl.espressif.com/dl/package_esp32_dev_index.json && \
    arduino-cli core update-index && \
    # Install board packages
    arduino-cli core install briki:mbc-wb@2.0.0

# Copy hardware override files
RUN test -f ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/tools/partitions/8MB_ffat.csv
COPY ExtraFile/8MB_ffat.csv ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/tools/partitions/

RUN test -f ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/tools/partitions/8MB_spiffs.csv
COPY ExtraFile/8MB_spiffs.csv ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/tools/partitions/

RUN test -f ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/cores/samd21/SERCOM.cpp
COPY ExtraFile/SERCOM.cpp ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/cores/samd21/

# Set working directory
WORKDIR /workspace

# Create build output directory
RUN mkdir -p /workspace/builds

# Default command shows usage
CMD ["arduino-cli", "compile", "--help"]
