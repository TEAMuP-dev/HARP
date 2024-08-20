/**
 * @file
 * @brief A collection of functions to interact with the Gradio API
 * @author  xribene
 */

#pragma once

#include <fstream>
#include "juce_core/juce_core.h"
#include "utils.h"

static juce::String resolveSpaceUrl(juce::String urlOrName) {
    if (urlOrName.contains("localhost") || urlOrName.contains("huggingface.co") || urlOrName.contains("http")){
        // do nothing! the url is already valid
    }
    else {
        DBG("HARPProcessorEditor::buttonClicked: spaceUrl is not a valid url");
        urlOrName = "https://huggingface.co/spaces/" + urlOrName;
    }
    return urlOrName;
}

using CtrlList = std::vector<std::pair<juce::Uuid, std::shared_ptr<Ctrl>>>;

void getControls(
                juce::URL endpoint, 
                juce::Array<juce::var>& ctrlList, 
                juce::DynamicObject& cardDict,
                juce::String& error
                )
{
    // First make a POST request to get an event ID
    // The body of the POST request is an empty JSON array
    juce::String jsonBody = R"({"data": []})";
    juce::URL postEndpoint = endpoint.withPOSTData(jsonBody);
    juce::StringPairArray responseHeaders;
    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
	                   .withExtraHeaders("Content-Type: application/json\r\nAccept: */*")
	                   .withConnectionTimeoutMs (10000)
	                   .withResponseHeaders (&responseHeaders)
	                   .withStatusCode (&statusCode)
	                   .withNumRedirectsToFollow (5)
	                   .withHttpRequestCmd ("POST");

    std::unique_ptr<juce::InputStream> stream (postEndpoint.createInputStream (options));

    juce::String eventID;
    if (stream != nullptr)
    {
        juce::String response = stream->readEntireStreamAsString();
        // DBG("Response: " << response);
        // DBG("Status Code: " << status_code);

        // Check the status code to ensure the request was successful
        if (statusCode == 200)
        {
            juce::var parsedResponse = juce::JSON::parse(response);
            if (parsedResponse.isObject())
            {   
                juce::DynamicObject* obj = parsedResponse.getDynamicObject();
                eventID = obj->getProperty("event_id");
                // if (eventID.isString())
                //     eventID = eventID.toString();                    
                // else
                // {
                //     error = "event_id not found in the response.";
                //     DBG(error);
                // }
            }
            else
            {
                error = "Failed to parse JSON response.";
                DBG(error);
            }
        }
        else
        {
            error = "Request failed with status code: " + juce::String(statusCode);
            DBG(error);
        }
    }
    else
    {
        error = "Failed to create input stream for POST request.";
        DBG(error);
    }

    if (error.isNotEmpty())
        return;

    // Now we make a GET request to the endpoint with the event ID appended
    // The endpoint for the get request is the same as the post request with /{eventID} appended
    juce::URL getEndpoint = endpoint.getChildURL(eventID);
    juce::StringPairArray get_response_headers;
    int getStatusCode = 0;
    auto getOptions = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
	                   .withExtraHeaders("Content-Type: application/json\r\nAccept: */*")
	                   .withConnectionTimeoutMs (10000)
	                   .withResponseHeaders (&get_response_headers)
	                   .withStatusCode (&getStatusCode)
	                   .withNumRedirectsToFollow (5);
	                  //  .withHttpRequestCmd ("POST");
    std::unique_ptr<juce::InputStream> getStream(getEndpoint.createInputStream(getOptions));
    
    if (getStream != nullptr)
    {
        // Read the entire response from the stream
        juce::String getResponse = getStream->readEntireStreamAsString();
        // DBG("Response: " << getResponse);

        // From the gradio app we receive a JSON string 
        // ( see core.py in pyharp --> gr.Text(label="Controls"))
        // Extract the data portion from the response
        juce::String dataKey = "data: ";
        int dataIndex = getResponse.indexOf(dataKey);

        if (dataIndex != -1){
            // Extract the portion after "data: "
            juce::String dataPortion = getResponse.substring(dataIndex + dataKey.length()).trim();

            // The data is wrapped in quotes and brackets, so remove them
            if (dataPortion.startsWith("[\"") && dataPortion.endsWith("\"]")){
                dataPortion = dataPortion.substring(2, dataPortion.length() - 2);
            }
            // DBG("Extracted Data Portion: " << dataPortion);
            // Replace single quotes with double quotes to make it valid JSON
            dataPortion = dataPortion.replace("'", "\"");

            // Replace Python booleans (True) with JSON booleans (true)
            dataPortion = dataPortion.replace("False", "false").replace("True", "true");

            // Parse the extracted JSON string
            juce::var parsedData;
            juce::Result parseResult = juce::JSON::parse(dataPortion, parsedData);
            
            if (parsedData.isObject()){
                juce::DynamicObject* obj = parsedData.getDynamicObject();
                if (obj != nullptr){
                    // Get the card and controls objects from the parsed data
                    juce::DynamicObject* cardObj = obj->getProperty("card").getDynamicObject();
                    if (cardObj != nullptr)
                        cardDict = *cardObj;  
                    else
                    {
                        error = "Couldn't load the modelCard dict from the controls response.";
                        DBG(error);
                    }
                    juce::Array<juce::var>* ctrlArray = obj->getProperty("ctrls").getArray();
                    if (ctrlArray != nullptr)
                        ctrlList = *ctrlArray;  
                    else
                    {
                        error = "Couldn't load the controls array/list from the controls response.";
                        DBG(error);
                    }
                }
            }
            else{
                error = "Failed to parse the data portion of the received controls JSON.";
                DBG(error);
            }
        }
        else{
            error = "Data key not found in the controls response.";
            DBG(error);
        }
    }
    else
    {
        error = "Failed to create input stream for GET request.";
        DBG(error);
    }

    if (error.isNotEmpty())
        return;
}