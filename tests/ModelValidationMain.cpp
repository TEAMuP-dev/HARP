#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include "Model.h"
#include "utils/Errors.h"
#include "utils/Logging.h"

JUCE_IMPLEMENT_SINGLETON(HARPLogger)

using namespace juce;

namespace
{
constexpr auto registryRelativePath = "resources/models/model_registry.json";
constexpr auto audioFixtureRelativePath = "resources/media/test.wav";
constexpr auto midiFixtureRelativePath = "resources/media/test.mid";
constexpr auto defaultTextControlValue = "Short validation prompt";
constexpr int defaultPerModelTimeoutMs = 120000;
constexpr int defaultMaxRetries = 0;
constexpr int defaultRetryDelayMs = 15000;

struct ValidationEntry
{
    String id;
    String name;
    String path;
    String mode;
    String requiredEnv;
    String prompt;
};

struct ValidationResultRow
{
    String id;
    String name;
    String path;
    String outcome; // "passed" | "failed" | "skipped" | "inconclusive"
    String reason;
    bool retryable = false;
};

struct Summary
{
    int passed = 0;
    int failed = 0;
    int skipped = 0;
    int inconclusive = 0;
};

Summary summarize(const std::vector<ValidationResultRow>& results)
{
    Summary summary;

    for (const auto& result : results)
    {
        if (result.outcome == "passed")
            summary.passed += 1;
        else if (result.outcome == "skipped")
            summary.skipped += 1;
        else if (result.outcome == "inconclusive")
            summary.inconclusive += 1;
        else
            summary.failed += 1;
    }

    return summary;
}

File repoRoot()
{
    return File(HARP_SOURCE_DIR);
}

String getEnvValue(const char* name)
{
    if (const char* value = std::getenv(name))
    {
        return String::fromUTF8(value);
    }

    return {};
}

String firstNonEmptyEnv(std::initializer_list<const char*> names)
{
    for (const auto* name : names)
    {
        const auto value = getEnvValue(name);

        if (value.isNotEmpty())
        {
            return value;
        }
    }

    return {};
}

String getSelectedModelId()
{
    return getEnvValue("HARP_MODEL_VALIDATION_ID");
}

int getPerModelTimeoutMs()
{
    const auto value = getEnvValue("HARP_MODEL_VALIDATION_TIMEOUT_MS");

    if (value.isEmpty())
    {
        return defaultPerModelTimeoutMs;
    }

    const auto parsed = value.getIntValue();
    return parsed > 0 ? parsed : defaultPerModelTimeoutMs;
}

int getMaxRetries()
{
    const auto value = getEnvValue("HARP_MODEL_VALIDATION_RETRIES");

    if (value.isEmpty())
    {
        return defaultMaxRetries;
    }

    const auto parsed = value.getIntValue();
    return parsed >= 0 ? parsed : defaultMaxRetries;
}

int getRetryDelayMs()
{
    const auto value = getEnvValue("HARP_MODEL_VALIDATION_RETRY_DELAY_MS");

    if (value.isEmpty())
    {
        return defaultRetryDelayMs;
    }

    const auto parsed = value.getIntValue();
    return parsed > 0 ? parsed : defaultRetryDelayMs;
}

File getReportDir()
{
    const auto envValue = getEnvValue("HARP_MODEL_VALIDATION_REPORT_DIR");

    if (envValue.isNotEmpty())
    {
        return File::isAbsolutePath(envValue) ? File(envValue) : repoRoot().getChildFile(envValue);
    }

    return repoRoot().getChildFile("artifacts/model_validation/remote");
}

void seedProviderTokens(const ValidationEntry& entry, SharedAPIKeys& sharedTokens)
{
    if (entry.mode == "remote_api")
    {
        const auto token = firstNonEmptyEnv({ "HARP_STABILITY_API_KEY", "STABILITY_API_KEY" });

        if (token.isNotEmpty())
        {
            sharedTokens.savedTokens[Provider::Stability] = token;
        }
    }
    else
    {
        const auto token = firstNonEmptyEnv({ "HARP_HUGGINGFACE_TOKEN", "HF_TOKEN" });

        if (token.isNotEmpty())
        {
            sharedTokens.savedTokens[Provider::HuggingFace] = token;
        }
    }
}

var parseJsonFile(const File& file)
{
    const auto parsed = JSON::parse(file.loadFileAsString());

    if (parsed.isVoid())
    {
        throw std::runtime_error(("Failed to parse " + file.getFullPathName()).toStdString());
    }

    return parsed;
}

std::vector<ValidationEntry> loadRemoteValidationEntries(const String& selectedModelId)
{
    const auto parsedRegistry = parseJsonFile(repoRoot().getChildFile(registryRelativePath));

    if (! parsedRegistry.isObject())
    {
        throw std::runtime_error("Model registry root must be a JSON object.");
    }

    const auto* root = parsedRegistry.getDynamicObject();

    if (root == nullptr || ! root->hasProperty("models") || ! root->getProperty("models").isArray())
    {
        throw std::runtime_error("Model registry must contain a models array.");
    }

    std::vector<ValidationEntry> entries;
    std::unordered_set<String> seenIds;

    for (const auto& modelVar : *root->getProperty("models").getArray())
    {
        if (! modelVar.isObject())
            continue;

        const auto* model = modelVar.getDynamicObject();

        if (model == nullptr || ! model->hasProperty("validation"))
            continue;

        const auto validationVar = model->getProperty("validation");

        if (! validationVar.isObject())
            continue;

        const auto* validation = validationVar.getDynamicObject();

        if (validation == nullptr || ! static_cast<bool>(validation->getProperty("enabled")))
            continue;

        const auto mode = validation->getProperty("mode").toString();

        if (mode != "remote_gradio" && mode != "remote_api")
            continue;

        const auto id = model->getProperty("id").toString();

        if (! seenIds.insert(id).second)
        {
            throw std::runtime_error(("Duplicate remote validation model id: " + id).toStdString());
        }

        if (selectedModelId.isNotEmpty() && id != selectedModelId)
            continue;

        entries.push_back({
            id,
            model->getProperty("name").toString(),
            model->getProperty("path").toString(),
            mode,
            validation->getProperty("requires_env").toString(),
            validation->getProperty("prompt").toString(),
        });
    }

    return entries;
}

void seedModelInputs(Model& model, const ValidationEntry& entry)
{
    const auto audioFixture = repoRoot().getChildFile(audioFixtureRelativePath);
    const auto midiFixture = repoRoot().getChildFile(midiFixtureRelativePath);

    for (const auto& input : model.getInputTracks())
    {
        if (auto* audio = dynamic_cast<AudioTrackComponentInfo*>(input.get()))
        {
            if (audio->required)
            {
                audio->path = audioFixture.getFullPathName().toStdString();
            }
        }
        else if (auto* midi = dynamic_cast<MidiTrackComponentInfo*>(input.get()))
        {
            if (midi->required)
            {
                midi->path = midiFixture.getFullPathName().toStdString();
            }
        }
    }

    const auto textControlValue =
        entry.prompt.isNotEmpty() ? entry.prompt.toStdString()
                                  : std::string(defaultTextControlValue);

    for (const auto& control : model.getControls())
    {
        if (auto* text = dynamic_cast<TextBoxComponentInfo*>(control.get()))
        {
            if (text->value.empty())
            {
                text->value = textControlValue;
            }
        }
    }
}

String validateOutputFiles(const std::vector<File>& outputFiles, const LabelList& labels)
{
    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    for (const auto& outputFile : outputFiles)
    {
        if (! outputFile.existsAsFile())
        {
            return "missing output file " + outputFile.getFileName();
        }

        const auto extension = outputFile.getFileExtension().toLowerCase();

        if (extension == ".mid" || extension == ".midi")
        {
            FileInputStream midiStream(outputFile);
            MidiFile midiFile;

            if (! midiStream.openedOk() || ! midiFile.readFrom(midiStream))
            {
                return "unreadable MIDI output " + outputFile.getFileName();
            }

            if (midiFile.getNumTracks() < 1)
            {
                return "MIDI output has no tracks: " + outputFile.getFileName();
            }
        }
        else if (formatManager.findFormatForFileExtension(extension) != nullptr)
        {
            std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(outputFile));

            if (reader == nullptr)
            {
                return "undecodable audio output " + outputFile.getFileName();
            }

            if (reader->lengthInSamples <= 0)
            {
                return "empty audio output " + outputFile.getFileName();
            }
        }
        else if (outputFile.getSize() <= 0)
        {
            return "empty output file " + outputFile.getFileName();
        }
    }

