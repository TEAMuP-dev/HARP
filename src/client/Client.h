/**
 * @file
 * @brief The Client class for making http requests to API
 * @author huiranyu
 */

#pragma once

#include <fstream>

#include "../HarpLogger.h"
#include "../errors.h"
#include "../utils.h"
#include "juce_core/juce_core.h"
class Client

{
public:
    Client() = default;
    virtual ~Client() {};

    // Space Info
    virtual OpResult setSpaceInfo(const SpaceInfo&) = 0;

    virtual SpaceInfo getSpaceInfo() const = 0;

    // Requests
    virtual OpResult uploadFileRequest(const juce::File&,
                                       juce::String&,
                                       const int timeoutMs = 10000) const = 0;

    virtual OpResult processRequest(Error&,
                                    juce::String&,
                                    std::vector<juce::String>&,
                                    LabelList&) = 0;

    virtual OpResult getControls(juce::Array<juce::var>& inputComponents,
                                 juce::Array<juce::var>& outputComponents,
                                 juce::DynamicObject& cardDict) = 0;

    virtual OpResult cancel() = 0;
    
    // Authorization
    virtual void setToken(const juce::String& token) = 0;

    virtual juce::String getToken() const = 0;

    virtual void setTokenEnabled(bool enabled) = 0;

    virtual OpResult validateToken(const juce::String& token) const = 0;

private:
    
    juce::String getAuthorizationHeader() const;
    juce::String getJsonContentTypeHeader() const;
    juce::String getAcceptHeader() const;
    juce::String createCommonHeaders() const;
    juce::String createJsonHeaders() const;

    SpaceInfo spaceInfo;
    juce::String token;
    // A bool flag that could be controlled by a checkbox
    bool tokenEnabled = true;
};