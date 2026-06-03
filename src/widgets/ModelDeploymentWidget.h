/**
 * @file ModelDeploymentWidget.h
 * @brief Temporary frontend for packaging Hugging Face models with tools/model_agent.
 */

#pragma once

#include <atomic>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../widgets/StatusAreaWidget.h"

#include "../utils/Logging.h"

using namespace juce;

class ModelDeploymentWidget : public Component, private Timer
{
public:
    ModelDeploymentWidget() : progressBar(progressValue)
    {
        titleLabel.setText("Model Deployment", dontSendNotification);
        titleLabel.setFont(Font(FontOptions(22.0f, Font::bold)));
        addAndMakeVisible(titleLabel);

        setupLabel(modelRepoLabel, "Hugging Face model");
        setupLabel(outputLabel, "Package output");
        setupLabel(harpRootLabel, "HARP root");

        setupEditor(modelRepoEditor, "author/model-name or https://huggingface.co/author/model-name");
        modelRepoEditor.onTextChange = [this] { validateInputs(); };

        const auto defaultOutput =
            File::getSpecialLocation(File::tempDirectory).getChildFile("harp_model_agent_hf_spaces");
        setupEditor(outputEditor, defaultOutput.getFullPathName());
        outputEditor.onTextChange = [this] { validateInputs(); };

        setupEditor(harpRootEditor, File::getCurrentWorkingDirectory().getFullPathName());
        harpRootEditor.onTextChange = [this] { validateInputs(); };

        validateButton.setButtonText("Validate");
        validateButton.onClick = [this] { validateInputs(true); };
        addAndMakeVisible(validateButton);

        deployButton.setButtonText("Deploy");
        deployButton.onClick = [this] { deployModel(); };
        addAndMakeVisible(deployButton);

        clearButton.setButtonText("Clear Log");
        clearButton.onClick = [this] { clearLog(); };
        addAndMakeVisible(clearButton);

        statusLabel.setText("Ready", dontSendNotification);
        statusLabel.setFont(Font(FontOptions(15.0f, Font::bold)));
        addAndMakeVisible(statusLabel);

        validationLabel.setText("Enter a Hugging Face model repository to begin.",
                                dontSendNotification);
        validationLabel.setColour(Label::textColourId, Colours::lightgrey);
        addAndMakeVisible(validationLabel);

        progressValue = 0.0;
        addAndMakeVisible(progressBar);

        logEditor.setMultiLine(true);
        logEditor.setReadOnly(true);
        logEditor.setScrollbarsShown(true);
        logEditor.setCaretVisible(false);
        logEditor.setFont(Font(FontOptions(13.0f)));
        logEditor.setText("Deployment log will appear here.", dontSendNotification);
        addAndMakeVisible(logEditor);

        setSize(760, 520);
        validateInputs();
    }

    ~ModelDeploymentWidget() override
    {
        stopTimer();
        if (deploymentThread != nullptr)
        {
            deploymentThread->signalThreadShouldExit();
            deploymentThread->waitForThreadToExit(2000);
        }
    }

    void paint(Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14);
        titleLabel.setBounds(area.removeFromTop(32));
        area.removeFromTop(8);

        auto formArea = area.removeFromTop(138);
        layoutField(formArea, modelRepoLabel, modelRepoEditor);
        formArea.removeFromTop(8);
        layoutField(formArea, outputLabel, outputEditor);
        formArea.removeFromTop(8);
        layoutField(formArea, harpRootLabel, harpRootEditor);

        area.removeFromTop(8);
        auto buttonRow = area.removeFromTop(32);
        validateButton.setBounds(buttonRow.removeFromLeft(120));
        buttonRow.removeFromLeft(8);
        deployButton.setBounds(buttonRow.removeFromLeft(120));
        buttonRow.removeFromLeft(8);
        clearButton.setBounds(buttonRow.removeFromLeft(120));

