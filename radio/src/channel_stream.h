/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#pragma once

#include "hal/serial_driver.h"

// Streams the model's mixed channel outputs over USB-VCP in the text
// protocol the Aircast app already reads from its EdgeTX Lua script
// (aircast-usb.lua): "$<ch1>,<ch2>,...,<ch24>,\r\n", repeated at a fixed
// rate while the port is set to UART_MODE_CHANNELS. Keeping the wire format
// identical means the app side needs no change to fly from this firmware
// mode instead of the script.
#define CHANNEL_STREAM_BAUDRATE 115200

void channelStreamSetSerialDriver(void* ctx, const etx_serial_driver_t* drv);
void channelStreamModelLoaded();
