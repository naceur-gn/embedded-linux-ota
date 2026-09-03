#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

echo "=== OTA Client Installation ==="

if [ ! -f "$BUILD_DIR/ota-client" ]; then
    echo "Error: Build not found. Run 'cmake --build build' first."
    exit 1
fi

echo "Installing binaries..."
sudo cp "$BUILD_DIR/ota-client" /opt/ota/bin/
sudo cp "$BUILD_DIR/ota-device-info" /opt/ota/bin/
sudo chown root:root /opt/ota/bin/ota-client
sudo chown root:root /opt/ota/bin/ota-device-info
sudo chmod 755 /opt/ota/bin/ota-client
sudo chmod 755 /opt/ota/bin/ota-device-info

echo "Installing configuration..."
sudo cp "$PROJECT_DIR/configs/device.conf" /etc/ota/
sudo chown root:root /etc/ota/device.conf
sudo chmod 644 /etc/ota/device.conf

echo "Installing systemd service..."
sudo cp "$PROJECT_DIR/systemd/ota-client.service" /etc/systemd/system/
sudo systemctl daemon-reload

echo "Installation complete."
echo ""
echo "To start the service:"
echo "  sudo systemctl start ota-client"
echo ""
echo "To enable on boot:"
echo "  sudo systemctl enable ota-client"
