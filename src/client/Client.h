/**
 * @file
 * @brief Parent class for making HTTP requests to an API
 * @author huiranyu
 */

#pragma once

#include "juce_core/juce_core.h"
#include <fstream>

#include "../HarpLogger.h"
#include "../errors.h"
#include "../utils.h"

using namespace juce;

class Client
{
public:
    Client() = default;
    virtual ~Client() {};

    // Space Info
    virtual OpResult setSpaceInfo(const SpaceInfo&) = 0;
    SpaceInfo getSpaceInfo() const { return spaceInfo; }

    // Requests
    virtual OpResult getControls(Array<var>& inputComponents,
                                 Array<var>& outputComponents,
                                 DynamicObject& cardDict) = 0;
    virtual OpResult uploadFileRequest(const File&, String&, const int timeoutMs = 10000) const = 0;
    virtual OpResult processRequest(Error&, String&, std::vector<String>&, LabelList&) = 0;
    virtual OpResult cancel() = 0;

    // Authorization
    void setToken(const String& t) { accessToken = t; }
    String getToken() const { return accessToken; }

    //OpResult queryToken(const String& token) const;
    virtual OpResult validateToken(const String& newToken) const;

protected:
    String getAuthorizationHeader(String t = "") const;
    String getAcceptHeader() const;
    String getJsonContentTypeHeader() const;

    String createCommonHeaders() const;
    String createJsonHeaders() const;

    String accessToken;
    URL tokenValidationURL;

    SpaceInfo spaceInfo;
};
