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
 * @brief This class handles interactions with a playback region on the plugins
 * UI.
 * @author hugo flores garcia, aldo aguilar
 */
#pragma once

#include "../ARA/AudioModification.h"
#include "../ARA/EditorView.h"
#include "../ARA/DocumentControllerSpecialisation.h"
#include "../WaveformCache/WaveformCache.h"
#include "../UI/LookAndFeel.h"

#include <JuceHeader.h>

/**
 * @class PlaybackRegionView
 * @brief A class responsible for audio playback view and interactions.
 */
class PlaybackRegionView : public juce::Component,
                           public juce::ChangeListener,
                           public juce::SettableTooltipClient,
                           private ARAAudioSource::Listener,
                           private ARAPlaybackRegion::Listener,
                           private ARAEditorView::Listener {
public:
  PlaybackRegionView(EditorView &editorView, ARAPlaybackRegion &region,
                     WaveformCache &cache);
  ~PlaybackRegionView() override;

  // Event handlers
  void mouseDown(const juce::MouseEvent &m) override;
  void mouseUp(const juce::MouseEvent &) override;
  void mouseDoubleClick(const juce::MouseEvent &) override;
  void changeListenerCallback(juce::ChangeBroadcaster *) override;

  // ARA event handlers
  void didEnableAudioSourceSamplesAccess(ARAAudioSource *, bool) override;
  void willUpdatePlaybackRegionProperties(
      ARAPlaybackRegion *,
      ARAPlaybackRegion::PropertiesPtr newProperties) override;
  void didUpdatePlaybackRegionContent(ARAPlaybackRegion *,
                                      ARAContentUpdateScopes) override;
  void onNewSelection(const ARAViewSelection &viewSelection) override;

  // Juce paint and resize
  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  class PreviewRegionOverlay : public Component {
    static constexpr auto previewLength = 0.5;

  public:
    PreviewRegionOverlay(PlaybackRegionView &ownerIn) : owner(ownerIn) {}

    void update() {
      const auto &previewState = owner.getDocumentController()->previewState;

      if (previewState.previewedRegion.load() == &owner.playbackRegion) {
        const auto previewStartTime =
            previewState.previewTime.load() -
            owner.playbackRegion.getStartInPlaybackTime();
        const auto pixelPerSecond =
            owner.getWidth() / owner.playbackRegion.getDurationInPlaybackTime();

        setBounds(
            roundToInt((previewStartTime - previewLength / 2) * pixelPerSecond),
            0, roundToInt(previewLength * pixelPerSecond), owner.getHeight());

        setVisible(true);
      } else {
        setVisible(false);
      }

      repaint();
    }

    void paint(Graphics &g) override {
      g.setColour(Colours::darkred.withAlpha(0.5f));
      g.fillRect(getLocalBounds());
    }

  private:
    PlaybackRegionView &owner;
  };

  TensorJuceDocumentControllerSpecialisation *getDocumentController() const;
  EditorView &araEditorView;
  ARAPlaybackRegion &playbackRegion;
  WaveformCache &waveformCache;
  PreviewRegionOverlay previewRegionOverlay;
  juce::AudioBuffer<float> *mDeepAudio;
  bool isSelected = false;
  HARPLookAndFeel mHARPLookAndFeel;
};