        area.removeFromTop(10);
        statusLabel.setBounds(area.removeFromTop(24));
        validationLabel.setBounds(area.removeFromTop(24));
        area.removeFromTop(6);
        progressBar.setBounds(area.removeFromTop(18));
        area.removeFromTop(10);
        logEditor.setBounds(area);
    }

private:
    struct DeploymentRequest
    {
        String repo;
        File outputDir;
        File harpRoot;
    };

    class DeploymentThread : public Thread
    {
    public:
        explicit DeploymentThread(DeploymentRequest request)
            : Thread("Model Agent Deployment"), deploymentRequest(std::move(request))
        {
        }

        void run() override
        {
            appendLog("Running model-agent backend...\n");
            appendLog(buildCommandPreview(deploymentRequest) + "\n\n");

            ChildProcess process;
            if (! process.start(buildShellCommand(deploymentRequest)))
            {
                exitCode = -1;
                appendLog("Could not start model-agent process.\n");
                finished = true;
                return;
            }

            while (process.isRunning() && ! threadShouldExit())
            {
                readAvailableOutput(process);
                wait(80);
            }

            if (threadShouldExit() && process.isRunning())
            {
                process.kill();
                exitCode = -1;
                appendLog("\nDeployment canceled.\n");
            }
            else
            {
                readAvailableOutput(process);
                exitCode = (int) process.getExitCode();
                appendLog(exitCode == 0 ? "\nDeployment completed.\n" : "\nDeployment failed.\n");
            }

            finished = true;
        }

        String consumeLog()
        {
            const ScopedLock lock(logLock);
            auto text = pendingLog;
            pendingLog.clear();
            return text;
        }

        bool hasFinished() const { return finished; }
        int getExitCode() const { return exitCode; }

        static String buildCommandPreview(const DeploymentRequest& request)
        {
            return "cd " + request.harpRoot.getFullPathName()
                   + "\nPYTHONDONTWRITEBYTECODE=1 python3 -B -m tools.model_agent package-repo "
                   + request.repo + " --output " + request.outputDir.getFullPathName();
        }

    private:
        static String shellQuote(const String& value)
        {
            return "'" + value.replace("'", "'\"'\"'") + "'";
        }

        static StringArray buildShellCommand(const DeploymentRequest& request)
        {
            const String script =
                "cd " + shellQuote(request.harpRoot.getFullPathName())
                + " && PYTHONDONTWRITEBYTECODE=1 python3 -B -m tools.model_agent package-repo "
                + shellQuote(request.repo) + " --output "
                + shellQuote(request.outputDir.getFullPathName());

            StringArray args;
            args.add("/bin/bash");
            args.add("-lc");
            args.add(script);
            return args;
        }

        void readAvailableOutput(ChildProcess& process)
        {
            char buffer[4096] {};
            for (;;)
            {
                const int bytesRead = process.readProcessOutput(buffer, (int) sizeof(buffer));
                if (bytesRead <= 0)
                    break;

                appendLog(String::fromUTF8(buffer, bytesRead));
            }
        }

        void appendLog(const String& text)
        {
            const ScopedLock lock(logLock);
            pendingLog += text;
        }

        DeploymentRequest deploymentRequest;
        CriticalSection logLock;
        String pendingLog;
        std::atomic<bool> finished { false };
        std::atomic<int> exitCode { -1 };
    };

    static void setupLabel(Label& label, const String& text)
    {
        label.setText(text, dontSendNotification);
        label.setJustificationType(Justification::centredLeft);
    }

    void setupEditor(TextEditor& editor, const String& placeholder)
    {
        editor.setMultiLine(false);
        editor.setReturnKeyStartsNewLine(false);
        editor.setTextToShowWhenEmpty(placeholder, Colours::grey);
        editor.setWantsKeyboardFocus(true);
        addAndMakeVisible(editor);
    }

    void layoutField(Rectangle<int>& area, Label& label, TextEditor& editor)
    {
        auto row = area.removeFromTop(40);
        label.setBounds(row.removeFromLeft(150));
        row.removeFromLeft(8);
        editor.setBounds(row);
    }

    void validateInputs(bool announceValid = false)
    {
        const auto repo = getRepoText();
        const auto root = File(harpRootEditor.getText());
        const auto output = File(outputEditor.getText());

        String message;
        bool valid = true;

        if (! isValidRepo(repo))
        {
            valid = false;
            message = "Use a Hugging Face model id like author/model-name.";
        }
        else if (! root.getChildFile("tools/model_agent").exists())
        {
            valid = false;
            message = "HARP root must contain tools/model_agent.";
        }
        else if (output.getFullPathName().isEmpty())
        {
            valid = false;
            message = "Choose an output folder for generated Space files.";
        }
        else
        {
            message = announceValid ? "Validation passed. Ready to deploy." : "Ready to deploy.";
        }

        validationLabel.setText(message, dontSendNotification);
        validationLabel.setColour(Label::textColourId, valid ? Colours::lightgreen : Colours::orange);
        deployButton.setEnabled(valid && ! isDeploymentRunning());
        validateButton.setEnabled(! isDeploymentRunning());
    }

    String getRepoText() const
    {
        auto text = modelRepoEditor.getText().trim().trimCharactersAtEnd("/");
        const String prefix = "https://huggingface.co/";
        if (text.startsWith(prefix))
            text = text.substring(prefix.length());
        if (text.startsWith("models/"))
            text = text.substring(7);
        return text;
    }

    static bool isValidRepo(const String& repo)
    {
        if (repo.isEmpty() || repo.contains(" ") || repo.startsWith("spaces/"))
            return false;
        StringArray parts;
        parts.addTokens(repo, "/", "");
        parts.removeEmptyStrings();
        return parts.size() == 2;
    }

    bool isDeploymentRunning() const
    {
        return deploymentThread != nullptr && deploymentThread->isThreadRunning();
    }

    void deployModel()
    {
        validateInputs();
        if (! deployButton.isEnabled())
            return;

        DeploymentRequest request { getRepoText(),
                                    File(outputEditor.getText()),
                                    File(harpRootEditor.getText()) };

        clearLog();
        statusLabel.setText("Deploying...", dontSendNotification);
        progressValue = -1.0;
        deployButton.setEnabled(false);
        validateButton.setEnabled(false);
        deploymentThread = std::make_unique<DeploymentThread>(request);
        deploymentThread->startThread();
        startTimerHz(10);
    }

    void clearLog()
    {
        logEditor.clear();
    }

    void timerCallback() override
    {
        if (deploymentThread == nullptr)
            return;

        const auto newLog = deploymentThread->consumeLog();
        if (newLog.isNotEmpty())
        {
            logEditor.moveCaretToEnd();
            logEditor.insertTextAtCaret(newLog);
        }

        if (deploymentThread->hasFinished())
        {
            deploymentThread->waitForThreadToExit(50);
            const bool succeeded = deploymentThread->getExitCode() == 0;
            statusLabel.setText(succeeded ? "Deployment complete" : "Deployment failed",
                                dontSendNotification);
            progressValue = succeeded ? 1.0 : 0.0;
            deploymentThread.reset();
            stopTimer();
            validateInputs();
        }
    }

    Label titleLabel;
    Label modelRepoLabel;
    Label outputLabel;
    Label harpRootLabel;
    Label statusLabel;
    Label validationLabel;

    TextEditor modelRepoEditor;
    TextEditor outputEditor;
    TextEditor harpRootEditor;
    TextEditor logEditor;

    TextButton validateButton;
    TextButton deployButton;
    TextButton clearButton;

    double progressValue = 0.0;
    ProgressBar progressBar;

    std::unique_ptr<DeploymentThread> deploymentThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModelDeploymentWidget)
};
