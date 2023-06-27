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
 * @brief Wrapper for buffering audio source.
 * @author JUCE, hugo flores garcia, aldo aguilar
 */

#pragma once

class PossiblyBufferedReader {
public:
  PossiblyBufferedReader() = default;

  explicit PossiblyBufferedReader(unique_ptr<BufferingAudioReader> readerIn)
      : setTimeoutFn(
            [ptr = readerIn.get()](int ms) { ptr->setReadTimeout(ms); }),
        reader(std::move(readerIn)) {}

  explicit PossiblyBufferedReader(unique_ptr<AudioFormatReader> readerIn)
      : setTimeoutFn(), reader(std::move(readerIn)) {}

  void setReadTimeout(int ms) {
    NullCheckedInvocation::invoke(setTimeoutFn, ms);
  }

  AudioFormatReader *get() const { return reader.get(); }

private:
  std::function<void(int)> setTimeoutFn;
  unique_ptr<AudioFormatReader> reader;
};
