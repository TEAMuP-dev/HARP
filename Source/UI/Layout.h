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

#pragma once

#include "../Timeline/PlayheadPositionLabel.h"
#include "../Util/TimeToViewScaling.h"
#include "juce_audio_basics/juce_audio_basics.h"

constexpr auto trackHeight = 60;

class VerticalLayoutViewportContent : public Component {
public:
  void resized() override;
};

class VerticalLayoutViewport : public Viewport {
public:
  VerticalLayoutViewport();
  void paint(Graphics &g) override;

  std::function<void(Rectangle<int>)> onVisibleAreaChanged;

  VerticalLayoutViewportContent content;

private:
  void visibleAreaChanged(const Rectangle<int> &newVisibleArea) override;
};

class OverlayComponent : public Component,
                         private Timer,
                         private TimeToViewScaling::Listener {
public:
  class PlayheadMarkerComponent : public Component {
    void paint(Graphics &g) override;
  };

  OverlayComponent(PlayHeadState &playHeadStateIn,
                   TimeToViewScaling &timeToViewScalingIn);
  ~OverlayComponent() override;
  void resized() override;
  void setHorizontalOffset(int offset);
  void setSelectedTimeRange(std::optional<ARA::ARAContentTimeRange> timeRange);
  void zoomLevelChanged(double) override;
  void paint(Graphics &g) override;

private:
  void updatePlayHeadPosition();
  void timerCallback() override;

  static constexpr double markerWidth = 2.0;

  PlayHeadState &playHeadState;
  TimeToViewScaling &timeToViewScaling;
  int horizontalOffset = 0;
  std::optional<ARA::ARAContentTimeRange> selectedTimeRange;
  PlayheadMarkerComponent playheadMarker;
};

// #pragma once

// #include "juce_audio_basics/juce_audio_basics.h"

// #include "../Util/TimeToViewScaling.h"
// #include "../Timeline/PlayheadPositionLabel.h"

// constexpr auto trackHeight = 60;

// class VerticalLayoutViewportContent : public Component
// {
// public:
//     void resized() override
//     {
//         auto bounds = getLocalBounds();

//         for (auto* component : getChildren())
//         {
//             component->setBounds (bounds.removeFromTop (trackHeight));
//             component->resized();
//         }
//     }
// };

// class VerticalLayoutViewport : public Viewport
// {
// public:
//     VerticalLayoutViewport()
//     {
//         setViewedComponent (&content, false);
//     }

//     void paint (Graphics& g) override
//     {
//         g.fillAll (getLookAndFeel().findColour
//         (ResizableWindow::backgroundColourId).brighter());
//     }

//     std::function<void (Rectangle<int>)> onVisibleAreaChanged;

//     VerticalLayoutViewportContent content;

// private:
//     void visibleAreaChanged (const Rectangle<int>& newVisibleArea) override
//     {
//         NullCheckedInvocation::invoke (onVisibleAreaChanged, newVisibleArea);
//     }
// };

// class OverlayComponent : public Component,
//                          private Timer,
//                          private TimeToViewScaling::Listener
// {
// public:
//     class PlayheadMarkerComponent : public Component
//     {
//         void paint (Graphics& g) override { g.fillAll
//         (Colours::darkred.darker (0.2f)); }
//     };

//     OverlayComponent (PlayHeadState& playHeadStateIn, TimeToViewScaling&
//     timeToViewScalingIn)
//         : playHeadState (playHeadStateIn), timeToViewScaling
//         (timeToViewScalingIn)
//     {
//         addChildComponent (playheadMarker);
//         setInterceptsMouseClicks (false, false);
//         startTimerHz (30);

//         timeToViewScaling.addListener (this);
//     }

//     ~OverlayComponent() override
//     {
//         timeToViewScaling.removeListener (this);

//         stopTimer();
//     }

//     void resized() override
//     {
//         updatePlayHeadPosition();
//     }

//     void setHorizontalOffset (int offset)
//     {
//         horizontalOffset = offset;
//     }

//     void setSelectedTimeRange (std::optional<ARA::ARAContentTimeRange>
//     timeRange)
//     {
//         selectedTimeRange = timeRange;
//         repaint();
//     }

//     void zoomLevelChanged (double) override
//     {
//         updatePlayHeadPosition();
//         repaint();
//     }

//     void paint (Graphics& g) override
//     {
//         if (selectedTimeRange)
//         {
//             auto bounds = getLocalBounds();
//             bounds.setLeft (timeToViewScaling.getXForTime
//             (selectedTimeRange->start)); bounds.setRight
//             (timeToViewScaling.getXForTime (selectedTimeRange->start +
//             selectedTimeRange->duration)); g.setColour
//             (getLookAndFeel().findColour
//             (ResizableWindow::backgroundColourId).brighter().withAlpha
//             (0.3f)); g.fillRect (bounds); g.setColour
//             (Colours::whitesmoke.withAlpha (0.5f)); g.drawRect (bounds);
//         }
//     }

// private:
//     void updatePlayHeadPosition()
//     {
//         if (playHeadState.isPlaying.load (std::memory_order_relaxed))
//         {
//             const auto markerX = timeToViewScaling.getXForTime
//             (playHeadState.timeInSeconds.load (std::memory_order_relaxed));
//             const auto playheadLine = getLocalBounds().withTrimmedLeft ((int)
//             (markerX - markerWidth / 2.0) - horizontalOffset)
//                                                       .removeFromLeft ((int)
//                                                       markerWidth);
//             playheadMarker.setVisible (true);
//             playheadMarker.setBounds (playheadLine);
//         }
//         else
//         {
//             playheadMarker.setVisible (false);
//         }
//     }

//     void timerCallback() override
//     {
//         updatePlayHeadPosition();
//     }

//     static constexpr double markerWidth = 2.0;

//     PlayHeadState& playHeadState;
//     TimeToViewScaling& timeToViewScaling;
//     int horizontalOffset = 0;
//     std::optional<ARA::ARAContentTimeRange> selectedTimeRange;
//     PlayheadMarkerComponent playheadMarker;
// };