    for (const auto& label : labels)
    {
        if (label == nullptr)
        {
            return "null label output";
        }
    }

    return {};
}

struct FailureInfo
{
    String reason;
    bool retryable = false;
    bool inconclusive = false;
};

FailureInfo classifyFailure(const Error& error)
{
    FailureInfo info;
    info.reason = toUserMessage(error);

    if (const auto* http = std::get_if<HttpError>(&error))
    {
        // Sleeping/restarting spaces and server-side errors are transient
        info.retryable = http->type == HttpError::Type::ConnectionFailed
                         || (http->type == HttpError::Type::BadStatusCode
                             && (http->statusCode == 429 || http->statusCode >= 500));
    }
    else if (const auto* gradio = std::get_if<GradioError>(&error))
    {
        // Quota exhaustion says nothing about the health of the model itself
        info.inconclusive = gradio->detail.containsIgnoreCase("quota");
    }
    else if (const auto* file = std::get_if<FileError>(&error))
    {
        info.retryable = file->type == FileError::Type::UploadFailed
                         || file->type == FileError::Type::DownloadFailed;
    }

    return info;
}

ValidationResultRow makeFailureRow(const ValidationEntry& entry, const Error& error)
{
    const auto failure = classifyFailure(error);

    return { entry.id,
             entry.name,
             entry.path,
             failure.inconclusive ? "inconclusive" : "failed",
             failure.reason,
             failure.retryable };
}

