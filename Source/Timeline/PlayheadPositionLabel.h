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
 * @brief This class handles information relaged to the playhead of the DAW.
 * @author hugo flores garcia, aldo aguilar
 */
#pragma once

#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_formats/juce_audio_formats.h"

#include <ARA_Library/PlugIn/ARAPlug.h>
#include <ARA_Library/Utilities/ARAPitchInterpretation.h>
#include <ARA_Library/Utilities/ARATimelineConversion.h>

using namespace juce;

/**
 * @brief Struct representing the state of the playhead.
 *
 */
struct PlayHeadState {
  std::atomic<bool> isPlaying{false}, isLooping{false};
  std::atomic<double> timeInSeconds{0.0}, loopPpqStart{0.0}, loopPpqEnd{0.0};

  void update(const Optional<AudioPlayHead::PositionInfo> &info);
};

/**
 * @brief A label that displays the playhead position and updates in real time.
 *
 */
class PlayheadPositionLabel : public Label, private Timer {
public:
  PlayheadPositionLabel(PlayHeadState &playHeadStateIn);
  ~PlayheadPositionLabel() override;

  void selectMusicalContext(ARAMusicalContext *newSelectedMusicalContext);

private:
  void timerCallback() override;
  static String timeToTimecodeString(double seconds);

  PlayHeadState &playHeadState;
  ARAMusicalContext *selectedMusicalContext = nullptr;
};

// #pragma once

// #include "juce_audio_basics/juce_audio_basics.h"
// #include "juce_audio_formats/juce_audio_formats.h"

// #include <ARA_Library/Utilities/ARAPitchInterpretation.h>
// #include <ARA_Library/Utilities/ARATimelineConversion.h>
// #include <ARA_Library/PlugIn/ARAPlug.h>

// using namespace juce;

// struct PlayHeadState
// {
//     void update (const Optional<AudioPlayHead::PositionInfo>& info)
//     {
//         if (info.hasValue())
//         {
//             isPlaying.store (info->getIsPlaying(),
//             std::memory_order_relaxed); timeInSeconds.store
//             (info->getTimeInSeconds().orFallback (0),
//             std::memory_order_relaxed); isLooping.store
//             (info->getIsLooping(), std::memory_order_relaxed); const auto
//             loopPoints = info->getLoopPoints();

//             if (loopPoints.hasValue())
//             {
//                 loopPpqStart = loopPoints->ppqStart;
//                 loopPpqEnd = loopPoints->ppqEnd;
//             }
//         }
//         else
//         {
//             isPlaying.store (false, std::memory_order_relaxed);
//             isLooping.store (false, std::memory_order_relaxed);
//         }
//     }

//     std::atomic<bool> isPlaying { false },
//                       isLooping { false };
//     std::atomic<double> timeInSeconds { 0.0 },
//                         loopPpqStart  { 0.0 },
//                         loopPpqEnd    { 0.0 };
// };

// class PlayheadPositionLabel : public Label,
//                               private Timer
// {
// public:
//     PlayheadPositionLabel (PlayHeadState& playHeadStateIn)
//         : playHeadState (playHeadStateIn)
//     {
//         startTimerHz (30);
//     }

//     ~PlayheadPositionLabel() override
//     {
//         stopTimer();
//     }

//     void selectMusicalContext (ARAMusicalContext* newSelectedMusicalContext)
//     {
//         selectedMusicalContext = newSelectedMusicalContext;
//     }

// private:
//     void timerCallback() override
//     {
//         const auto timePosition = playHeadState.timeInSeconds.load
//         (std::memory_order_relaxed);

//         auto text = timeToTimecodeString (timePosition);

//         if (playHeadState.isPlaying.load (std::memory_order_relaxed))
//             text += " (playing)";
//         else
//             text += " (stopped)";

//         if (selectedMusicalContext != nullptr)
//         {
//             const
//             ARA::PlugIn::HostContentReader<ARA::kARAContentTypeTempoEntries>
//             tempoReader (selectedMusicalContext); const
//             ARA::PlugIn::HostContentReader<ARA::kARAContentTypeBarSignatures>
//             barSignaturesReader (selectedMusicalContext);

//             if (tempoReader && barSignaturesReader)
//             {
//                 const ARA::TempoConverter<decltype (tempoReader)>
//                 tempoConverter (tempoReader); const
//                 ARA::BarSignaturesConverter<decltype (barSignaturesReader)>
//                 barSignaturesConverter (barSignaturesReader); const auto
//                 quarterPosition = tempoConverter.getQuarterForTime
//                 (timePosition); const auto barIndex =
//                 barSignaturesConverter.getBarIndexForQuarter
//                 (quarterPosition); const auto beatDistance =
//                 barSignaturesConverter.getBeatDistanceFromBarStartForQuarter
//                 (quarterPosition); const auto quartersPerBeat = 4.0 /
//                 (double) barSignaturesConverter.getBarSignatureForQuarter
//                 (quarterPosition).denominator; const auto beatIndex = (int)
//                 beatDistance; const auto tickIndex = juce::roundToInt
//                 ((beatDistance - beatIndex) * quartersPerBeat * 960.0);

//                 text += newLine;
//                 text += String::formatted ("bar %d | beat %d | tick %03d",
//                 (barIndex >= 0) ? barIndex + 1 : barIndex, beatIndex + 1,
//                 tickIndex + 1); text += "  -  ";

//                 const
//                 ARA::PlugIn::HostContentReader<ARA::kARAContentTypeSheetChords>
//                 chordsReader (selectedMusicalContext);

//                 if (chordsReader && chordsReader.getEventCount() > 0)
//                 {
//                     const auto begin = chordsReader.begin();
//                     const auto end = chordsReader.end();
//                     auto it = begin;

//                     while (it != end && it->position <= quarterPosition)
//                         ++it;

//                     if (it != begin)
//                         --it;

//                     const ARA::ChordInterpreter interpreter (true);
//                     text += "chord ";
//                     text += String (interpreter.getNameForChord (*it));
//                 }
//                 else
//                 {
//                     text += "(no chords provided)";
//                 }
//             }
//         }

//         setText (text, NotificationType::dontSendNotification);
//     }

//     // Copied from AudioPluginDemo.h: quick-and-dirty function to format a
//     timecode string static String timeToTimecodeString (double seconds)
//     {
//         auto millisecs = roundToInt (seconds * 1000.0);
//         auto absMillisecs = std::abs (millisecs);

//         return String::formatted ("%02d:%02d:%02d.%03d",
//                                   millisecs / 3600000,
//                                   (absMillisecs / 60000) % 60,
//                                   (absMillisecs / 1000)  % 60,
//                                   absMillisecs % 1000);
//     }

//     PlayHeadState& playHeadState;
//     ARAMusicalContext* selectedMusicalContext = nullptr;
// };
