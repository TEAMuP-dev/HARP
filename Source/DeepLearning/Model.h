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
 * @brief This is the interface for any kind of deep learning model. this will
 * be the base class for wave 2 wave, wave 2 label, text 2 wave, midi 2 wave,
 * wave 2 midi, etc.
 * @author hugo flores garcia, aldo aguilar
 */

#pragma once

#include <any>
#include <map>
#include <string>
#include <unordered_map>

using std::any;
using std::map;
using std::string;

namespace modelparams {
  inline bool contains(const map<string, any> &params, const string &key) {
    return params.find(key) != params.end();
  }
}

/**
 * @class Model
 * @brief Abstract class for different types of deep learning processors.
 */
class Model {
public:
  /**
   * @brief Load the model parameters.
   * @param params A map of parameters for the model.
   * @return A boolean indicating whether the model was loaded successfully.
   */
  virtual bool load(const string &modelPath) = 0;

  /**
   * @brief Checks if the model is ready.
   * @return A boolean indicating whether the model is ready.
   */
  virtual bool ready() const = 0;
};
