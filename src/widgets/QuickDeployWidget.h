/**
 * @file QuickDeployWidget.h
 * @brief One-shot "deploy a model" front-end for tools/model_agent.
 *
 * The full ModelAgentWidget exposes every CLI subcommand as a data-driven form.
 * Most of the time a user just wants to point at a model and publish it, so this
 * widget reduces the whole flow to two text boxes -- the model link and the
 * target org/Space -- plus a Deploy button. It maps directly onto:
 *
 *     python -m tools.model_agent deploy <link> --repo <target> --yes
 *
 * "Preview plan" runs the same command with `--plan` (analyze only, deploy
 * nothing). The agent stays a Python program (single source of truth); this
 * widget just builds the invocation, injects the saved API keys into the run
 * environment, runs it in a background thread, and streams stdout/stderr.
 */

#pragma once

#include <atomic>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../clients/Client.h"
#include "../utils/Enums.h"
#include "../utils/Logging.h"
#include "../utils/Settings.h"
#include "ModelAgentWidget.h"

using namespace juce;

class QuickDeployWidget : public Component, private Timer
{
public:
    QuickDeployWidget() : progressBar(progressValue)
    {
        titleLabel.setText("Deploy a Model", dontSendNotification);
        titleLabel.setFont(Font(FontOptions(22.0f, Font::bold)));
        addAndMakeVisible(titleLabel);

        helpLabel.setText(
            "Paste a model link and where to publish it, then click Deploy. "
            "The agent detects the source (GitHub repo, Hugging Face model, or "
            "Space), picks the deployment mode, and does the rest.",
            dontSendNotification);
        helpLabel.setColour(Label::textColourId, Colours::lightgrey);
        helpLabel.setJustificationType(Justification::topLeft);
        addAndMakeVisible(helpLabel);

        setupLabel(modelLabel, "Model link");
        setupEditor(modelEditor,
                    "https://github.com/org/model   or   org/model");
        modelEditor.onTextChange = [this] { validateInputs(); };

        setupLabel(targetLabel, "Deploy to");
        setupEditor(targetEditor,
                    "teamup-tech   (your org)   or   teamup-tech/model");
        targetEditor.onTextChange = [this] { validateInputs(); };

        targetHintLabel.setText(
            "Tip: enter just your org (e.g. teamup-tech) and the Space name is "
            "taken from the model link.",
            dontSendNotification);
        targetHintLabel.setColour(Label::textColourId, Colours::grey);
        targetHintLabel.setJustificationType(Justification::topLeft);
        addAndMakeVisible(targetHintLabel);

        userTokenToggle.setButtonText("Add a Hugging Face token field (remote proxy deploys)");
        userTokenToggle.setToggleState(true, dontSendNotification);
        addAndMakeVisible(userTokenToggle);

        // Advanced, auto-detected settings. Usually left untouched; kept visible
        // so a non-standard Python / repo location can still be corrected.
        setupLabel(pythonLabel, "Python");
        setupEditor(pythonEditor, defaultPythonExecutable());
        pythonEditor.setText(defaultPythonExecutable(), dontSendNotification);

        setupLabel(harpRootLabel, "HARP root");
        setupEditor(harpRootEditor, "Path to the HARP repo (contains tools/model_agent)");
        harpRootEditor.setText(detectHarpRoot().getFullPathName(), dontSendNotification);
        harpRootEditor.onTextChange = [this] { validateInputs(); };
        harpRootBrowse.setButtonText("Browse...");
        harpRootBrowse.onClick = [this] { browseForRoot(); };
        addAndMakeVisible(harpRootBrowse);

        keysNoteLabel.setText(
            "Uses the Hugging Face and Gemini keys saved under Settings -> API Keys.",
            dontSendNotification);
        keysNoteLabel.setColour(Label::textColourId, Colours::lightgrey);
        keysNoteLabel.setJustificationType(Justification::topLeft);
        addAndMakeVisible(keysNoteLabel);

        previewButton.setButtonText("Preview Plan");
        previewButton.onClick = [this] { startRun(true); };
        addAndMakeVisible(previewButton);

        deployButton.setButtonText("Deploy");
        deployButton.onClick = [this] { startRun(false); };
        addAndMakeVisible(deployButton);

        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this] { cancelRun(); };
        cancelButton.setEnabled(false);
        addAndMakeVisible(cancelButton);

        clearButton.setButtonText("Clear Log");
        clearButton.onClick = [this] { logEditor.clear(); };
        addAndMakeVisible(clearButton);

        advancedButton.setButtonText("All commands...");
        advancedButton.onClick = [this] { openAdvanced(); };
        addAndMakeVisible(advancedButton);

