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
  * @brief Header of the UI above the document view, shows ticks for time
  * and can dispaly other information depending on the DAW.
  * region sequences. 
  * @author JUCE, hugo flores garcia, aldo aguilar
  */
#pragma once

#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_formats/juce_audio_formats.h"

using namespace juce;

/**
 * @class RulersHeader
 * @brief Represents a header that contains labels for chords, bars, and time.
 */
class RulersHeader : public Component
{
public:
    /**
     * @brief Construct a new Rulers Header object
     */
    RulersHeader();

    /**
     * @brief Resize the RulersHeader and its labels
     */
    void resized() override;

    /**
     * @brief Paint the RulersHeader with its labels
     */
    void paint (Graphics& g) override;

private:
    Label chordsLabel, barsLabel, timeLabel; /**< Labels for chords, bars, and time */
};


// #pragma once

// class RulersHeader : public Component
// {
// public:
//     RulersHeader()
//     {
//         chordsLabel.setText ("Chords", NotificationType::dontSendNotification);
//         addAndMakeVisible (chordsLabel);

//         barsLabel.setText ("Bars", NotificationType::dontSendNotification);
//         addAndMakeVisible (barsLabel);

//         timeLabel.setText ("Time", NotificationType::dontSendNotification);
//         addAndMakeVisible (timeLabel);
//     }

//     void resized() override
//     {
//         auto bounds = getLocalBounds();
//         const auto rulerHeight = bounds.getHeight() / 3;

//         for (auto* label : { &chordsLabel, &barsLabel, &timeLabel })
//             label->setBounds (bounds.removeFromTop (rulerHeight));
//     }

//     void paint (Graphics& g) override
//     {
//         auto bounds = getLocalBounds();
//         const auto rulerHeight = bounds.getHeight() / 3;
//         g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
//         g.fillRect (bounds);
//         g.setColour (getLookAndFeel().findColour (ResizableWindow::backgroundColourId).contrasting());
//         g.drawRect (bounds);
//         bounds.removeFromTop (rulerHeight);
//         g.drawRect (bounds.removeFromTop (rulerHeight));
//     }

// private:
//     Label chordsLabel, barsLabel, timeLabel;
// };
