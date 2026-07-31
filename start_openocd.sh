#!/bin/bash
# Sla op als start_openocd.sh

echo "Stopping conflicting processes..."
sudo pkill -9 -f openocd 2>/dev/null
sudo pkill -9 -f esptool 2>/dev/null
lsof 2>/dev/null | grep usbmodem | awk '{print $2}' | xargs sudo kill -9 2>/dev/null

echo "Unloading conflicting drivers..."
sudo kextunload -b com.apple.driver.AppleUSBSerialDriver 2>/dev/null

echo "Waiting for USB to stabilize..."
sleep 2

echo "Starting OpenOCD..."
sudo ~/.platformio/packages/tool-openocd-esp32/bin/openocd \
  -f board/esp32s3-builtin.cfg
