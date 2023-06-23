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
  * @brief This class handles the UI updates for the plugins document display. 
  * The display mirrors the DAW document, and will show region sequences with the
  * ARA plugin is placed in the first position of the effects chain.
  * @author JUCE, hugo flores garcia, aldo aguilar
  */

#pragma once

#include <ARA_Library/Utilities/ARAPitchInterpretation.h>
#include <ARA_Library/PlugIn/ARAPlug.h>

#include "juce_audio_basics/juce_audio_basics.h"

#include "Layout.h"
#include "ZoomControls.h"
#include "../WaveformCache/WaveformCache.h"
#include "TrackHeader.h"
#include "RegionSequenceView.h"
#include "RulersHeader.h"
#include "RulersView.h"

/**
 * @class DocumentView
 * @brief This class handles the DocumentView related functions
 */
class DocumentView : public Component,
                     public ChangeListener,
                     public ARAMusicalContext::Listener,
                     private ARADocument::Listener,
                     private ARAEditorView::Listener
{
public:
    /**
     * @brief Constructor
     * @param editorView
     * @param playHeadState
     */
    DocumentView(ARAEditorView& editorView, PlayHeadState& playHeadState);

    /**
     * @brief Destructor
     */
    ~DocumentView() override;

    //==============================================================================
    // ARADocument::Listener overrides
    void didAddMusicalContextToDocument(ARADocument*, ARAMusicalContext* musicalContext) override;
    void willDestroyMusicalContext(ARAMusicalContext* musicalContext) override;
    void didReorderRegionSequencesInDocument(ARADocument*) override;
    void didAddRegionSequenceToDocument(ARADocument*, ARARegionSequence*) override;
    void willRemoveRegionSequenceFromDocument(ARADocument*, ARARegionSequence* regionSequence) override;
    void didEndEditing(ARADocument*) override;

    //==============================================================================
    /**
     * @brief Implements changeListenerCallback from ChangeListener class
     * @param source
     */
    void changeListenerCallback(ChangeBroadcaster* source) override;

    //==============================================================================
    // ARAEditorView::Listener overrides
    void onNewSelection(const ARAViewSelection& viewSelection) override;
    void onHideRegionSequences(const std::vector<ARARegionSequence*>& regionSequences) override;

    //==============================================================================
    /**
     * @brief Overrides the paint method from Component class
     * @param g
     */
    void paint(Graphics& g) override;

    /**
     * @brief Overrides the resized method from Component class
     */
    void resized() override;

    //==============================================================================
    static constexpr int headerWidth = 120;

private:
    struct RegionSequenceViewKey
    {
        explicit RegionSequenceViewKey (ARARegionSequence* regionSequence)
            : orderIndex (regionSequence->getOrderIndex()), sequence (regionSequence)
        {
        }

        bool operator< (const RegionSequenceViewKey& other) const
        {
            return std::tie (orderIndex, sequence) < std::tie (other.orderIndex, other.sequence);
        }

        ARA::ARAInt32 orderIndex;
        ARARegionSequence* sequence;
    };

    void selectMusicalContext(ARAMusicalContext* newSelectedMusicalContext);
    void zoom(double factor);
    template <typename T> void layOutVertically(Rectangle<int> bounds, T& components, int verticalOffset = 0);
    void update();
    void addTrackViews(ARARegionSequence* regionSequence);
    void removeRegionSequenceView(ARARegionSequence* regionSequence);
    void invalidateRegionSequenceViews();
    void rebuildRegionSequenceViews();

    ARAEditorView& araEditorView;
    ARADocument& araDocument;
    bool regionSequenceViewsAreValid = false;
    TimeToViewScaling timeToViewScaling;
    double timelineLength = 0.0;
    ARAMusicalContext* selectedMusicalContext = nullptr;
    std::vector<ARARegionSequence*> hiddenRegionSequences;
    WaveformCache waveformCache;
    std::map<RegionSequenceViewKey, unique_ptr<TrackHeader>> trackHeaders;
    std::map<RegionSequenceViewKey, unique_ptr<RegionSequenceView>> regionSequenceViews;
    RulersHeader rulersHeader;
    RulersView rulersView;
    VerticalLayoutViewport viewport;
    OverlayComponent overlay;
    ZoomControls zoomControls;
    PlayheadPositionLabel playheadPositionLabel;
    TooltipWindow tooltip;
    int viewportHeightOffset = 0;
};
