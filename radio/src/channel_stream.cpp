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

#include "channel_stream.h"

#include <stdio.h>

#include "channel_layout.h"

#include "edgetx.h"
#include "hal/adc_driver.h"
#include "hal/usb_driver.h"
#include "mixes.h"
#include "switches.h"
#include "os/timer.h"

#define CHANNEL_STREAM_CHANNEL_COUNT 24
#define CHANNEL_STREAM_SEND_PERIOD_MS 20
#define CHANNEL_STREAM_LINE_BYTES 192
#define CHANNEL_STREAM_RX_BYTES 128
#define CHANNEL_STREAM_HEARD_BYTES 96

static const etx_serial_driver_t* channelStreamSerialDrv = nullptr;
static void* channelStreamSerialCtx = nullptr;
static timer_handle_t channelStreamTimer = TIMER_INITIALIZER;

static uint8_t channelStreamRx[CHANNEL_STREAM_RX_BYTES];
static volatile uint8_t channelStreamRxHead = 0;
static volatile uint8_t channelStreamRxTail = 0;
static char channelStreamHeardLine[CHANNEL_STREAM_HEARD_BYTES];
static uint8_t channelStreamHeardLen = 0;
static ChannelMix channelStreamMixes[CHANNEL_MIXES];
static uint8_t channelStreamMixCount = 0;
static bool channelStreamLayoutKnown = false;

static void channelStreamSendLine()
{
  auto sendByte = channelStreamSerialDrv->sendByte;
  if (!sendByte) return;

  char line[CHANNEL_STREAM_LINE_BYTES];
  int len = snprintf(line, sizeof(line), "$");
  for (uint8_t i = 0; i < CHANNEL_STREAM_CHANNEL_COUNT; i++) {
    len += snprintf(line + len, sizeof(line) - len, "%d,", channelOutputs[i]);
  }
  len += snprintf(line + len, sizeof(line) - len, "\r\n");

  if (usbSerialFreeSpace() < (uint32_t)len) return;
  for (int i = 0; i < len; i++) sendByte(channelStreamSerialCtx, (uint8_t)line[i]);
}

static int channelStreamSource(const char* control)
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

static uint8_t channelStreamFirstMix(uint8_t destCh)
{
  for (uint8_t i = 0; i < MAX_MIXERS; i++) {
    MixData* mix = mixAddress(i);
    if (!mix->srcRaw || mix->destCh >= destCh) return i;
  }
  return MAX_MIXERS;
}

static void channelStreamSetUp(const ChannelMix& wanted)
{
  if (wanted.channel < 1 || wanted.channel > CHANNEL_STREAM_CHANNEL_COUNT) return;
  uint8_t destCh = wanted.channel - 1;
  int source = channelStreamSource(wanted.control);
  uint8_t first = channelStreamFirstMix(destCh);
  if (source < 0 || getMixCount() >= MAX_MIXERS || first >= MAX_MIXERS) return;
  MixData* existing = mixAddress(first);
  if (existing->srcRaw && existing->destCh == destCh) return;
  insertMix(first, destCh);
  MixData* mix = mixAddress(first);
  mix->srcRaw = source;
  strncpy(mix->name, wanted.name, sizeof(mix->name));
}

static void channelStreamSetUpChannels()
{
  for (uint8_t at = 0; at < channelStreamMixCount; at++) channelStreamSetUp(channelStreamMixes[at]);
}

static bool channelStreamSameMixes(const ChannelMix* mixes, uint8_t count)
{
  if (!channelStreamLayoutKnown || count != channelStreamMixCount) return false;
  for (uint8_t at = 0; at < count; at++) {
    if (mixes[at].control != channelStreamMixes[at].control ||
        mixes[at].channel != channelStreamMixes[at].channel)
      return false;
  }
  return true;
}

static void channelStreamHeard(const char* line)
{
  if (line[0] != '@') return;
  ChannelMix mixes[CHANNEL_MIXES];
  uint8_t count = channelMixesFor(line, mixes);
  if (channelStreamSameMixes(mixes, count)) return;
  for (uint8_t at = 0; at < count; at++) channelStreamMixes[at] = mixes[at];
  channelStreamMixCount = count;
  channelStreamLayoutKnown = true;
  channelStreamSetUpChannels();
}

static void channelStreamReceive(uint8_t* data, uint32_t len)
{
  for (uint32_t i = 0; i < len; i++) {
    uint8_t next = (channelStreamRxHead + 1) % CHANNEL_STREAM_RX_BYTES;
    if (next == channelStreamRxTail) return;
    channelStreamRx[channelStreamRxHead] = data[i];
    channelStreamRxHead = next;
  }
}

static void channelStreamDrain()
{
  while (channelStreamRxTail != channelStreamRxHead) {
    char c = (char)channelStreamRx[channelStreamRxTail];
    channelStreamRxTail = (channelStreamRxTail + 1) % CHANNEL_STREAM_RX_BYTES;
    if (c == '\n') {
      channelStreamHeardLine[channelStreamHeardLen] = '\0';
      channelStreamHeard(channelStreamHeardLine);
      channelStreamHeardLen = 0;
    } else if (channelStreamHeardLen < CHANNEL_STREAM_HEARD_BYTES - 1) {
      channelStreamHeardLine[channelStreamHeardLen++] = c;
    } else {
      channelStreamHeardLen = 0;
    }
  }
}

static void channelStreamPoll()
{
  if (channelStreamSerialDrv->setReceiveCb || !channelStreamSerialDrv->getByte) return;
  uint8_t byte;
  while (channelStreamSerialDrv->getByte(channelStreamSerialCtx, &byte) > 0)
    channelStreamReceive(&byte, 1);
}

static void channelStreamTimerCb(timer_handle_t*)
{
  channelStreamPoll();
  channelStreamDrain();
  channelStreamSendLine();
}

void channelStreamModelLoaded()
{
  if (channelStreamSerialDrv && channelStreamLayoutKnown) channelStreamSetUpChannels();
}

static void channelStreamStart()
{
  if (channelStreamSerialDrv->setReceiveCb) {
    channelStreamSerialDrv->setReceiveCb(channelStreamSerialCtx, channelStreamReceive);
  }
  if (!timer_is_created(&channelStreamTimer)) {
    timer_create(&channelStreamTimer, channelStreamTimerCb, "channels", CHANNEL_STREAM_SEND_PERIOD_MS, true);
  }
  timer_start(&channelStreamTimer);
}

static void channelStreamStop()
{
  timer_stop(&channelStreamTimer);
}

void channelStreamSetSerialDriver(void* ctx, const etx_serial_driver_t* drv)
{
  if (!drv && channelStreamSerialDrv && channelStreamSerialDrv->setReceiveCb) {
    channelStreamSerialDrv->setReceiveCb(channelStreamSerialCtx, nullptr);
  }
  channelStreamSerialCtx = ctx;
  channelStreamSerialDrv = drv;
  channelStreamLayoutKnown = false;

  if (drv) {
    channelStreamStart();
  } else {
    channelStreamStop();
  }
}
