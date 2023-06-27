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
 * @brief UI for drawing the ruler component.
 * @author JUCE, hugo flores garcia, aldo aguilar
 */

#pragma once

#include "../Timeline/PlayheadPositionLabel.h"
#include "../Util/TimeToViewScaling.h"

class RulersView : public Component,
                   public SettableTooltipClient,
                   private Timer,
                   private TimeToViewScaling::Listener,
                   private ARAMusicalContext::Listener {
public:
  class CycleMarkerComponent : public Component {
    void paint(Graphics &g) override;
  };

  RulersView(PlayHeadState &playHeadStateIn,
             TimeToViewScaling &timeToViewScalingIn, ARADocument &document);
  ~RulersView() override;

  void paint(Graphics &g) override;
  void mouseDrag(const MouseEvent &m) override;
  void mouseUp(const MouseEvent &m) override;
  void mouseDoubleClick(const MouseEvent &) override;
  void selectMusicalContext(ARAMusicalContext *newSelectedMusicalContext);
  void zoomLevelChanged(double) override;
  void doUpdateMusicalContextContent(ARAMusicalContext *,
                                     ARAContentUpdateScopes) override;

private:
  void updateCyclePosition();
  void timerCallback() override;

private:
  PlayHeadState &playHeadState;
  TimeToViewScaling &timeToViewScaling;
  ARADocument &araDocument;
  ARAMusicalContext *selectedMusicalContext = nullptr;
  CycleMarkerComponent cycleMarker;
  bool isDraggingCycle = false;
};
