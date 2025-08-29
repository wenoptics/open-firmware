# Dockerfile for Scribit Open Firmware Compilation Environment
# Based on Ubuntu Desktop with VNC access for GUI-based Arduino IDE

FROM --platform=linux/amd64 wenoptics/ubuntu-desktop-novnc:latest

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV ARDUINO_IDE_VERSION=1.8.19
ENV ARDUINO_INSTALL_DIR=/opt/arduino
ENV ARDUINO_USER_DIR=/root/Arduino
ENV ARDUINO15_DIR=/root/.arduino15

# Remove problematic Chrome repository and update system
RUN rm -f /etc/apt/sources.list.d/google-chrome.list && \
    apt-get update && apt-get install -y \
    wget \
    curl \
    unzip \
    xz-utils \
    openjdk-8-jdk \
    python3 \
    python3-pip \
    git \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

# Set JAVA_HOME for Arduino IDE
ENV JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64

# Download and install Arduino Legacy IDE 1.8.19
RUN cd /tmp && \
    wget https://downloads.arduino.cc/arduino-${ARDUINO_IDE_VERSION}-linux64.tar.xz && \
    tar -xf arduino-${ARDUINO_IDE_VERSION}-linux64.tar.xz && \
    mv arduino-${ARDUINO_IDE_VERSION} ${ARDUINO_INSTALL_DIR} && \
    rm arduino-${ARDUINO_IDE_VERSION}-linux64.tar.xz

# Create Arduino directories
RUN mkdir -p ${ARDUINO_USER_DIR}/libraries && \
    mkdir -p ${ARDUINO15_DIR}/packages && \
    mkdir -p ${ARDUINO15_DIR}/preferences

# Copy the entire project to the container
COPY . /workspace/open-firmware

# Set working directory
WORKDIR /workspace/open-firmware

# Create Arduino preferences file with board URLs
RUN echo "boardsmanager.additional.urls=https://www.briki.org/download/resources/package_briki_index.json,https://dl.espressif.com/dl/package_esp32_dev_index.json" > ${ARDUINO15_DIR}/preferences.txt && \
    echo "compiler.warning_level=none" >> ${ARDUINO15_DIR}/preferences.txt && \
    echo "editor.font=Monospaced,plain,12" >> ${ARDUINO15_DIR}/preferences.txt

# Install board packages using Arduino CLI (more reliable than GUI for automation)
RUN cd /tmp && \
    curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh && \
    mv bin/arduino-cli /usr/local/bin/ && \
    arduino-cli config init && \
    arduino-cli config add board_manager.additional_urls https://www.briki.org/download/resources/package_briki_index.json && \
    arduino-cli config add board_manager.additional_urls https://dl.espressif.com/dl/package_esp32_dev_index.json && \
    arduino-cli core update-index && \
    arduino-cli core install briki:mbc-wb@2.0.0 && \
    arduino-cli core install esp32:esp32

# Copy hardware override files to correct locations
RUN mkdir -p ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/tools/partitions && \
    mkdir -p ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/cores/samd21 && \
    cp ExtraFile/8MB_ffat.csv ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/tools/partitions/ && \
    cp ExtraFile/8MB_spiffs.csv ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/tools/partitions/ && \
    cp ExtraFile/SERCOM.cpp ${ARDUINO15_DIR}/packages/briki/hardware/mbc-wb/2.0.0/cores/samd21/

# Copy libraries to firmware directory
RUN cp -r ExtraFile/arduino-mqtt Firmware/ScribitESP/ && \
    cp -r ExtraFile/StepperDriver Firmware/ScribitESP/

# Copy example configuration files to proper locations for compilation
RUN cp ExtraFile/SIConfig.hpp.example Firmware/ScribitESP/SIConfig.hpp && \
    cp ExtraFile/Mk4duoVersion.h.example Firmware/MK4duo/Mk4duoVersion.h && \
    cp ExtraFile/ScribitVersion.hpp.example Firmware/ScribitESP/ScribitVersion.hpp

# Create desktop shortcuts for Arduino IDE and terminal
RUN mkdir -p /root/Desktop && \
    echo "[Desktop Entry]" > /root/Desktop/Arduino.desktop && \
    echo "Version=1.0" >> /root/Desktop/Arduino.desktop && \
    echo "Type=Application" >> /root/Desktop/Arduino.desktop && \
    echo "Name=Arduino IDE" >> /root/Desktop/Arduino.desktop && \
    echo "Comment=Arduino IDE for firmware compilation" >> /root/Desktop/Arduino.desktop && \
    echo "Exec=${ARDUINO_INSTALL_DIR}/arduino" >> /root/Desktop/Arduino.desktop && \
    echo "Icon=${ARDUINO_INSTALL_DIR}/lib/arduino_icon.ico" >> /root/Desktop/Arduino.desktop && \
    echo "Terminal=false" >> /root/Desktop/Arduino.desktop && \
    echo "Categories=Development;" >> /root/Desktop/Arduino.desktop && \
    chmod +x /root/Desktop/Arduino.desktop

# Copy verification script
COPY docker/verify-setup.sh /root/verify-setup.sh
RUN chmod +x /root/verify-setup.sh

# Create a simple compilation test script for backward compatibility
RUN echo '#!/bin/bash' > /root/test_compilation.sh && \
    echo 'echo "Running verification script..."' >> /root/test_compilation.sh && \
    echo '/root/verify-setup.sh' >> /root/test_compilation.sh && \
    chmod +x /root/test_compilation.sh

# Create a README for the Docker environment
RUN echo "# Scribit Firmware Compilation Environment" > /root/README_DOCKER.md && \
    echo "" >> /root/README_DOCKER.md && \
    echo "This Docker container provides a complete environment for compiling Scribit firmware." >> /root/README_DOCKER.md && \
    echo "" >> /root/README_DOCKER.md && \
    echo "## Quick Start" >> /root/README_DOCKER.md && \
    echo "1. Access the desktop via web browser at http://localhost:6080" >> /root/README_DOCKER.md && \
    echo "2. Double-click the Arduino IDE icon on the desktop" >> /root/README_DOCKER.md && \
    echo "3. Open the firmware projects:" >> /root/README_DOCKER.md && \
    echo "   - ESP32 firmware: /workspace/open-firmware/Firmware/ScribitESP/ScribitESP.ino" >> /root/README_DOCKER.md && \
    echo "   - SAMD firmware: /workspace/open-firmware/Firmware/MK4duo/MK4duo.ino" >> /root/README_DOCKER.md && \
    echo "" >> /root/README_DOCKER.md && \
    echo "## Testing Compilation" >> /root/README_DOCKER.md && \
    echo "Run the test script: /root/test_compilation.sh" >> /root/README_DOCKER.md && \
    echo "" >> /root/README_DOCKER.md && \
    echo "## Board Selection" >> /root/README_DOCKER.md && \
    echo "- For ESP32: Select 'Briki MBC-WB' > 'Scribit ESP32'" >> /root/README_DOCKER.md && \
    echo "- For SAMD: Select 'Briki MBC-WB' > 'Scribit SAMD'" >> /root/README_DOCKER.md

# Set the Arduino IDE path in PATH
ENV PATH="${ARDUINO_INSTALL_DIR}:${PATH}"

# Expose VNC port
EXPOSE 80

# Set default command to start the VNC server
CMD ["/startup.sh"]
