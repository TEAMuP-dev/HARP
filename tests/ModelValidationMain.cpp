#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unordered_set>

#include <juce_core/juce_core.h>

#include "Model.h"
#include "utils/Logging.h"

JUCE_IMPLEMENT_SINGLETON(HARPLogger)

using namespace juce;

namespace
{
constexpr auto registryRelativePath = "resources/models/model_registry.json";
constexpr auto audioFixtureRelativePath = "resources/media/test.wav";
constexpr auto midiFixtureRelativePath = "resources/media/test.mid";
constexpr int defaultPerModelTimeoutMs = 120000;

struct ValidationEntry
{
    String id;
    String name;
    String path;
    String mode;
    String requiredEnv;
};

struct ValidationResultRow
{
    String id;
    String name;
    String path;
    String outcome;
    String reason;
};

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

bool isChildMode()
{
    return getEnvValue("HARP_MODEL_VALIDATION_CHILD") == "1";
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

File getReportDir()
{
    const auto envValue = getEnvValue("HARP_MODEL_VALIDATION_REPORT_DIR");

    if (envValue.isNotEmpty())
    {
        return File::isAbsolutePath(envValue) ? File(envValue) : repoRoot().getChildFile(envValue);
    }

    return repoRoot().getChildFile("artifacts/model_validation/remote");
}

void seedProviderTokens(const ValidationEntry& entry)
{
    SharedResourcePointer<SharedAPIKeys> sharedTokens;

    if (entry.mode == "remote_api")
    {
        const auto token = firstNonEmptyEnv({ "HARP_STABILITY_API_KEY", "STABILITY_API_KEY" });

        if (token.isNotEmpty())
        {
            sharedTokens->savedTokens[Provider::Stability] = token;
        }
    }
    else
    {
        const auto token = firstNonEmptyEnv({ "HARP_HUGGINGFACE_TOKEN", "HF_TOKEN" });

        if (token.isNotEmpty())
        {
            sharedTokens->savedTokens[Provider::HuggingFace] = token;
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

std::vector<ValidationEntry> loadRemoteValidationEntries()
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

    const auto selectedModelId = getSelectedModelId();
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
        });
    }

    return entries;
}

void seedModelInputs(Model& model)
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

    for (const auto& control : model.getControls())
    {
        if (auto* text = dynamic_cast<TextBoxComponentInfo*>(control.get()))
        {
            if (text->value.empty())
            {
                text->value = "Short validation prompt";
            }
        }
    }
}