        statusLabel.setText("Ready", dontSendNotification);
        statusLabel.setFont(Font(FontOptions(15.0f, Font::bold)));
        addAndMakeVisible(statusLabel);

        validationLabel.setColour(Label::textColourId, Colours::lightgrey);
        addAndMakeVisible(validationLabel);

        progressValue = 0.0;
        addAndMakeVisible(progressBar);

        logEditor.setMultiLine(true);
        logEditor.setReadOnly(true);
        logEditor.setScrollbarsShown(true);
        logEditor.setCaretVisible(false);
        logEditor.setFont(Font(FontOptions(13.0f)));
        logEditor.setText("Deployment output will appear here.", dontSendNotification);
        addAndMakeVisible(logEditor);

        setSize(720, 640);
        validateInputs();
    }

    ~QuickDeployWidget() override
    {
        stopTimer();
        if (agentThread != nullptr)
        {
            agentThread->signalThreadShouldExit();
            agentThread->waitForThreadToExit(2000);
        }
    }

    void paint(Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14);
        titleLabel.setBounds(area.removeFromTop(32));
        area.removeFromTop(4);
        helpLabel.setBounds(area.removeFromTop(48));
        area.removeFromTop(8);

        layoutField(area, modelLabel, modelEditor, nullptr);
        area.removeFromTop(6);
        layoutField(area, targetLabel, targetEditor, nullptr);
        targetHintLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(4);
        userTokenToggle.setBounds(area.removeFromTop(24));
        area.removeFromTop(10);

        layoutField(area, pythonLabel, pythonEditor, nullptr);
        area.removeFromTop(6);
        layoutField(area, harpRootLabel, harpRootEditor, &harpRootBrowse);
        area.removeFromTop(4);
        keysNoteLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(10);

        auto buttonRow = area.removeFromTop(32);
        deployButton.setBounds(buttonRow.removeFromLeft(120));
        buttonRow.removeFromLeft(8);
        previewButton.setBounds(buttonRow.removeFromLeft(120));
        buttonRow.removeFromLeft(8);
        cancelButton.setBounds(buttonRow.removeFromLeft(100));
        buttonRow.removeFromLeft(8);
        clearButton.setBounds(buttonRow.removeFromLeft(100));
        advancedButton.setBounds(buttonRow.removeFromRight(120));

        area.removeFromTop(10);
        statusLabel.setBounds(area.removeFromTop(22));
        validationLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);
        progressBar.setBounds(area.removeFromTop(18));
        area.removeFromTop(10);
        logEditor.setBounds(area);
    }

