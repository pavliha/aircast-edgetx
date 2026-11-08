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

#include "channel_layout.h"

#include <stdlib.h>
#include <string.h>

#define CHANNEL_STREAM_ROLE_COMMAS 7

static const ChannelMix channelStreamFunctions[] = {
    {"RS", "camREB", 18}, {"LS", "camyaw", 20}, {"S2", "zoom", 21},
    {"SG", "camtyp", 22}, {"SH", "termty", 24}, {"SF", "skld", 11},
    {"SE", "BOMARM", 12}, {"6POS", "", 5},   {"S1", "campit", 19},
    {"SC", "cam90", 23, -100},
};
static const ChannelMix channelStreamModeSwitch = {"SB", "phold", 0};

#define CHANNEL_STREAM_FUNCTIONS (sizeof(channelStreamFunctions) / sizeof(channelStreamFunctions[0]))
#define CHANNEL_STREAM_BAY 7

static uint8_t channelStreamModeChannelOf(const char* echo)
{
  const char* end = strchr(echo, ';');
  const char* last = nullptr;
  uint8_t commas = 0;
  for (const char* c = echo; *c && c != end; c++) {
    if (*c != ',') continue;
    commas++;
    last = c;
  }
  return commas == CHANNEL_STREAM_ROLE_COMMAS ? (uint8_t)strtoul(last + 1, nullptr, 10)
                                       : 0;
}

uint8_t channelMixesFor(const char* echo, ChannelMix mixes[CHANNEL_MIXES])
{
  const char* cursor = strchr(echo, ';');
  bool named = cursor != nullptr;
  uint8_t modeChannel = channelStreamModeChannelOf(echo);
  uint8_t count = 0;
  for (uint8_t at = 0; at < CHANNEL_STREAM_FUNCTIONS; at++) {
    uint8_t channel = named ? 0 : channelStreamFunctions[at].channel;
    if (cursor) {
      char* end = nullptr;
      channel = (uint8_t)strtoul(cursor + 1, &end, 10);
      cursor = (*end == ',') ? end : nullptr;
    }
    if (at == CHANNEL_STREAM_BAY && !modeChannel) continue;
    mixes[count] = channelStreamFunctions[at];
    mixes[count++].channel = channel;
  }
  if (modeChannel) {
    mixes[count] = channelStreamModeSwitch;
    mixes[count++].channel = modeChannel;
  }
  return count;
}
