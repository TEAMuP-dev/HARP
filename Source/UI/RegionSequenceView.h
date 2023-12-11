/**
 * @file
 * @brief This file is part of the JUCE examples.
 *
 * Copyright (c) 2022 - Raw Material Software Limited
 * The code included in this file is provided under the terms of the ISC license
 * http://www.isc.org/downloads/software-support-policy/isc-license. Permission
 * To use, copy, modify, and/or distribute this software for any purpose with or
 * without fee is hereby granted provided that the above copyright notice and
 * this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
 * WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
 * PURPOSE, ARE DISCLAIMED.
 *
 * @brief UI for a single region sequence/track in the plugin. There can be
 * multiple instances of this class when the plugin is applied to multiple
 * region sequences.
 * @author JUCE, hugo flores garcia, aldo aguilar
 */

#pragma once

#include "../Timeline/PlaybackRegionView.h"
#include "../Util/TimeToViewScaling.h"
#include "../ARA/EditorView.h"
// #include "../ARA/PlaybackRegion.h"

class RegionSequenceView : public Component,
                           public ChangeBroadcaster,
                           private TimeToViewScaling::Listener,
                           private ARARegionSequence::Listener,
                           private ARAPlaybackRegion::Listener {
public:
  RegionSequenceView(EditorView &editorView, TimeToViewScaling &scaling,
                     ARARegionSequence &rs, WaveformCache &cache);
  ~RegionSequenceView() override;

  void willUpdateRegionSequenceProperties(
      ARARegionSequence *,
      ARARegionSequence::PropertiesPtr newProperties) override;
  void willRemovePlaybackRegionFromRegionSequence(
      ARARegionSequence *, ARAPlaybackRegion *playbackRegion) override;
  void didAddPlaybackRegionToRegionSequence(
      ARARegionSequence *, ARAPlaybackRegion *playbackRegion) override;
  void willDestroyPlaybackRegion(ARAPlaybackRegion *playbackRegion) override;
  void didUpdatePlaybackRegionProperties(ARAPlaybackRegion *) override;
  void zoomLevelChanged(double) override;
  void resized() override;

  auto getPlaybackDuration() const noexcept { return playbackDuration; }

private:
  void createAndAddPlaybackRegionView(ARAPlaybackRegion *playbackRegion);
  void updatePlaybackDuration();

  EditorView &araEditorView;
  TimeToViewScaling &timeToViewScaling;
  ARARegionSequence &regionSequence;
  WaveformCache &waveformCache;
  std::unordered_map<ARAPlaybackRegion *, unique_ptr<PlaybackRegionView>>
      playbackRegionViews;
  double playbackDuration = 0.0;
};