String summarizeReason(const String& reason)
{
    auto lines = StringArray::fromLines(reason);

    for (int index = lines.size() - 1; index >= 0; --index)
    {
        const auto line = lines[index].trim();

        if (line.isNotEmpty())
        {
            return line.replace("|", "\\|");
        }
    }

    return {};
}

String renderMarkdownReport(const std::vector<ValidationResultRow>& results,
                            const Summary& summary)
{
    String markdown;
    markdown << "# HARP Remote Model Validation\n\n";
    markdown << "- Total: " << results.size() << "\n";
    markdown << "- Passed: " << summary.passed << "\n";
    markdown << "- Failed: " << summary.failed << "\n";
    markdown << "- Skipped: " << summary.skipped << "\n";
    markdown << "- Inconclusive: " << summary.inconclusive << "\n\n";
    markdown << "## Dashboard\n\n";
    markdown << "| Model Path | Outcome | Detail |\n";
    markdown << "| --- | --- | --- |\n";

    for (const auto& result : results)
    {
        markdown << "| `" << result.path << "` | " << result.outcome << " | "
                 << summarizeReason(result.reason) << " |\n";
    }

    return markdown;
}

void writeReport(const File& reportDir, const std::vector<ValidationResultRow>& results)
{
    reportDir.createDirectory();

    const auto summary = summarize(results);
    Array<var> rows;

    for (const auto& result : results)
    {
        auto* row = new DynamicObject();
        row->setProperty("id", result.id);
        row->setProperty("name", result.name);
        row->setProperty("path", result.path);
        row->setProperty("outcome", result.outcome);
        row->setProperty("reason", result.reason);
        rows.add(var(row));
    }

    auto* summaryObject = new DynamicObject();
    summaryObject->setProperty("total", static_cast<int>(results.size()));
    summaryObject->setProperty("passed", summary.passed);
    summaryObject->setProperty("failed", summary.failed);
    summaryObject->setProperty("skipped", summary.skipped);
    summaryObject->setProperty("inconclusive", summary.inconclusive);

    auto* report = new DynamicObject();
    report->setProperty("generated_at", Time::getCurrentTime().toISO8601(true));
    report->setProperty("registry_path",
                        repoRoot().getChildFile(registryRelativePath).getFullPathName());
    report->setProperty("summary", var(summaryObject));
    report->setProperty("results", rows);

    reportDir.getChildFile("latest.json").replaceWithText(JSON::toString(var(report), true));
    reportDir.getChildFile("latest.md").replaceWithText(renderMarkdownReport(results, summary));
}

