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

#include "gtests.h"
#include "channel_layout.h"

static uint8_t channelOf(const ChannelMix* mixes, uint8_t count, const char* control)
{
  for (uint8_t at = 0; at < count; at++) {
    if (!strcmp(mixes[at].control, control)) return mixes[at].channel;
  }
  return 0;
}

static const char* nameOf(const ChannelMix* mixes, uint8_t count, const char* control)
{
  for (uint8_t at = 0; at < count; at++) {
    if (!strcmp(mixes[at].control, control)) return mixes[at].name;
  }
  return "";
}

TEST(ChannelLayout, AnAppThatNamesNoLayoutGetsTheR2d2CameraAndPayloadChannels)
{
  ChannelMix mixes[CHANNEL_MIXES];
  uint8_t count = channelMixesFor("@F,1", mixes);
  EXPECT_EQ(9, count);
  EXPECT_EQ(18, channelOf(mixes, count, "RS"));
  EXPECT_STREQ("camREB", mixes[0].name);
  EXPECT_EQ(20, channelOf(mixes, count, "LS"));
  EXPECT_EQ(11, channelOf(mixes, count, "SF"));
  EXPECT_EQ(0, channelOf(mixes, count, "6POS"));
  EXPECT_EQ(0, channelOf(mixes, count, "SB"));
  EXPECT_EQ(19, channelOf(mixes, count, "S1"));
  EXPECT_EQ(23, channelOf(mixes, count, "SC"));
}

TEST(ChannelLayout, ADroneWithFlightModesOnCh10GetsTheWholeR2d2Model)
{
  ChannelMix mixes[CHANNEL_MIXES];
  uint8_t count = channelMixesFor("@F,1,C,1,2,3,4,10;18,20,21,22,24,11,12,5,19,23", mixes);
  EXPECT_EQ(11, count);
  EXPECT_EQ(5, channelOf(mixes, count, "6POS"));
  EXPECT_EQ(10, channelOf(mixes, count, "SB"));
  EXPECT_STREQ("phold", mixes[count - 1].name);
  EXPECT_EQ(19, channelOf(mixes, count, "S1"));
  EXPECT_STREQ("campit", nameOf(mixes, count, "S1"));
  EXPECT_EQ(23, channelOf(mixes, count, "SC"));
  EXPECT_STREQ("cam90", nameOf(mixes, count, "SC"));
}

TEST(ChannelLayout, AnOlderAppThatNamesEightChannelsLeavesTheTiltKnobAndTheLookSwitchOff)
{
  ChannelMix mixes[CHANNEL_MIXES];
  uint8_t count = channelMixesFor("@F,1,C,1,2,3,4,10;18,20,21,22,24,11,12,5", mixes);
  EXPECT_EQ(11, count);
  EXPECT_EQ(5, channelOf(mixes, count, "6POS"));
  EXPECT_EQ(10, channelOf(mixes, count, "SB"));
  EXPECT_EQ(0, channelOf(mixes, count, "S1"));
  EXPECT_EQ(0, channelOf(mixes, count, "SC"));
}

TEST(ChannelLayout, AnAppThatTurnsTiltOffLeavesTheTiltKnobOff)
{
  ChannelMix mixes[CHANNEL_MIXES];
  uint8_t count = channelMixesFor("@F,1,C,1,2,3,4,10;18,20,21,22,24,11,12,5,0,23", mixes);
  EXPECT_EQ(11, count);
  EXPECT_EQ(0, channelOf(mixes, count, "S1"));
  EXPECT_EQ(23, channelOf(mixes, count, "SC"));
}

TEST(ChannelLayout, ADroneWithFlightModesOnCh5GetsTheModeSwitchThereAndNoBaySwitch)
{
  ChannelMix mixes[CHANNEL_MIXES];
  uint8_t count = channelMixesFor("@F,1,C,1,2,3,4,5;18,20,21,22,24,11,12,0", mixes);
  EXPECT_EQ(11, count);
  EXPECT_EQ(0, channelOf(mixes, count, "6POS"));
  EXPECT_EQ(5, channelOf(mixes, count, "SB"));
}

TEST(ChannelLayout, AnOlderAppWithoutAFlightModeChannelSetsUpNoModeOrBaySwitch)
{
  ChannelMix mixes[CHANNEL_MIXES];
  uint8_t count = channelMixesFor("@F,1,C,1,2,3,4;13,14,15,0,0,0,0,0", mixes);
  EXPECT_EQ(9, count);
  EXPECT_EQ(13, channelOf(mixes, count, "RS"));
  EXPECT_EQ(0, channelOf(mixes, count, "SF"));
  EXPECT_EQ(0, channelOf(mixes, count, "SB"));
}

TEST(ChannelLayout, AFunctionTheAppsLayoutLeavesOutIsOff)
{
  ChannelMix mixes[CHANNEL_MIXES];
  uint8_t count = channelMixesFor("@N;13", mixes);
  EXPECT_EQ(9, count);
  EXPECT_EQ(13, channelOf(mixes, count, "RS"));
  EXPECT_EQ(0, channelOf(mixes, count, "LS"));
  EXPECT_EQ(0, channelOf(mixes, count, "S1"));
}
