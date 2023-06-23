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
#include "RulersView.h"

void RulersView::CycleMarkerComponent::paint (Graphics& g)
{
            g.setColour (Colours::darkred);
            const auto bounds = getLocalBounds().toFloat();
            g.drawRoundedRectangle (bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), 6.0f, 2.0f);
}

RulersView::RulersView (PlayHeadState& playHeadStateIn, TimeToViewScaling& timeToViewScalingIn, ARADocument& document)
        : playHeadState (playHeadStateIn), timeToViewScaling (timeToViewScalingIn), araDocument (document)
{
        timeToViewScaling.addListener (this);

        addChildComponent (cycleMarker);
        cycleMarker.setInterceptsMouseClicks (false, false);

        setTooltip ("Double-click to start playback, click to stop playback or to reposition, drag horizontal range to set cycle.");

        startTimerHz (30);
}

RulersView::~RulersView()
{
        stopTimer();

        timeToViewScaling.removeListener (this);
        selectMusicalContext (nullptr);
}

void RulersView::paint (Graphics& g)
{
        auto drawBounds = g.getClipBounds();
        const auto drawStartTime = timeToViewScaling.getTimeForX (drawBounds.getX());
        const auto drawEndTime   = timeToViewScaling.getTimeForX (drawBounds.getRight());

        const auto bounds = getLocalBounds();

        g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
        g.fillRect (bounds);
        g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId).contrasting());
        g.drawRect (bounds);

        const auto rulerHeight = bounds.getHeight() / 3;
        g.drawRect (drawBounds.getX(), rulerHeight, drawBounds.getRight(), rulerHeight);
        g.setFont (Font (12.0f));

        const int lightLineWidth = 1;
        const int heavyLineWidth = 3;

        if (selectedMusicalContext != nullptr)
        {
            const ARA::PlugIn::HostContentReader<ARA::kARAContentTypeTempoEntries> tempoReader (selectedMusicalContext);
            const ARA::TempoConverter<decltype (tempoReader)> tempoConverter (tempoReader);

            // chord ruler: one rect per chord, skipping empty "no chords"
            const auto chordBounds = drawBounds.removeFromTop (rulerHeight);
            const ARA::PlugIn::HostContentReader<ARA::kARAContentTypeSheetChords> chordsReader (selectedMusicalContext);

            if (tempoReader && chordsReader)
            {
                const ARA::ChordInterpreter interpreter (true);
                for (auto itChord = chordsReader.begin(); itChord != chordsReader.end(); ++itChord)
                {
                    if (interpreter.isNoChord (*itChord))
                        continue;

                    const auto chordStartTime = (itChord == chordsReader.begin()) ? 0 : tempoConverter.getTimeForQuarter (itChord->position);

                    if (chordStartTime >= drawEndTime)
                        break;

                    auto chordRect = chordBounds;
                    chordRect.setLeft (timeToViewScaling.getXForTime (chordStartTime));

                    if (std::next (itChord) != chordsReader.end())
                    {
                        const auto nextChordStartTime = tempoConverter.getTimeForQuarter (std::next (itChord)->position);

                        if (nextChordStartTime < drawStartTime)
                            continue;

                        chordRect.setRight (timeToViewScaling.getXForTime (nextChordStartTime));
                    }

                    g.drawRect (chordRect);
                    g.drawText (convertARAString (interpreter.getNameForChord (*itChord).c_str()),
                                chordRect.withTrimmedLeft (2),
                                Justification::centredLeft);
                }
            }

            // beat ruler: evaluates tempo and bar signatures to draw a line for each beat
            const ARA::PlugIn::HostContentReader<ARA::kARAContentTypeBarSignatures> barSignaturesReader (selectedMusicalContext);

            if (barSignaturesReader)
            {
                const ARA::BarSignaturesConverter<decltype (barSignaturesReader)> barSignaturesConverter (barSignaturesReader);

                const double beatStart = barSignaturesConverter.getBeatForQuarter (tempoConverter.getQuarterForTime (drawStartTime));
                const double beatEnd   = barSignaturesConverter.getBeatForQuarter (tempoConverter.getQuarterForTime (drawEndTime));
                const int endBeat = roundToInt (std::floor (beatEnd));
                RectangleList<int> rects;

                for (int beat = roundToInt (std::ceil (beatStart)); beat <= endBeat; ++beat)
                {
                    const auto quarterPos = barSignaturesConverter.getQuarterForBeat (beat);
                    const int x = timeToViewScaling.getXForTime (tempoConverter.getTimeForQuarter (quarterPos));
                    const auto barSignature = barSignaturesConverter.getBarSignatureForQuarter (quarterPos);
                    const int lineWidth = (quarterPos == barSignature.position) ? heavyLineWidth : lightLineWidth;
                    const int beatsSinceBarStart = roundToInt( barSignaturesConverter.getBeatDistanceFromBarStartForQuarter (quarterPos));
                    const int lineHeight = (beatsSinceBarStart == 0) ? rulerHeight : rulerHeight / 2;
                    rects.addWithoutMerging (Rectangle<int> (x - lineWidth / 2, 2 * rulerHeight - lineHeight, lineWidth, lineHeight));
                }

                g.fillRectList (rects);
            }
        }

        // time ruler: one tick for each second
        {
            RectangleList<int> rects;

            for (auto time = std::floor (drawStartTime); time <= drawEndTime; time += 1.0)
            {
                const int lineWidth  = (std::fmod (time, 60.0) <= 0.001) ? heavyLineWidth : lightLineWidth;
                const int lineHeight = (std::fmod (time, 10.0) <= 0.001) ? rulerHeight : rulerHeight / 2;
                rects.addWithoutMerging (Rectangle<int> (timeToViewScaling.getXForTime (time) - lineWidth / 2,
                                                         bounds.getHeight() - lineHeight,
                                                         lineWidth,
                                                         lineHeight));
            }

            g.fillRectList (rects);
        }
}

