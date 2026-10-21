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

#include "aircast.h"

#include <stdio.h>

#include "edgetx.h"
#include "os/timer.h"
#include "tasks/mixer_task.h"

#define AIRCAST_CHANNEL_COUNT 24
#define AIRCAST_SEND_PERIOD_MS 20

static const etx_serial_driver_t* aircastSerialDrv = nullptr;
static void* aircastSerialCtx = nullptr;
static timer_handle_t aircastTimer = TIMER_INITIALIZER;

static void aircastSendLine()
{
  auto sendByte = aircastSerialDrv->sendByte;
  if (!sendByte) return;

  auto put = [sendByte](char c) { sendByte(aircastSerialCtx, (uint8_t)c); };

  put('$');
  for (uint8_t i = 0; i < AIRCAST_CHANNEL_COUNT; i++) {
    char num[8];
    int len = snprintf(num, sizeof(num), "%d,", channelOutputs[i]);
    for (int j = 0; j < len; j++) put(num[j]);
  }
  put('\r');
  put('\n');
}

static void aircastTimerCb(timer_handle_t*)
{
  if (mixerTaskRunning()) aircastSendLine();
}

static void aircastStart()
{
  if (!timer_is_created(&aircastTimer)) {
    timer_create(&aircastTimer, aircastTimerCb, "aircast", AIRCAST_SEND_PERIOD_MS, true);
  }
  timer_start(&aircastTimer);
}

static void aircastStop()
{
  timer_stop(&aircastTimer);
}

void aircastSetSerialDriver(void* ctx, const etx_serial_driver_t* drv)
{
  aircastSerialCtx = ctx;
  aircastSerialDrv = drv;

  if (drv) {
    aircastStart();
  } else {
    aircastStop();
  }
}
