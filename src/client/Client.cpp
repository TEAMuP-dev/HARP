#include "Client.h"

OpResult Client::validateToken(const String& inToken) const
{
    // Create the error here, in case we need it
    Error error;
    int statusCode = 0;

    // Create a GET request to account API with provided token
    auto options = URL::InputStreamOptions(URL::ParameterHandling::inAddress)
                       .withExtraHeaders(getAuthorizationHeader())
                       .withConnectionTimeoutMs(5000)
                       .withStatusCode(&statusCode);

    std::unique_ptr<InputStream> stream(URL(tokenValidationURL).createInputStream(options));

    if (stream == nullptr)
    {
        error.code = statusCode;
        error.devMessage = "Failed to create input stream for GET request \nto validate token.";
        return OpResult::fail(error);
    }

    String response = stream->readEntireStreamAsString();

    // Check the status code to ensure the request was successful
    if (statusCode != 200)
    {
        error.code = statusCode;
        error.devMessage = "Authentication failed with status code: " + String(statusCode);
        return OpResult::fail(error);
    }

    // Parse the response
    var parsedResponse = JSON::parse(response);
    if (! parsedResponse.isObject())
    {
        error.devMessage = "Failed to parse JSON response from validation API.";
        return OpResult::fail(error);
    }

    DynamicObject* obj = parsedResponse.getDynamicObject();
    if (obj == nullptr)
    {
        error.devMessage = "Parsed JSON from validation API is not an object.";
        return OpResult::fail(error);
    }

    return OpResult::ok();
}

String Client::getAuthorizationHeader() const
{
    if (! accessToken.isEmpty())
    {
        return "Authorization: Bearer " + accessToken + "\r\n";
    }
    return "";
}

String Client::getAcceptHeader() const { return "Accept: */*\r\n"; }

String Client::getJsonContentTypeHeader() const { return "Content-Type: application/json\r\n"; }

String Client::createCommonHeaders() const { return getAuthorizationHeader() + getAcceptHeader(); }

String Client::createJsonHeaders() const
{
    return createCommonHeaders() + getJsonContentTypeHeader();
}