void RulersView::mouseDrag (const MouseEvent& m)
{
        isDraggingCycle = true;

        auto cycleRect = getBounds();
        cycleRect.setLeft  (jmin (m.getMouseDownX(), m.x));
        cycleRect.setRight (jmax (m.getMouseDownX(), m.x));
        cycleMarker.setBounds (cycleRect);
}

void RulersView::mouseUp (const MouseEvent& m)
{
        auto playbackController = araDocument.getDocumentController()->getHostPlaybackController();

        if (playbackController != nullptr)
        {
            const auto startTime = timeToViewScaling.getTimeForX (jmin (m.getMouseDownX(), m.x));
            const auto endTime   = timeToViewScaling.getTimeForX (jmax (m.getMouseDownX(), m.x));

            if (playHeadState.isPlaying.load (std::memory_order_relaxed))
                playbackController->requestStopPlayback();
            else
                playbackController->requestSetPlaybackPosition (startTime);

            if (isDraggingCycle)
                playbackController->requestSetCycleRange (startTime, endTime - startTime);
        }

        isDraggingCycle = false;
}

void RulersView::mouseDoubleClick (const MouseEvent&)
{
        if (auto* playbackController = araDocument.getDocumentController()->getHostPlaybackController())
        {
            if (! playHeadState.isPlaying.load (std::memory_order_relaxed))
                playbackController->requestStartPlayback();
        }
}

void RulersView::selectMusicalContext (ARAMusicalContext* newSelectedMusicalContext)
{
        if (auto* oldSelection = std::exchange (selectedMusicalContext, newSelectedMusicalContext);
            oldSelection != selectedMusicalContext)
        {
            if (oldSelection != nullptr)
                oldSelection->removeListener (this);

            if (selectedMusicalContext != nullptr)
                selectedMusicalContext->addListener (this);

            repaint();
        }
}

void RulersView::zoomLevelChanged (double)
{
        repaint();
}

void RulersView::doUpdateMusicalContextContent (ARAMusicalContext*, ARAContentUpdateScopes)
{
        repaint();
}

void RulersView::updateCyclePosition()
{
        if (selectedMusicalContext != nullptr)
        {
            const ARA::PlugIn::HostContentReader<ARA::kARAContentTypeTempoEntries> tempoReader (selectedMusicalContext);
            const ARA::TempoConverter<decltype (tempoReader)> tempoConverter (tempoReader);

            const auto loopStartTime = tempoConverter.getTimeForQuarter (playHeadState.loopPpqStart.load (std::memory_order_relaxed));
            const auto loopEndTime   = tempoConverter.getTimeForQuarter (playHeadState.loopPpqEnd.load   (std::memory_order_relaxed));

            auto cycleRect = getBounds();
            cycleRect.setLeft  (timeToViewScaling.getXForTime (loopStartTime));
            cycleRect.setRight (timeToViewScaling.getXForTime (loopEndTime));
            cycleMarker.setVisible (true);
            cycleMarker.setBounds (cycleRect);
        }
        else
        {
            cycleMarker.setVisible (false);
        }
}

void RulersView::timerCallback()
{
        if (! isDraggingCycle)
            updateCyclePosition();
}
