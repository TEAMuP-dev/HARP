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
 * @brief UI class for handling layout for UI components.
 * @author JUCE, hugo flores garcia, aldo aguilar
 */

#include "Layout.h"

void VerticalLayoutViewportContent::resized() {
  auto bounds = getLocalBounds();

  for (auto *component : getChildren()) {
    component->setBounds(bounds.removeFromTop(trackHeight));
    component->resized();
  }
}

VerticalLayoutViewport::VerticalLayoutViewport() {
  setViewedComponent(&content, false);
}

void VerticalLayoutViewport::paint(Graphics &g) {
  g.fillAll(getLookAndFeel()
                .findColour(ResizableWindow::backgroundColourId)
                .brighter());
}

void VerticalLayoutViewport::visibleAreaChanged(
    const Rectangle<int> &newVisibleArea) {
  NullCheckedInvocation::invoke(onVisibleAreaChanged, newVisibleArea);
}

void OverlayComponent::PlayheadMarkerComponent::paint(Graphics &g) {
  g.fillAll(Colours::darkred.darker(0.2f));
}

OverlayComponent::OverlayComponent(PlayHeadState &playHeadStateIn,
                                   TimeToViewScaling &timeToViewScalingIn)
    : playHeadState(playHeadStateIn), timeToViewScaling(timeToViewScalingIn) {
  addChildComponent(playheadMarker);
  setInterceptsMouseClicks(false, false);
  startTimerHz(30);

  timeToViewScaling.addListener(this);
}

OverlayComponent::~OverlayComponent() {
  timeToViewScaling.removeListener(this);

  stopTimer();
}

void OverlayComponent::resized() { updatePlayHeadPosition(); }

void OverlayComponent::setHorizontalOffset(int offset) {
  horizontalOffset = offset;
}

void OverlayComponent::setSelectedTimeRange(
    std::optional<ARA::ARAContentTimeRange> timeRange) {
  selectedTimeRange = timeRange;
  repaint();
}

void OverlayComponent::zoomLevelChanged(double) {
  updatePlayHeadPosition();
  repaint();
}

void OverlayComponent::paint(Graphics &g) {
  if (selectedTimeRange) {
    auto bounds = getLocalBounds();
    bounds.setLeft(timeToViewScaling.getXForTime(selectedTimeRange->start));
    bounds.setRight(timeToViewScaling.getXForTime(selectedTimeRange->start +
                                                  selectedTimeRange->duration));
    g.setColour(getLookAndFeel()
                    .findColour(ResizableWindow::backgroundColourId)
                    .brighter()
                    .withAlpha(0.3f));
    g.fillRect(bounds);
    g.setColour(Colours::whitesmoke.withAlpha(0.5f));
    g.drawRect(bounds);
  }
}

void OverlayComponent::updatePlayHeadPosition() {
  if (playHeadState.isPlaying.load(std::memory_order_relaxed)) {
    const auto markerX = timeToViewScaling.getXForTime(
        playHeadState.timeInSeconds.load(std::memory_order_relaxed));
    const auto playheadLine =
        getLocalBounds()
            .withTrimmedLeft((int)(markerX - markerWidth / 2.0) -
                             horizontalOffset)
            .removeFromLeft((int)markerWidth);
    playheadMarker.setVisible(true);
    playheadMarker.setBounds(playheadLine);
  } else {
    playheadMarker.setVisible(false);
  }
}

void OverlayComponent::timerCallback() { updatePlayHeadPosition(); }