String validateOutputFiles(const std::vector<File>& outputFiles, const LabelList& labels)
{
    for (const auto& outputFile : outputFiles)
    {
        if (! outputFile.existsAsFile())
        {
            return "missing output file";
        }

        const auto extension = outputFile.getFileExtension().toLowerCase();

        if (extension == ".mid" || extension == ".midi")
        {
            std::ifstream midiStream(outputFile.getFullPathName().toStdString(), std::ios::binary);
            char header[4] {};
            midiStream.read(header, 4);

            if (String::fromUTF8(header, 4) != "MThd")
            {
                return "invalid MIDI output";
            }
        }
        else if (outputFile.getSize() <= 0)
        {
            return "empty output file";
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

String classifyFailureMessage(const String& message)
{
    if (message.containsIgnoreCase("status code 503"))
        return "503 Service Unavailable";
    if (message.containsIgnoreCase("timed out"))
        return "remote Gradio timeout";
    if (message.containsIgnoreCase("runtime error occurred at endpoint"))
        return "remote Gradio runtime error";
    if (message.containsIgnoreCase("valid API key"))
        return "missing or invalid API key";

    return message.isNotEmpty() ? message : "unknown failure";
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

String renderMarkdownReport(const std::vector<ValidationResultRow>& results)
{
    int passed = 0;
    int failed = 0;
    int skipped = 0;

    for (const auto& result : results)
    {
        if (result.outcome == "passed")
            passed += 1;
        else if (result.outcome == "failed")
            failed += 1;
        else if (result.outcome == "skipped")
            skipped += 1;
    }

    String markdown;
    markdown << "# HARP Remote Model Validation\n\n";
    markdown << "- Total: " << results.size() << "\n";
    markdown << "- Passed: " << passed << "\n";
    markdown << "- Failed: " << failed << "\n";
    markdown << "- Skipped: " << skipped << "\n\n";
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

    int passed = 0;
    int failed = 0;
    int skipped = 0;
    Array<var> rows;

    for (const auto& result : results)
    {
        if (result.outcome == "passed")
            passed += 1;
        else if (result.outcome == "failed")
            failed += 1;
        else if (result.outcome == "skipped")
            skipped += 1;

        auto* row = new DynamicObject();
        row->setProperty("id", result.id);
        row->setProperty("name", result.name);
        row->setProperty("path", result.path);
        row->setProperty("outcome", result.outcome);
        row->setProperty("reason", result.reason);
        rows.add(var(row));
    }

    auto* summary = new DynamicObject();
    summary->setProperty("total", static_cast<int>(results.size()));
    summary->setProperty("passed", passed);
    summary->setProperty("failed", failed);
    summary->setProperty("skipped", skipped);

    auto* report = new DynamicObject();
    report->setProperty("generated_at", Time::getCurrentTime().toISO8601(true));
    report->setProperty("registry_path", repoRoot().getChildFile(registryRelativePath).getFullPathName());
    report->setProperty("summary", var(summary));
    report->setProperty("results", rows);

    const auto reportJson = JSON::toString(var(report), true);
    const auto reportMd = renderMarkdownReport(results);

    reportDir.getChildFile("latest.json").replaceWithText(reportJson);
    reportDir.getChildFile("latest.md").replaceWithText(reportMd);
    reportDir.getChildFile("status.json").replaceWithText(reportJson);
    reportDir.getChildFile("dashboard.md").replaceWithText(reportMd);
}

ValidationResultRow validateEntry(const ValidationEntry& entry)
{
    if (entry.requiredEnv.isNotEmpty() && getEnvValue(entry.requiredEnv.toRawUTF8()).isEmpty())
    {
        return { entry.id, entry.name, entry.path, "skipped",
                 "Required environment variable " + entry.requiredEnv + " is not set." };
    }

    seedProviderTokens(entry);

    Model model;
    const auto loadResult = model.load(entry.path);

    if (loadResult.failed())
    {
        return { entry.id,
                 entry.name,
                 entry.path,
                 "failed",
                 classifyFailureMessage(toUserMessage(loadResult.getError())) };
    }

    seedModelInputs(model);

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
        return { entry.id,
                 entry.name,
                 entry.path,
                 "failed",
                 classifyFailureMessage(toUserMessage(processResult.getError())) };
    }

    if (static_cast<int>(outputFiles.size()) != static_cast<int>(model.getOutputTracks().size()))
    {
        return { entry.id, entry.name, entry.path, "failed", "unexpected number of output files" };
    }

    const auto outputError = validateOutputFiles(outputFiles, labels);

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
        return true;
    }

    return false;
}

ValidationResultRow runEntryInChildProcess(const ValidationEntry& entry)
{
    StringArray command;
    command.add("env");
    command.add("HARP_MODEL_VALIDATION_CHILD=1");
    command.add("HARP_MODEL_VALIDATION_ID=" + entry.id);
    command.add("HARP_MODEL_VALIDATION_TIMEOUT_MS=" + String(getPerModelTimeoutMs()));

    const auto reportDir = getReportDir();
    command.add("HARP_MODEL_VALIDATION_REPORT_DIR=" + reportDir.getFullPathName());

    const auto hfToken = firstNonEmptyEnv({ "HARP_HUGGINGFACE_TOKEN", "HF_TOKEN" });
    if (hfToken.isNotEmpty())
    {
        command.add("HARP_HUGGINGFACE_TOKEN=" + hfToken);
    }

    const auto stabilityKey = firstNonEmptyEnv({ "HARP_STABILITY_API_KEY", "STABILITY_API_KEY" });
    if (stabilityKey.isNotEmpty())
    {
        command.add("HARP_STABILITY_API_KEY=" + stabilityKey);
    }

    command.add(File::getSpecialLocation(File::currentExecutableFile).getFullPathName());

    ChildProcess child;

    if (! child.start(command))
    {
        return { entry.id, entry.name, entry.path, "failed", "failed to launch child process" };
    }

    if (! child.waitForProcessToFinish(getPerModelTimeoutMs()))
    {
        child.kill();
        return { entry.id, entry.name, entry.path, "failed", "model validation timed out" };
    }

    const auto output = child.readAllProcessOutput();
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
} // namespace

int main(int argc, char* argv[])
{
    ignoreUnused(argc, argv);

    ScopedJuceInitialiser_GUI scopedJuce;

    try
    {
        const auto entries = loadRemoteValidationEntries();
        const auto reportDir = getReportDir();

        if (entries.empty())
        {
            std::cerr << "No enabled remote validation entries found.\n";
            return 1;
        }

        if (isChildMode())
        {
            const auto result = validateEntry(entries.front());
            std::cout << "RESULT_JSON: " << serializeResultRow(result) << '\n';
            return result.outcome == "failed" ? 1 : 0;
        }

        std::vector<ValidationResultRow> results;
        results.reserve(entries.size());

        for (const auto& entry : entries)
        {
            const auto result = runEntryInChildProcess(entry);
            results.push_back(result);

            std::cout << result.path << " - " << result.outcome;

            if (result.reason.isNotEmpty())
            {
                std::cout << ": " << result.reason;
            }

            std::cout << '\n';
        }

        writeReport(reportDir, results);

        const auto hasFailures = std::any_of(results.begin(), results.end(), [](const auto& result) {
            return result.outcome == "failed";
        });

        return hasFailures ? 1 : 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}