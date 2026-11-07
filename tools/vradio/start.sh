#!/bin/sh
set -eu
mode="${VRADIO_MODE:-lua}"
sd=/tmp/sd
rm -rf "$sd"
cp -R /opt/vradio/sd "$sd"
port_mode=LUA
[ "$mode" = firmware ] && port_mode=AIRCAST
printf 'serialPort: \n   AUX1:\n      mode: %s\n      power: 0\n' "$port_mode" >> "$sd/RADIO/radio.yml"
if [ "$mode" = lua ]; then
  mkdir -p "$sd/SCRIPTS/TOOLS"
  cp "${VRADIO_SCRIPT:-/script/aircast-usb.lua}" "$sd/SCRIPTS/TOOLS/aircast-usb.lua"
fi
exec /opt/vradio/vradio --sd "$sd" "$@"
