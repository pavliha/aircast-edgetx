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

#include "aircast_layout.h"

#include <stdlib.h>
#include <string.h>

#define AIRCAST_ROLE_COMMAS 7

static const AircastMix aircastFunctions[] = {
    {"RS", "camREB", 18}, {"LS", "camyaw", 20}, {"S2", "zoom", 21},
    {"SG", "camtyp", 22}, {"SH", "termty", 24}, {"SF", "skld", 11},
    {"SE", "BOMARM", 12}, {"6POS", "", 5},
};
static const AircastMix aircastModeSwitch = {"SB", "phold", 0};

#define AIRCAST_FUNCTIONS (sizeof(aircastFunctions) / sizeof(aircastFunctions[0]))
#define AIRCAST_BAY (AIRCAST_FUNCTIONS - 1)

static uint8_t aircastModeChannelOf(const char* echo)
{
  const char* end = strchr(echo, ';');
  const char* last = nullptr;
  uint8_t commas = 0;
  for (const char* c = echo; *c && c != end; c++) {
    if (*c != ',') continue;
    commas++;
    last = c;
  }
  return commas == AIRCAST_ROLE_COMMAS ? (uint8_t)strtoul(last + 1, nullptr, 10)
                                       : 0;
}

uint8_t aircastMixesFor(const char* echo, AircastMix mixes[AIRCAST_MIXES])
{
  const char* cursor = strchr(echo, ';');
  bool named = cursor != nullptr;
  uint8_t modeChannel = aircastModeChannelOf(echo);
  uint8_t count = 0;
  for (uint8_t at = 0; at < AIRCAST_FUNCTIONS; at++) {
    uint8_t channel = named ? 0 : aircastFunctions[at].channel;
    if (cursor) {
      char* end = nullptr;
      channel = (uint8_t)strtoul(cursor + 1, &end, 10);
      cursor = (*end == ',') ? end : nullptr;
    }
    if (at == AIRCAST_BAY && !modeChannel) continue;
    mixes[count] = aircastFunctions[at];
    mixes[count++].channel = channel;
  }
  if (modeChannel) {
    mixes[count] = aircastModeSwitch;
    mixes[count++].channel = modeChannel;
  }
  return count;
}
