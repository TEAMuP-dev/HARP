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
 * @brief class for async callbacks. based on the juce Async Updater
 * https://docs.juce.com/master/classAsyncUpdater.html#a5cb530c31e68d13bbdf078ed54911e0c
 * @author JUCE, hugo flores garcia, aldo aguilar
 */

#pragma once

#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_formats/juce_audio_formats.h"

using namespace juce;

class AsyncConfigurationCallback : private AsyncUpdater {
public:
  explicit AsyncConfigurationCallback(std::function<void()> callbackIn)
      : callback(std::move(callbackIn)) {}

  ~AsyncConfigurationCallback() override { cancelPendingUpdate(); }

  template <typename RequiresLock> auto withLock(RequiresLock &&fn) {
    const SpinLock::ScopedTryLockType scope(processingFlag);
    return fn(scope.isLocked());
  }

  void startConfigure() { triggerAsyncUpdate(); }

private:
  void handleAsyncUpdate() override {
    const SpinLock::ScopedLockType scope(processingFlag);
    callback();
  }

  std::function<void()> callback;
  SpinLock processingFlag;
};
