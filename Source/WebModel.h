#pragma once 

#include <fstream>
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
// #include <cpprest/base64.h>

using namespace web::http;
using namespace web::http::client;

#include "Model.h"


class WebModel : public Model
{
public:
    virtual bool load(const map<string, any> &params) override {
        // get the name of the huggingface repo we're going to use
        if (!params.contains("url")) {
            return false;
        }
        if(!params.contains("api_name")){
            return false;
        }
        m_url = any_cast<string>(params.at("url"));
        m_api_name = any_cast<string>(params.at("api_name"));

        m_loaded = true;
        return true;
    }

    virtual bool ready() const override {
        return m_loaded;
    }

    // read audio file to a vector of bytes
    bool read_audio_file_to_base64(const string& filePath, string& output) const {
        std::ifstream file(filePath, std::ios::binary);

        if (!file) {
            // Handle file opening error
            DBG("Error: Failed to open file.");
            return false;
        }

        // get the file size
        file.seekg(0, std::ios::end);
        const std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<char> buffer(fileSize);
        if (!file.read(buffer.data(), fileSize)) {
            std::cerr << "Failed to read file: " << filePath << std::endl;
            return false;
        }

        std::vector<unsigned char> bytes(buffer.begin(), buffer.end());
        output = utility::conversions::to_base64(bytes);

        return true;
    }


    bool process_bytes(const string& audioWavBytes, const string& outputFileName) const {
        // Create an HTTP client object
        try {
            http_client client(U(m_url));
            // DBG("Sending request to " + m_url + m_api_name);

            // Create a JSON object with the request data
            web::json::value requestData;
            requestData[U("data")] = web::json::value::array({ web::json::value::object({
                { U("name"), web::json::value::string(U("audio.wav")) },
                { U("data"), web::json::value::string(audioWavBytes) }
            }) });

            // Create the HTTP request and set the request URI
            http_request request(methods::POST);
            request.set_request_uri(m_api_name);
            request.set_body(requestData);

            // Send the HTTP request and wait for the response
            pplx::task<http_response> response = client.request(request);
            response.wait();

            // Save the response data to a file
            std::ofstream outputFile(outputFileName);
            outputFile << response.get().extract_utf8string().get();
            outputFile.close();
            return true;
        } catch (const std::exception& e) {
            DBG("Error: " + string(e.what()));
            return false;
        }
    }


private:
    string m_url {};
    string m_api_name {};
    bool m_loaded = false;

};

class WebWave2Wave : public WebModel, public Wave2Wave
{
public:

    virtual void process(juce::AudioBuffer<float> *bufferToProcess, int sampleRate) const override {
        // save the buffer to file
        juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("input.wav");
        if (!save_buffer_to_file(*bufferToProcess, tempFile, sampleRate)) {
            DBG("Failed to save buffer to file.");
            return;
        }

        // read the file to a vector of bytes
        string audioWavBytes;
        if (!read_audio_file_to_base64(tempFile.getFullPathName().toStdString(), audioWavBytes)) {
            DBG("Failed to read audio file to base64.");
            return;
        }

        // process the bytes
        juce::File tempOutputFile = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("output.wav");
        if (!process_bytes(audioWavBytes, tempOutputFile.getFullPathName().toStdString())){
            DBG("Failed to process bytes.");
            return;
        }

        // read the output file to a buffer
        // TODO: we're gonna have to resample here? 
        int newSampleRate;
        load_buffer_from_file(tempOutputFile, *bufferToProcess, newSampleRate);
        jassert (newSampleRate == sampleRate);

        // delete the temp input file
        tempFile.deleteFile();
        tempOutputFile.deleteFile();


        return;
    }

};