private:
    /* ---- Process thread (streams a child process's stdout/stderr) ---- */

    class AgentThread : public Thread
    {
    public:
        AgentThread(StringArray commandToRun, File temporaryScript)
            : Thread("Quick Deploy"),
              command(std::move(commandToRun)),
              scriptFile(std::move(temporaryScript))
        {
        }

        ~AgentThread() override { scriptFile.deleteFile(); }

        void run() override
        {
            ChildProcess process;
            if (! process.start(command, ChildProcess::wantStdOut | ChildProcess::wantStdErr))
            {
                appendLog("Could not start the model-agent process. Is Python installed and on PATH?\n");
                exitCode = -1;
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
                appendLog("\nDeployment canceled.\n");
                exitCode = -1;
            }
            else
            {
                readAvailableOutput(process);
                exitCode = (int) process.getExitCode();
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

    private:
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

        StringArray command;
        File scriptFile;
        CriticalSection logLock;
        String pendingLog;
        std::atomic<bool> finished { false };
        std::atomic<int> exitCode { -1 };
    };

    /* ---- Helpers ---- */

    static String defaultPythonExecutable()
    {
#if JUCE_WINDOWS
        return "python";
#else
        return "python3";
#endif
    }

    static File detectHarpRoot()
    {
        Array<File> starts;
        starts.add(File::getCurrentWorkingDirectory());
        starts.add(File::getSpecialLocation(File::currentExecutableFile).getParentDirectory());

        for (auto start : starts)
        {
            File dir = start;
            for (int depth = 0; depth < 8 && dir.exists(); ++depth)
            {
                if (dir.getChildFile("tools/model_agent").isDirectory())
                    return dir;
                dir = dir.getParentDirectory();
            }
        }

        return File::getCurrentWorkingDirectory();
    }

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

    void layoutField(Rectangle<int>& area, Label& label, TextEditor& editor, TextButton* browse)
    {
        auto row = area.removeFromTop(34);
        label.setBounds(row.removeFromLeft(110));
        row.removeFromLeft(8);
        if (browse != nullptr)
            browse->setBounds(row.removeFromRight(90));
        editor.setBounds(row);
    }

    void browseForRoot()
    {
        const File start = File::isAbsolutePath(harpRootEditor.getText())
                               ? File(harpRootEditor.getText())
                               : File::getCurrentWorkingDirectory();
        fileChooser = std::make_unique<FileChooser>("Select the HARP repo folder", start);
        fileChooser->launchAsync(
            FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
            [this](const FileChooser& chooser)
            {
                const auto result = chooser.getResult();
                if (result != File())
                {
                    harpRootEditor.setText(result.getFullPathName(), dontSendNotification);
                    validateInputs();
                }
            });
    }

    bool isRunning() const { return agentThread != nullptr && agentThread->isThreadRunning(); }

    static String quoteArg(const String& value)
    {
#if JUCE_WINDOWS
        return "\"" + value.replace("\"", "\"\"") + "\"";
#else
        return "'" + value.replace("'", "'\"'\"'") + "'";
#endif
    }

    StringArray buildTokens(bool planOnly) const
    {
        StringArray tokens;
        tokens.add(pythonEditor.getText().trim().isEmpty() ? defaultPythonExecutable()
                                                            : pythonEditor.getText().trim());
        tokens.add("-B");
        tokens.add("-m");
        tokens.add("tools.model_agent");
        tokens.add("deploy");
        tokens.add(modelEditor.getText().trim());

        const auto target = targetEditor.getText().trim();
        if (target.isNotEmpty())
        {
            tokens.add("--repo");
            tokens.add(target);
        }

        // GUI can't answer the interactive y/N prompt: --plan only analyzes,
        // --yes deploys unattended.
        tokens.add(planOnly ? "--plan" : "--yes");

        if (! userTokenToggle.getToggleState())
            tokens.add("--no-user-token");

        return tokens;
    }

    // A stored key from the shared settings store (Settings -> API Keys),
    // falling back to the value persisted on disk.
    String storedKey(Provider p) const
    {
        if (sharedTokens->savedTokens.contains(p))
            return sharedTokens->savedTokens.at(p).trim();
        return Settings::getString("apikeys." + enumToString(p)).trim();
    }

    // Inject the saved Gemini + Hugging Face keys into the run environment (not
    // the visible command), so the LLM drafting and the Space push both work
    // without any terminal env setup. deploy defaults to the Gemini provider.
    void appendEnvExports(String& script) const
    {
        const String llmKey = storedKey(Provider::Gemini);
        const String hfToken = storedKey(Provider::HuggingFace);
#if JUCE_WINDOWS
        if (llmKey.isNotEmpty())
            script << "set \"GEMINI_API_KEY=" << llmKey << "\"\r\n";
        if (hfToken.isNotEmpty())
            script << "set \"HF_TOKEN=" << hfToken << "\"\r\n";
#else
        if (llmKey.isNotEmpty())
            script << "export GEMINI_API_KEY=" << quoteArg(llmKey) << "\n";
        if (hfToken.isNotEmpty())
            script << "export HF_TOKEN=" << quoteArg(hfToken) << "\n";
#endif
    }

    String buildScript(const StringArray& tokens) const
    {
        String pythonLine;
        for (const auto& token : tokens)
            pythonLine += quoteArg(token) + " ";
        pythonLine = pythonLine.trim();

        const auto root = harpRootEditor.getText().trim();

        String script;
#if JUCE_WINDOWS
        script << "@echo off\r\n";
        script << "cd /d " << quoteArg(root) << "\r\n";
        script << "set PYTHONDONTWRITEBYTECODE=1\r\n";
        appendEnvExports(script);
        script << pythonLine << "\r\n";
#else
        script << "cd " << quoteArg(root) << " || exit 1\n";
        script << "export PYTHONDONTWRITEBYTECODE=1\n";
        appendEnvExports(script);
        script << pythonLine << "\n";
#endif
        return script;
    }

    static StringArray shellInvocation(const File& scriptFile)
    {
        StringArray args;
#if JUCE_WINDOWS
        args.add("cmd.exe");
        args.add("/C");
        args.add(scriptFile.getFullPathName());
#else
        args.add("/bin/bash");
        args.add("-l");
        args.add(scriptFile.getFullPathName());
#endif
        return args;
    }

    // Returns "" when inputs are valid for the requested action.
    String validationError(bool planOnly) const
    {
        const auto root = File(harpRootEditor.getText().trim());
        if (harpRootEditor.getText().trim().isEmpty()
            || ! root.getChildFile("tools/model_agent").isDirectory())
            return "HARP root must contain tools/model_agent.";
        if (modelEditor.getText().trim().isEmpty())
            return "Enter a model link (GitHub repo, HF model, or HF Space).";
        if (! planOnly && targetEditor.getText().trim().isEmpty())
            return "Enter where to deploy (your org, or owner/space).";
        return {};
    }

    void validateInputs()
    {
        const auto deployError = validationError(false);
        const bool canDeploy = deployError.isEmpty() && ! isRunning();
        const bool canPreview = validationError(true).isEmpty() && ! isRunning();

        deployButton.setEnabled(canDeploy);
        previewButton.setEnabled(canPreview);

        if (isRunning())
            return;

        if (! deployError.isEmpty())
        {
            validationLabel.setText(deployError, dontSendNotification);
            validationLabel.setColour(Label::textColourId, Colours::orange);
        }
        else
        {
            validationLabel.setText("Ready to deploy.", dontSendNotification);
            validationLabel.setColour(Label::textColourId, Colours::lightgreen);
        }
    }

    void startRun(bool planOnly)
    {
        if (validationError(planOnly).isNotEmpty() || isRunning())
            return;

        const auto tokens = buildTokens(planOnly);
        const auto script = buildScript(tokens);

#if JUCE_WINDOWS
        const String extension = ".bat";
#else
        const String extension = ".sh";
#endif
        auto scriptFile = File::getSpecialLocation(File::tempDirectory)
                              .getChildFile("harp_quick_deploy_"
                                            + String(Time::getMillisecondCounter()) + extension);
        // Write the script verbatim. buildScript() already emits the correct
        // line endings per platform (\n for the bash .sh, \r\n for the .bat);
        // the default lineFeed ("\r\n") would rewrite the .sh to CRLF and bash
        // would then choke on the trailing \r (e.g. a final "--plan\r" flag
        // becomes an unrecognized argument), so pass nullptr to disable it.
        scriptFile.replaceWithText(script, false, false, nullptr);

        logEditor.clear();
        logEditor.setText("$ " + tokens.joinIntoString(" ") + "\n\n", dontSendNotification);
        statusLabel.setText(planOnly ? "Analyzing..." : "Deploying...", dontSendNotification);
        progressValue = -1.0;
        deployButton.setEnabled(false);
        previewButton.setEnabled(false);
        cancelButton.setEnabled(true);
        runningPlanOnly = planOnly;

        agentThread = std::make_unique<AgentThread>(shellInvocation(scriptFile), scriptFile);
        agentThread->startThread();
        startTimerHz(10);
    }

    void cancelRun()
    {
        if (agentThread != nullptr)
        {
            statusLabel.setText("Canceling...", dontSendNotification);
            agentThread->signalThreadShouldExit();
        }
    }

    void openAdvanced()
    {
        DialogWindow::LaunchOptions options;
        options.dialogTitle = "Model Agent (all commands)";
        options.dialogBackgroundColour = Colours::darkgrey;
        options.content.setOwned(new ModelAgentWidget());
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.escapeKeyTriggersCloseButton = true;
        options.launchAsync();
    }

    void timerCallback() override
    {
        if (agentThread == nullptr)
            return;

        const auto newLog = agentThread->consumeLog();
        if (newLog.isNotEmpty())
        {
            logEditor.moveCaretToEnd();
            logEditor.insertTextAtCaret(newLog);
        }

        if (agentThread->hasFinished())
        {
            agentThread->waitForThreadToExit(50);
            const bool succeeded = agentThread->getExitCode() == 0;
            const String what = runningPlanOnly ? "Plan" : "Deployment";
            statusLabel.setText(succeeded ? what + " complete" : what + " failed",
                                dontSendNotification);
            progressValue = succeeded ? 1.0 : 0.0;
            agentThread.reset();
            stopTimer();
            cancelButton.setEnabled(false);
            validateInputs();
        }
    }

    /* ---- Members ---- */

    Label titleLabel;
    Label helpLabel;
    Label modelLabel;
    Label targetLabel;
    Label targetHintLabel;
    Label pythonLabel;
    Label harpRootLabel;
    Label keysNoteLabel;
    Label statusLabel;
    Label validationLabel;

    TextEditor modelEditor;
    TextEditor targetEditor;
    TextEditor pythonEditor;
    TextEditor harpRootEditor;
    TextButton harpRootBrowse;

    ToggleButton userTokenToggle;

    TextButton previewButton;
    TextButton deployButton;
    TextButton cancelButton;
    TextButton clearButton;
    TextButton advancedButton;

    TextEditor logEditor;

    double progressValue = 0.0;
    ProgressBar progressBar;

    bool runningPlanOnly = false;

    SharedResourcePointer<SharedAPIKeys> sharedTokens;

    std::unique_ptr<FileChooser> fileChooser;
    std::unique_ptr<AgentThread> agentThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuickDeployWidget)
};
