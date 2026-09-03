#!/bin/bash
set -e

OTA_USER="ota"
OTA_GROUP="ota"

if ! id "$OTA_USER" &>/dev/null; then
    echo "Creating system user: $OTA_USER"
    sudo useradd --system --no-create-home --shell /usr/sbin/nologin "$OTA_USER"
fi

if ! getent group "$OTA_GROUP" &>/dev/null; then
    echo "Creating group: $OTA_GROUP"
    sudo groupadd --system "$OTA_GROUP"
fi

if ! id -nG "$OTA_USER" | grep -qw "$OTA_GROUP"; then
    echo "Adding $OTA_USER to group $OTA_GROUP"
    sudo usermod -aG "$OTA_GROUP" "$OTA_USER"
fi

echo "Setting up directories..."

sudo mkdir -p /opt/ota/bin
sudo mkdir -p /etc/ota
sudo mkdir -p /var/lib/ota
sudo mkdir -p /var/lib/ota/downloads
sudo mkdir -p /var/log/ota

sudo chown "$OTA_USER:$OTA_GROUP" /opt/ota
sudo chown "$OTA_USER:$OTA_GROUP" /opt/ota/bin
sudo chown root:root /etc/ota
sudo chmod 755 /etc/ota
sudo chown "$OTA_USER:$OTA_GROUP" /var/lib/ota
sudo chmod 700 /var/lib/ota
sudo chown "$OTA_USER:$OTA_GROUP" /var/lib/ota/downloads
sudo chmod 700 /var/lib/ota/downloads
sudo chown "$OTA_USER:$OTA_GROUP" /var/log/ota
sudo chmod 750 /var/log/ota

echo "User and directory setup complete."
