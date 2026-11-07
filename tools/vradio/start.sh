#!/bin/sh
set -eu
mode="${VRADIO_MODE:-lua}"
sd=/tmp/sd
rm -rf "$sd"
cp -R /opt/vradio/sd "$sd"
port_mode=LUA
[ "$mode" = firmware ] && port_mode=CHANNELS
printf 'serialPort: \n   AUX1:\n      mode: %s\n      power: 0\n' "$port_mode" >> "$sd/RADIO/radio.yml"
if [ -n "${VRADIO_MODEL:-}" ]; then
  cp "$VRADIO_MODEL" "$sd/MODELS/model1.yml"
fi
if [ "$mode" = lua ]; then
  script="${VRADIO_SCRIPT:-/script/aircast-usb.lua}"
  mkdir -p "$sd/SCRIPTS/TOOLS"
  cp "$script" "$sd/SCRIPTS/TOOLS/$(basename "$script")"
fi
exec /opt/vradio/vradio --sd "$sd" "$@"
