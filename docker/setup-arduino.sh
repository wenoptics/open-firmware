#!/bin/bash

# Arduino CLI setup script
# This script configures Arduino CLI and installs required board packages

set -e

echo "Setting up Arduino CLI..."

# Initialize Arduino CLI configuration
arduino-cli config init

# Add board manager URLs
arduino-cli config add board_manager.additional_urls https://www.briki.org/download/resources/package_briki_index.json
arduino-cli config add board_manager.additional_urls https://dl.espressif.com/dl/package_esp32_dev_index.json

# Update board index
arduino-cli core update-index

# Install board packages
arduino-cli core install briki:mbc-wb@2.0.0
# arduino-cli core install esp32:esp32

echo "Arduino CLI setup complete!"