ValidationResultRow validateEntry(const ValidationEntry& entry)
{
    if (entry.requiredEnv.isNotEmpty() && getEnvValue(entry.requiredEnv.toRawUTF8()).isEmpty())
    {
        return { entry.id, entry.name, entry.path, "skipped",
                 "Required environment variable " + entry.requiredEnv + " is not set." };
    }

    // Must outlive the Model below — the shared object is destroyed with its
    // last SharedResourcePointer, and the Client reading the tokens is only
    // created inside Model::load
    SharedResourcePointer<SharedAPIKeys> sharedTokens;
    seedProviderTokens(entry, *sharedTokens);

    Model model;
    const auto loadResult = model.load(entry.path);

    if (loadResult.failed())
    {
        return makeFailureRow(entry, loadResult.getError());
    }

    seedModelInputs(model, entry);

    std::map<Uuid, File> inputFiles;

    for (const auto& input : model.getInputTracks())
    {
        if (auto* audio = dynamic_cast<AudioTrackComponentInfo*>(input.get()))
        {
            if (audio->required)
            {
                inputFiles[input->id] = File(audio->path);
            }
        }
        else if (auto* midi = dynamic_cast<MidiTrackComponentInfo*>(input.get()))
        {
            if (midi->required)
            {
                inputFiles[input->id] = File(midi->path);
            }
        }
    }

    std::vector<File> outputFiles;
    LabelList labels;
    const auto processResult = model.process(inputFiles, outputFiles, labels);

    if (processResult.failed())
    {
        return makeFailureRow(entry, processResult.getError());
    }

    String outputError;

    if (outputFiles.size() != model.getOutputTracks().size())
    {
        outputError = "unexpected number of output files (got "
                      + String(outputFiles.size()) + ", expected "
                      + String(model.getOutputTracks().size()) + ")";
    }
    else
    {
        outputError = validateOutputFiles(outputFiles, labels);
    }

    for (const auto& outputFile : outputFiles)
    {
        ignoreUnused(outputFile.deleteFile());
    }

    if (outputError.isNotEmpty())
    {
        return { entry.id, entry.name, entry.path, "failed", outputError };
    }

    return { entry.id, entry.name, entry.path, "passed", {} };
}

String serializeResultRow(const ValidationResultRow& result)
{
    DynamicObject::Ptr object = new DynamicObject();
    object->setProperty("id", result.id);
    object->setProperty("name", result.name);
    object->setProperty("path", result.path);
    object->setProperty("outcome", result.outcome);
    object->setProperty("reason", result.reason);
    object->setProperty("retryable", result.retryable);
    return JSON::toString(var(object), false).replaceCharacters("\r\n", "  ");
}

bool parseResultRowFromOutput(const String& output, ValidationResultRow& result)
{
    auto lines = StringArray::fromLines(output);

    for (int index = lines.size() - 1; index >= 0; --index)
    {
        const auto line = lines[index].trim();

        if (! line.startsWith("RESULT_JSON:"))
        {
            continue;
        }

        const auto parsed = JSON::parse(line.fromFirstOccurrenceOf("RESULT_JSON:", false, false).trim());

        if (! parsed.isObject())
        {
            return false;
        }

        const auto* object = parsed.getDynamicObject();

        if (object == nullptr)
        {
            return false;
        }

        result.id = object->getProperty("id").toString();
        result.name = object->getProperty("name").toString();
        result.path = object->getProperty("path").toString();
        result.outcome = object->getProperty("outcome").toString();
        result.reason = object->getProperty("reason").toString();
        result.retryable = static_cast<bool>(object->getProperty("retryable"));
        return true;
    }

    return false;
}

bool isRetryableFailure(const ValidationResultRow& result)
{
    return result.outcome == "failed" && result.retryable;
}

ValidationResultRow runEntryInChildProcessOnce(const ValidationEntry& entry)
{
    // The child inherits this process's environment (tokens, timeout, report
    // dir); only the model id needs passing, and argv keeps secrets off the
    // command line and works on Windows
    StringArray command;
    command.add(File::getSpecialLocation(File::currentExecutableFile).getFullPathName());
    command.add("--child");
    command.add(entry.id);

    ChildProcess child;

    if (! child.start(command))
    {
        return { entry.id, entry.name, entry.path, "failed", "failed to launch child process" };
    }

    // Drain output while waiting — an undrained pipe blocks the child once the
    // buffer fills, which would surface as a bogus timeout
    String output;
    std::thread outputReader([&child, &output] { output = child.readAllProcessOutput(); });

    const bool finished = child.waitForProcessToFinish(getPerModelTimeoutMs());

    if (! finished)
    {
        child.kill();
    }

    outputReader.join();

    if (! finished)
    {
        return { entry.id, entry.name, entry.path, "failed", "model validation timed out", true };
    }

    ValidationResultRow result;

    if (parseResultRowFromOutput(output, result))
    {
        return result;
    }

    const auto exitCode = child.getExitCode();
    const auto trimmedOutput = output.trim();
    const auto fallbackReason = trimmedOutput.isNotEmpty()
        ? summarizeReason(trimmedOutput)
        : "child process exited with code " + String(exitCode);

    return { entry.id, entry.name, entry.path, "failed", fallbackReason };
}

