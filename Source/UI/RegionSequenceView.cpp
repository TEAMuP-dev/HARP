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
#include "RegionSequenceView.h"

RegionSequenceView::RegionSequenceView (ARAEditorView& editorView, TimeToViewScaling& scaling, ARARegionSequence& rs, WaveformCache& cache)
    : araEditorView (editorView), timeToViewScaling (scaling), regionSequence (rs), waveformCache (cache)
{
        regionSequence.addListener (this);

        for (auto* playbackRegion : regionSequence.getPlaybackRegions())
            createAndAddPlaybackRegionView (playbackRegion);

        updatePlaybackDuration();

        timeToViewScaling.addListener (this);
}

RegionSequenceView::~RegionSequenceView()
{
        timeToViewScaling.removeListener (this);

        regionSequence.removeListener (this);

        for (const auto& it : playbackRegionViews)
            it.first->removeListener (this);
}

void RegionSequenceView::willUpdateRegionSequenceProperties (ARARegionSequence*, ARARegionSequence::PropertiesPtr newProperties)
{
        if (regionSequence.getColor() != newProperties->color)
        {
            for (auto& pbr : playbackRegionViews)
                pbr.second->repaint();
        }
}

void RegionSequenceView::willRemovePlaybackRegionFromRegionSequence (ARARegionSequence*, ARAPlaybackRegion* playbackRegion)
{
        playbackRegion->removeListener (this);
        removeChildComponent (playbackRegionViews[playbackRegion].get());
        playbackRegionViews.erase (playbackRegion);
        updatePlaybackDuration();
}

void RegionSequenceView::didAddPlaybackRegionToRegionSequence (ARARegionSequence*, ARAPlaybackRegion* playbackRegion)
{
        createAndAddPlaybackRegionView (playbackRegion);
        updatePlaybackDuration();
}

void RegionSequenceView::willDestroyPlaybackRegion (ARAPlaybackRegion* playbackRegion)
{
        playbackRegion->removeListener (this);
        removeChildComponent (playbackRegionViews[playbackRegion].get());
        playbackRegionViews.erase (playbackRegion);
        updatePlaybackDuration();
}

void RegionSequenceView::didUpdatePlaybackRegionProperties (ARAPlaybackRegion*)
{
        updatePlaybackDuration();
}

void RegionSequenceView::zoomLevelChanged (double)
{
        resized();
}

void RegionSequenceView::resized()
{
        for (auto& pbr : playbackRegionViews)
        {
            const auto playbackRegion = pbr.first;
            pbr.second->setBounds (
                getLocalBounds()
                    .withTrimmedLeft (timeToViewScaling.getXForTime (playbackRegion->getStartInPlaybackTime()))
                    .withWidth (timeToViewScaling.getXForTime (playbackRegion->getDurationInPlaybackTime())));
        }
}

void RegionSequenceView::createAndAddPlaybackRegionView (ARAPlaybackRegion* playbackRegion)
{
        playbackRegionViews[playbackRegion] = std::make_unique<PlaybackRegionView> (araEditorView,
                                                                                    *playbackRegion,
                                                                                    waveformCache);
        playbackRegion->addListener (this);
        addAndMakeVisible (*playbackRegionViews[playbackRegion]);
}

void RegionSequenceView::updatePlaybackDuration()
{
        const auto iter = std::max_element (
            playbackRegionViews.begin(),
            playbackRegionViews.end(),
            [] (const auto& a, const auto& b) { return a.first->getEndInPlaybackTime() < b.first->getEndInPlaybackTime(); });

        playbackDuration = iter != playbackRegionViews.end() ? iter->first->getEndInPlaybackTime()
                                                             : 0.0;

        sendChangeMessage();
}
