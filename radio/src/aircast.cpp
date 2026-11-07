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

#include "aircast_layout.h"

#include "edgetx.h"
#include "hal/adc_driver.h"
#include "hal/usb_driver.h"
#include "mixes.h"
#include "switches.h"
#include "os/timer.h"

#define AIRCAST_CHANNEL_COUNT 24
#define AIRCAST_SEND_PERIOD_MS 20
#define AIRCAST_LINE_BYTES 192
#define AIRCAST_RX_BYTES 128
#define AIRCAST_HEARD_BYTES 96

static const etx_serial_driver_t* aircastSerialDrv = nullptr;
static void* aircastSerialCtx = nullptr;
static timer_handle_t aircastTimer = TIMER_INITIALIZER;

static uint8_t aircastRx[AIRCAST_RX_BYTES];
static volatile uint8_t aircastRxHead = 0;
static volatile uint8_t aircastRxTail = 0;
static char aircastHeardLine[AIRCAST_HEARD_BYTES];
static uint8_t aircastHeardLen = 0;
static AircastMix aircastMixes[AIRCAST_MIXES];
static uint8_t aircastMixCount = 0;
static bool aircastLayoutKnown = false;

static void aircastSendLine()
{
  auto sendByte = aircastSerialDrv->sendByte;
  if (!sendByte) return;

  char line[AIRCAST_LINE_BYTES];
  int len = snprintf(line, sizeof(line), "$");
  for (uint8_t i = 0; i < AIRCAST_CHANNEL_COUNT; i++) {
    len += snprintf(line + len, sizeof(line) - len, "%d,", channelOutputs[i]);
  }
  len += snprintf(line + len, sizeof(line) - len, "\r\n");

  if (usbSerialFreeSpace() < (uint32_t)len) return;
  for (int i = 0; i < len; i++) sendByte(aircastSerialCtx, (uint8_t)line[i]);
}

static int aircastSource(const char* control)
{
  if (control[0] == 'S' && control[1] >= 'A' && control[1] <= 'Z' && !control[2]) {
    int sw = switchLookupIdx(control[1]);
    return sw < 0 ? -1 : MIXSRC_FIRST_SWITCH + sw;
  }
  for (uint8_t i = 0; i < adcGetMaxInputs(ADC_INPUT_FLEX); i++) {
    const char* label = adcGetInputLabel(ADC_INPUT_FLEX, i);
    if (label && !strcmp(label, control)) return MIXSRC_FIRST_POT + i;
  }
  return -1;
}

static uint8_t aircastFirstMix(uint8_t destCh)
{
  for (uint8_t i = 0; i < MAX_MIXERS; i++) {
    MixData* mix = mixAddress(i);
    if (!mix->srcRaw || mix->destCh >= destCh) return i;
  }
  return MAX_MIXERS;
}

static void aircastSetUp(const AircastMix& wanted)
{
  if (wanted.channel < 1 || wanted.channel > AIRCAST_CHANNEL_COUNT) return;
  uint8_t destCh = wanted.channel - 1;
  int source = aircastSource(wanted.control);
  uint8_t first = aircastFirstMix(destCh);
  if (source < 0 || getMixCount() >= MAX_MIXERS || first >= MAX_MIXERS) return;
  MixData* existing = mixAddress(first);
  if (existing->srcRaw && existing->destCh == destCh) return;
  insertMix(first, destCh);
  MixData* mix = mixAddress(first);
  mix->srcRaw = source;
  strncpy(mix->name, wanted.name, sizeof(mix->name));
}

static void aircastSetUpChannels()
{
  for (uint8_t at = 0; at < aircastMixCount; at++) aircastSetUp(aircastMixes[at]);
}

static bool aircastSameMixes(const AircastMix* mixes, uint8_t count)
{
  if (!aircastLayoutKnown || count != aircastMixCount) return false;
  for (uint8_t at = 0; at < count; at++) {
    if (mixes[at].control != aircastMixes[at].control ||
        mixes[at].channel != aircastMixes[at].channel)
      return false;
  }
  return true;
}

static void aircastHeard(const char* line)
{
  if (line[0] != '@') return;
  AircastMix mixes[AIRCAST_MIXES];
  uint8_t count = aircastMixesFor(line, mixes);
  if (aircastSameMixes(mixes, count)) return;
  for (uint8_t at = 0; at < count; at++) aircastMixes[at] = mixes[at];
  aircastMixCount = count;
  aircastLayoutKnown = true;
  aircastSetUpChannels();
}

static void aircastReceive(uint8_t* data, uint32_t len)
{
  for (uint32_t i = 0; i < len; i++) {
    uint8_t next = (aircastRxHead + 1) % AIRCAST_RX_BYTES;
    if (next == aircastRxTail) return;
    aircastRx[aircastRxHead] = data[i];
    aircastRxHead = next;
  }
}

static void aircastDrain()
{
  while (aircastRxTail != aircastRxHead) {
    char c = (char)aircastRx[aircastRxTail];
    aircastRxTail = (aircastRxTail + 1) % AIRCAST_RX_BYTES;
    if (c == '\n') {
      aircastHeardLine[aircastHeardLen] = '\0';
      aircastHeard(aircastHeardLine);
      aircastHeardLen = 0;
    } else if (aircastHeardLen < AIRCAST_HEARD_BYTES - 1) {
      aircastHeardLine[aircastHeardLen++] = c;
    } else {
      aircastHeardLen = 0;
    }
  }
}

static void aircastPoll()
{
  if (aircastSerialDrv->setReceiveCb || !aircastSerialDrv->getByte) return;
  uint8_t byte;
  while (aircastSerialDrv->getByte(aircastSerialCtx, &byte) > 0)
    aircastReceive(&byte, 1);
}

static void aircastTimerCb(timer_handle_t*)
{
  aircastPoll();
  aircastDrain();
  aircastSendLine();
}

void aircastModelLoaded()
{
  if (aircastSerialDrv && aircastLayoutKnown) aircastSetUpChannels();
}

static void aircastStart()
{
  if (aircastSerialDrv->setReceiveCb) {
    aircastSerialDrv->setReceiveCb(aircastSerialCtx, aircastReceive);
  }
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
  if (!drv && aircastSerialDrv && aircastSerialDrv->setReceiveCb) {
    aircastSerialDrv->setReceiveCb(aircastSerialCtx, nullptr);
  }
  aircastSerialCtx = ctx;
  aircastSerialDrv = drv;
  aircastLayoutKnown = false;

  if (drv) {
    aircastStart();
  } else {
    aircastStop();
  }
}