ValidationResultRow runEntryInChildProcess(const ValidationEntry& entry)
{
    const auto maxRetries = getMaxRetries();
    const auto retryDelayMs = getRetryDelayMs();

    auto result = runEntryInChildProcessOnce(entry);

    for (int attempt = 1; attempt <= maxRetries && isRetryableFailure(result); ++attempt)
    {
        std::cout << "  retry " << attempt << "/" << maxRetries
                  << " (waiting " << retryDelayMs / 1000 << "s)...\n";

        Thread::sleep(retryDelayMs);
        result = runEntryInChildProcessOnce(entry);
    }

    return result;
}

int runChildMode(const String& childModelId)
{
    const auto entries = loadRemoteValidationEntries(childModelId);

    if (entries.size() != 1)
    {
        std::cerr << "Child mode: model id \"" << childModelId
                  << "\" did not match exactly one enabled remote registry entry.\n";
        return 1;
    }

    const auto result = validateEntry(entries.front());
    std::cout << "RESULT_JSON: " << serializeResultRow(result) << '\n';
    return result.outcome == "failed" ? 1 : 0;
}
} // namespace

int main(int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI scopedJuce;

    try
    {
        for (int i = 1; i + 1 < argc; ++i)
        {
            if (String(argv[i]) == "--child")
            {
                return runChildMode(String::fromUTF8(argv[i + 1]));
            }
        }

        const auto entries = loadRemoteValidationEntries(getSelectedModelId());
        const auto reportDir = getReportDir();

        if (entries.empty())
        {
            std::cerr << "No enabled remote validation entries found.\n";
            return 1;
        }

        const auto totalEntries = static_cast<int>(entries.size());

        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "  HARP Remote Model Validation\n";
        std::cout << "========================================\n";
        std::cout << "  Models to validate: " << totalEntries << "\n";
        std::cout << "  Timeout per model:  " << getPerModelTimeoutMs() / 1000 << "s\n";
        std::cout << "  Max retries:        " << getMaxRetries() << "\n";
        std::cout << "----------------------------------------\n\n";

        std::vector<ValidationResultRow> results;
        results.reserve(entries.size());

        for (int i = 0; i < totalEntries; ++i)
        {
            const auto& entry = entries[static_cast<size_t>(i)];

            std::cout << "[" << (i + 1) << "/" << totalEntries << "] "
                      << entry.path << " ... " << std::flush;

            const auto result = runEntryInChildProcess(entry);
            results.push_back(result);

            // Write after every model so a killed run still leaves a report
            writeReport(reportDir, results);

            if (result.outcome == "passed")
                std::cout << "PASSED\n";
            else if (result.outcome == "skipped")
                std::cout << "SKIPPED (" << result.reason << ")\n";
            else if (result.outcome == "inconclusive")
                std::cout << "INCONCLUSIVE (" << result.reason << ")\n";
            else
                std::cout << "FAILED\n";
        }

        const auto summary = summarize(results);

        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "  Summary\n";
        std::cout << "========================================\n";
        std::cout << "  Passed:       " << summary.passed << " / " << totalEntries << "\n";
        std::cout << "  Failed:       " << summary.failed << " / " << totalEntries << "\n";
        std::cout << "  Skipped:      " << summary.skipped << " / " << totalEntries << "\n";
        std::cout << "  Inconclusive: " << summary.inconclusive << " / " << totalEntries << "\n";
        std::cout << "----------------------------------------\n";

        if (summary.failed > 0)
        {
            std::cout << "\n  Failed models:\n\n";

            for (const auto& result : results)
            {
                if (result.outcome == "passed" || result.outcome == "skipped"
                    || result.outcome == "inconclusive")
                    continue;

                std::cout << "    " << result.path << "\n";
                std::cout << "      Reason: " << result.reason << "\n\n";
            }
        }

        std::cout << "========================================\n\n";

        return summary.failed > 0 ? 1 : 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
