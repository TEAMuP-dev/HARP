#include "GradioClient.h"

int GradioClient::staticCounter = 0;

SpaceInfo GradioClient::parseSpaceAddress(juce::String spaceAddress, SpaceInfo& spaceInfo)
{
    DBG("Static Counter: " << GradioClient::staticCounter);
    DBG("Space Address: " << spaceAddress);
    // Increment the static counter
    GradioClient::staticCounter++;

    spaceInfo.userInput = spaceAddress;
    // Check if the URL is of Type 4 (localhost or gradio.live)
    if (spaceAddress.contains("localhost") || spaceAddress.contains("gradio.live"))
    {
        spaceInfo.gradio = spaceAddress;
        spaceInfo.huggingface = spaceAddress;
        return;
    }
    juce::String user;
    juce::String model;

    juce::String huggingFaceBaseUrl = "https://huggingface.co/spaces/";
    if (spaceAddress.contains(huggingFaceBaseUrl))
    {
        // Remove the base URL part
        auto spacePath = spaceAddress.fromFirstOccurrenceOf(huggingFaceBaseUrl, false, false);

        // Split the path into user and model using '/'
        auto parts = juce::StringArray::fromTokens(spacePath, "/", "");

        if (parts.size() >= 2)
        {
            user = parts[0];
            model = parts[1];
        }
        else
        {
            // result.huggingface = spaceAddress;
            spaceInfo.error = "Detected huggingface.co URL but could not parse user and model. Too few parts in " + spaceAddress;
            DBG(spaceInfo.error);
            return;
        }
    }
    else if (spaceAddress.contains("hf.space"))
    {
        // Remove the protocol part (e.g., "https://")
        auto withoutProtocol = spaceAddress.fromFirstOccurrenceOf("://", false, false);

        // Extract the subdomain part before ".hf.space/"
        auto subdomain = withoutProtocol.upToFirstOccurrenceOf(".hf.space", false, false);

        // Split the subdomain at the first hyphen
        auto firstHyphenIndex = subdomain.indexOfChar('-');
        if (firstHyphenIndex != -1)
        {
            user = subdomain.substring(0, firstHyphenIndex);
            model = subdomain.substring(firstHyphenIndex + 1);
        }
        else
        {
            // DBG("No hyphen found in the subdomain." << subdomain);
            // Even though the spaceAddress is supposed to be a gradio URL, we return it as the huggingface URL
            // because result.huggingface is used for the  "Open Space URL" button in the error dialog box
            // result.huggingface = spaceAddress;
            spaceInfo.error = "Detected hf.space URL but could not parse user and model. No hyphen found in the subdomain: " + subdomain;
            DBG(spaceInfo.error);
            return;
        }
    }
    // else if address is of the form user/model and doesn't contain http
    else if (spaceAddress.contains("/") && !spaceAddress.contains("http"))
    {
        // Extract user and model
        auto parts = juce::StringArray::fromTokens(spaceAddress, "/", "");
        if (parts.size() == 2)
        {
            user = parts[0];
            model = parts[1];
        }
        else
        {
            // DBG("Invalid URL: " << spaceAddress);
            // result.huggingface = spaceAddress;
            spaceInfo.error = "Detected user/model URL but could not parse user and model. Too many/few slashes in " + spaceAddress;
            DBG(spaceInfo.error);
            return;
        }
    }
    else
    {
        spaceInfo.error = "Invalid URL: " + spaceAddress + ". URL does not match any of the expected patterns.";
        DBG(spaceInfo.error);
        return;
    }

    // Construct the gradio and HF URLs
    if (!user.isEmpty() && !model.isEmpty())
    {   
        model = model.replace("-", "_");
        spaceInfo.huggingface = "https://huggingface.co/spaces/" + user + "/" + model;
        model = model.replace("_", "-");
        spaceInfo.gradio = "https://" + user + "-" + model + ".hf.space";
        spaceInfo.userName = user;
        spaceInfo.modelName = model;
    }
    else
    {
        spaceInfo.error = "Unkown error while parsing the space address: " + spaceAddress;
        DBG(spaceInfo.error);
        return;
    }
    DBG("User: " << user);
    DBG("Model: " << model);
    DBG("Gradio URL: " << spaceInfo.gradio);
    DBG("Huggingface URL: " << spaceInfo.huggingface);
    return;

}

void GradioClient::setSpaceInfo(const juce::String userProvidedSpaceAddress) 
{
    parseSpaceAddress(userProvidedSpaceAddress, spaceInfo);
    // endpoint = juce::URL(resolvedUrl);
    // endpoint = juce::URL ("http://127.0.0.1:7860/call/wav2wav-ctrls");
}
void GradioClient::getControls(
                // juce::URL endpoint, 
                juce::Array<juce::var>& ctrlList, 
                juce::DynamicObject& cardDict,
                juce::String& error
                )
{
    // setSpaceInfo has been called before this method
    // we need to make sure the parsing of the userProvidedSpaceAddress was successful
    // if not, return with an error message
    if (spaceInfo.error.isNotEmpty())
    {
        error = spaceInfo.error;
        return;
    }
    juce::URL controlsEndpoint = endpoint.getChildURL("call/wav2wav-ctrls");
    // First make a POST request to get an event ID
    // The body of the POST request is an empty JSON array
    juce::String jsonBody = R"({"data": []})";
    juce::URL postEndpoint = controlsEndpoint.withPOSTData(jsonBody);
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
    juce::URL getEndpoint = controlsEndpoint.getChildURL(eventID);
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