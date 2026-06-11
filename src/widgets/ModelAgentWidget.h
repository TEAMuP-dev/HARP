/**
 * @file ModelAgentWidget.h
 * @brief GUI front-end for tools/model_agent, exposing the CLI subcommands.
 *
 * The agent itself stays a Python program (single source of truth). This widget
 * is a thin front-end: it builds a `python -m tools.model_agent <subcommand>`
 * invocation from a data-driven form, runs it in a background thread via a
 * temporary platform script, and streams stdout/stderr into a log view.
 */

#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../utils/Logging.h"

using namespace juce;

class ModelAgentWidget : public Component, private Timer
{
public:
    ModelAgentWidget() : progressBar(progressValue)
    {
        commands = makeCommandSpecs();

        titleLabel.setText("Model Agent", dontSendNotification);
        titleLabel.setFont(Font(FontOptions(22.0f, Font::bold)));
        addAndMakeVisible(titleLabel);

        setupLabel(commandLabel, "Command");
        for (size_t i = 0; i < commands.size(); ++i)
            commandSelector.addItem(commands[i].name, static_cast<int>(i) + 1);
        commandSelector.onChange = [this] { commandChanged(); };
        addAndMakeVisible(commandSelector);

        descriptionLabel.setColour(Label::textColourId, Colours::lightgrey);
        descriptionLabel.setJustificationType(Justification::centredLeft);
        addAndMakeVisible(descriptionLabel);

        setupLabel(pythonLabel, "Python");
        setupEditor(pythonEditor, defaultPythonExecutable());
        pythonEditor.setText(defaultPythonExecutable(), dontSendNotification);
        pythonEditor.onTextChange = [this] { refreshPreview(); };

        setupLabel(harpRootLabel, "HARP root");
        setupEditor(harpRootEditor, "Path to the HARP repo (contains tools/model_agent)");
        harpRootEditor.setText(detectHarpRoot().getFullPathName(), dontSendNotification);
        harpRootEditor.onTextChange = [this] { validateInputs(); };
        harpRootBrowse.setButtonText("Browse...");
        harpRootBrowse.onClick = [this] { browseInto(harpRootEditor, true); };
        addAndMakeVisible(harpRootBrowse);

        for (auto& row : rows)
        {
            row.label.setJustificationType(Justification::centredLeft);
            addAndMakeVisible(row.label);
            addChildComponent(row.editor);
            addChildComponent(row.toggle);
            addChildComponent(row.browse);
            row.editor.onTextChange = [this] { refreshPreview(); };
            row.toggle.onClick = [this] { refreshPreview(); };
        }

        for (size_t i = 0; i < rows.size(); ++i)
        {
            const size_t index = i;
            rows[i].browse.setButtonText("Browse...");
            rows[i].browse.onClick = [this, index] { browseForRow(index); };
        }

        previewLabel.setText("Command preview", dontSendNotification);
        previewLabel.setColour(Label::textColourId, Colours::grey);
        addAndMakeVisible(previewLabel);

        previewEditor.setMultiLine(true, true);
        previewEditor.setReadOnly(true);
        previewEditor.setCaretVisible(false);
        previewEditor.setFont(Font(FontOptions(13.0f)));
        previewEditor.setColour(TextEditor::backgroundColourId, Colours::black.withAlpha(0.25f));
        addAndMakeVisible(previewEditor);

        runButton.setButtonText("Run");
        runButton.onClick = [this] { runCommand(); };
        addAndMakeVisible(runButton);

        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this] { cancelCommand(); };
        cancelButton.setEnabled(false);
        addAndMakeVisible(cancelButton);

        clearButton.setButtonText("Clear Log");
        clearButton.onClick = [this] { logEditor.clear(); };
        addAndMakeVisible(clearButton);

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
        logEditor.setText("Command output will appear here.", dontSendNotification);
        addAndMakeVisible(logEditor);

        setSize(840, 760);

        commandSelector.setSelectedId(1, sendNotification);
    }

    ~ModelAgentWidget() override
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
        area.removeFromTop(8);

        auto commandRow = area.removeFromTop(28);
        commandLabel.setBounds(commandRow.removeFromLeft(150));
        commandRow.removeFromLeft(8);
        commandSelector.setBounds(commandRow.removeFromLeft(220));
        commandRow.removeFromLeft(12);
        descriptionLabel.setBounds(commandRow);
        area.removeFromTop(8);

        layoutField(area, pythonLabel, pythonEditor, nullptr);
        area.removeFromTop(6);
        layoutField(area, harpRootLabel, harpRootEditor, &harpRootBrowse);
        area.removeFromTop(10);

        for (auto& row : rows)
        {
            if (! row.active)
                continue;

            if (row.spec.field == Field::Flag)
            {
                auto fieldRow = area.removeFromTop(32);
                row.label.setBounds(fieldRow.removeFromLeft(150));
                fieldRow.removeFromLeft(8);
                row.toggle.setBounds(fieldRow.removeFromLeft(40));
                area.removeFromTop(6);
            }
            else
            {
                layoutField(area, row.label, row.editor,
                            row.spec.browse == Browse::None ? nullptr : &row.browse);
                area.removeFromTop(6);
            }
        }

        area.removeFromTop(6);
        previewLabel.setBounds(area.removeFromTop(18));
        previewEditor.setBounds(area.removeFromTop(48));

        area.removeFromTop(8);
        auto buttonRow = area.removeFromTop(32);
        runButton.setBounds(buttonRow.removeFromLeft(120));
        buttonRow.removeFromLeft(8);
        cancelButton.setBounds(buttonRow.removeFromLeft(120));
        buttonRow.removeFromLeft(8);
        clearButton.setBounds(buttonRow.removeFromLeft(120));

        area.removeFromTop(10);
        statusLabel.setBounds(area.removeFromTop(22));
        validationLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(4);
        progressBar.setBounds(area.removeFromTop(18));
        area.removeFromTop(10);
        logEditor.setBounds(area);
    }

private:
    /* ---- Command model ---- */

    enum class Field
    {
        Text,
        Int,
        Flag
    };

    enum class Browse
    {
        None,
        OpenFile,
        OpenDir
    };

    struct ArgSpec
    {
        String flag; // empty => positional argument
        String label;
        Field field = Field::Text;
        Browse browse = Browse::None;
        String defaultValue;
        String placeholder;
        bool required = false;
        bool multi = false; // positional may contain several space-separated values
    };

    struct CommandSpec
    {
        String name;
        String description;
        std::vector<ArgSpec> args;
        bool runsModelCode = false;
    };

    static std::vector<CommandSpec> makeCommandSpecs()
    {
        std::vector<CommandSpec> specs;

        specs.push_back(
            { "discover",
              "Search Hugging Face Spaces for candidate models.",
              { { "--query", "Query", Field::Text, Browse::None, "", "e.g. audio", false },
                { "--author", "Author/org", Field::Text, Browse::None, "", "e.g. teamup-tech", false },
                { "--limit", "Limit", Field::Int, Browse::None, "25", "max results", false },
                { "--output", "Output JSON", Field::Text, Browse::OpenFile, "", "optional file", false },
                { "--all", "Include non-Gradio", Field::Flag, Browse::None, "0", "", false } } });

        specs.push_back(
            { "probe",
              "Fetch HARP controls from one or more model endpoints.",
              { { "", "Model paths", Field::Text, Browse::None, "", "space-separated ids/urls", true, true },
                { "--output", "Output JSON", Field::Text, Browse::OpenFile, "", "optional file", false } } });

        specs.push_back(
            { "score-card",
              "Score a raw Hugging Face model-card JSON file.",
              { { "", "Card JSON", Field::Text, Browse::OpenFile, "", "path to card.json", true },
                { "--output", "Output JSON", Field::Text, Browse::OpenFile, "", "optional file", false } } });

        specs.push_back(
            { "render-app",
              "Render a starter pyharp app.py from a model card.",
              { { "", "Card JSON", Field::Text, Browse::OpenFile, "", "path to card.json", true },
                { "--output", "Output app.py", Field::Text, Browse::OpenFile, "", "optional file", false } } });

        specs.push_back(
            { "generate-package",
              "Write app.py, requirements, and manifest for a model card.",
              { { "", "Card JSON", Field::Text, Browse::OpenFile, "", "path to card.json", true },
                { "--output", "Output dir", Field::Text, Browse::OpenDir, "", "package folder", false },
                { "--smoke-test", "Smoke-test (runs code)", Field::Flag, Browse::None, "0", "", false } },
              true });

        specs.push_back(
            { "package-repo",
              "Fetch a HF model repo and write a HARP Space package.",
              { { "", "Repo id", Field::Text, Browse::None, "", "author/model-name", true },
                { "--output", "Output dir", Field::Text, Browse::OpenDir, "", "Space folder", false },
                { "--smoke-test", "Smoke-test (runs code)", Field::Flag, Browse::None, "0", "", false } },
              true });

        specs.push_back(
            { "package",
              "Package probed endpoints into reviewable folders.",
              { { "", "Model paths", Field::Text, Browse::None, "", "space-separated ids", false, true },
                { "--from-file", "From file", Field::Text, Browse::OpenFile, "", "discovery/probe JSON", false },
                { "--output", "Output dir", Field::Text, Browse::OpenDir, "", "packages folder", false },
                { "--no-space-metadata", "Skip Space metadata", Field::Flag, Browse::None, "0", "", false } } });

        specs.push_back(
            { "harvest",
              "Download app.py files from an author's Spaces (read-only).",
              { { "--author", "Author/org", Field::Text, Browse::None, "teamup-tech", "", false },
                { "--query", "Query", Field::Text, Browse::None, "", "optional", false },
                { "--limit", "Limit", Field::Int, Browse::None, "100", "", false },
                { "--filename", "Filename", Field::Text, Browse::None, "app.py", "", false },
                { "--output", "Output dir", Field::Text, Browse::OpenDir, "", "harvest folder", false } } });

        specs.push_back(
            { "analyze",
              "Statically analyze harvested app.py files (no execution).",
              { { "", "Path", Field::Text, Browse::OpenDir, "", "harvest dir or app.py", true },
                { "--filename", "Filename", Field::Text, Browse::None, "app.py", "", false },
                { "--summary-only", "Summary only", Field::Flag, Browse::None, "0", "", false },
                { "--output", "Output JSON", Field::Text, Browse::OpenFile, "", "optional file", false } } });

        specs.push_back(
            { "smoke-test",
              "Launch a generated package's app.py and verify HARP controls.",
              { { "", "Package dir", Field::Text, Browse::OpenDir, "", "generated package folder", true },
                { "--startup-timeout", "Startup timeout (s)", Field::Int, Browse::None, "180", "", false } },
              true });

        specs.push_back(
            { "scaffold-recipe",
              "Build a recipe skeleton from a harvested app.py (fills I/O, stubs inference).",
              { { "", "Harvested app.py", Field::Text, Browse::OpenFile, "", "path to app.py", true },
                { "--output", "Recipe JSON out", Field::Text, Browse::OpenFile, "", "recipe.json", false } } });

        specs.push_back(
            { "render-recipe",
              "Render a pyharp app.py from a recipe JSON.",
              { { "", "Recipe JSON", Field::Text, Browse::OpenFile, "", "recipe.json", true },
                { "--output", "Output app.py", Field::Text, Browse::OpenFile, "", "optional file", false } } });

        specs.push_back(
            { "generate-recipe",
              "Write app.py, requirements, and manifest from a recipe JSON.",
              { { "", "Recipe JSON", Field::Text, Browse::OpenFile, "", "recipe.json", true },
                { "--output", "Output dir", Field::Text, Browse::OpenDir, "", "package folder", false },
                { "--smoke-test", "Smoke-test (runs code)", Field::Flag, Browse::None, "0", "", false } },
              true });

        specs.push_back(
            { "generate-recipe-from-llm",
              "LLM-draft a recipe for any model (needs GEMINI/ANTHROPIC/OPENAI API key).",
              { { "--repo", "Repo id", Field::Text, Browse::None, "", "author/model (or use --card)", false },
                { "--card", "Card JSON", Field::Text, Browse::OpenFile, "", "model card json", false },
                { "--inputs", "Input types", Field::Text, Browse::None, "", "e.g. audio,slider", false },
                { "--outputs", "Output types", Field::Text, Browse::None, "", "e.g. audio,labels", false },
                { "--provider", "Provider", Field::Text, Browse::None, "", "gemini|anthropic|openai (auto)", false },
                { "--output", "Recipe JSON out", Field::Text, Browse::OpenFile, "", "recipe.json", false } } });

        specs.push_back(
            { "list-models",
              "List LLM models available for your configured provider/API key.",
              { { "--provider", "Provider", Field::Text, Browse::None, "", "gemini|anthropic|openai (auto)", false } } });

        specs.push_back(
            { "complete-recipe",
              "LLM-fill the _todo stubs of a scaffolded recipe (preserves I/O).",
              { { "", "Scaffold recipe", Field::Text, Browse::OpenFile, "", "recipe.json with _todo", true },
                { "--repo", "Repo id (context)", Field::Text, Browse::None, "", "optional, for README", false },
                { "--provider", "Provider", Field::Text, Browse::None, "", "gemini|anthropic|openai (auto)", false },
                { "--output", "Recipe JSON out", Field::Text, Browse::OpenFile, "", "completed.json", false },
                { "--generate-package", "Write package", Field::Flag, Browse::None, "0", "", false },
                { "--smoke-test", "Smoke-test (runs code)", Field::Flag, Browse::None, "0", "", false } },
              true });

        return specs;
    }

    struct ParamRow
    {
        Label label;
        TextEditor editor;
        ToggleButton toggle;
        TextButton browse;
        ArgSpec spec;
        bool active = false;
    };

    static constexpr int maxRows = 6;

    /* ---- Process thread ---- */

    class AgentThread : public Thread
    {
    public:
        AgentThread(StringArray commandToRun, File temporaryScript)
            : Thread("Model Agent"),
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
                appendLog("\nCommand canceled.\n");
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
        label.setBounds(row.removeFromLeft(150));
        row.removeFromLeft(8);
        if (browse != nullptr)
            browse->setBounds(row.removeFromRight(90));
        editor.setBounds(row);
    }

    const CommandSpec& currentCommand() const
    {
        const int index = jmax(0, commandSelector.getSelectedId() - 1);
        return commands[static_cast<size_t>(index)];
    }

    void commandChanged()
    {
        const auto& spec = currentCommand();
        descriptionLabel.setText(spec.description, dontSendNotification);

        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto& row = rows[i];
            const bool active = i < spec.args.size();
            row.active = active;

            if (! active)
            {
                row.label.setVisible(false);
                row.editor.setVisible(false);
                row.toggle.setVisible(false);
                row.browse.setVisible(false);
                continue;
            }

            row.spec = spec.args[i];
            row.label.setText(row.spec.label, dontSendNotification);
            row.label.setVisible(true);

            if (row.spec.field == Field::Flag)
            {
                row.editor.setVisible(false);
                row.browse.setVisible(false);
                row.toggle.setVisible(true);
                row.toggle.setToggleState(row.spec.defaultValue == "1", dontSendNotification);
            }
            else
            {
                row.toggle.setVisible(false);
                row.editor.setVisible(true);
                row.editor.setText(row.spec.defaultValue, dontSendNotification);
                row.editor.setTextToShowWhenEmpty(row.spec.placeholder, Colours::grey);
                row.browse.setVisible(row.spec.browse != Browse::None);
            }
        }

        resized();
        refreshPreview();
    }

    void browseForRow(size_t index)
    {
        if (index >= rows.size() || ! rows[index].active)
            return;

        const bool directory = rows[index].spec.browse == Browse::OpenDir;
        browseInto(rows[index].editor, directory);
    }

    void browseInto(TextEditor& editor, bool directories)
    {
        int flags = FileBrowserComponent::openMode;
        flags |= directories ? FileBrowserComponent::canSelectDirectories
                             : FileBrowserComponent::canSelectFiles;

        const File start = File::isAbsolutePath(editor.getText()) ? File(editor.getText())
                                                                   : File(harpRootEditor.getText());

        fileChooser = std::make_unique<FileChooser>("Select", start);
        fileChooser->launchAsync(flags,
                                 [this, &editor](const FileChooser& chooser)
                                 {
                                     const auto result = chooser.getResult();
                                     if (result != File())
                                     {
                                         editor.setText(result.getFullPathName(), dontSendNotification);
                                         validateInputs();
                                     }
                                 });
    }

    bool isRunning() const { return agentThread != nullptr && agentThread->isThreadRunning(); }

    StringArray collectArguments(String& error) const
    {
        StringArray positional;
        StringArray options;

        for (const auto& row : rows)
        {
            if (! row.active)
                continue;

            const auto& spec = row.spec;

            if (spec.field == Field::Flag)
            {
                if (row.toggle.getToggleState())
                    options.add(spec.flag);
                continue;
            }

            const auto value = row.editor.getText().trim();

            if (value.isEmpty())
            {
                if (spec.required && spec.flag.isEmpty())
                    error = "Missing required value: " + spec.label;
                continue;
            }

            if (spec.flag.isEmpty())
            {
                if (spec.multi)
                {
                    StringArray tokens;
                    tokens.addTokens(value, " ", "\"");
                    tokens.removeEmptyStrings();
                    positional.addArray(tokens);
                }
                else
                {
                    positional.add(value);
                }
            }
            else
            {
                options.add(spec.flag);
                options.add(value);
            }
        }

        StringArray combined;
        combined.addArray(positional);
        combined.addArray(options);
        return combined;
    }

    static String quoteArg(const String& value)
    {
#if JUCE_WINDOWS
        return "\"" + value.replace("\"", "\"\"") + "\"";
#else
        return "'" + value.replace("'", "'\"'\"'") + "'";
#endif
    }

    StringArray buildPythonTokens(String& error) const
    {
        const auto subcommandArgs = collectArguments(error);

        StringArray tokens;
        tokens.add(pythonEditor.getText().trim().isEmpty() ? defaultPythonExecutable()
                                                            : pythonEditor.getText().trim());
        tokens.add("-B");
        tokens.add("-m");
        tokens.add("tools.model_agent");
        tokens.add(currentCommand().name);
        tokens.addArray(subcommandArgs);
        return tokens;
    }

    String buildPreview(String& error) const
    {
        const auto tokens = buildPythonTokens(error);
        return tokens.joinIntoString(" ");
    }

    String buildScript(const StringArray& pythonTokens) const
    {
        String pythonLine;
        for (const auto& token : pythonTokens)
            pythonLine += quoteArg(token) + " ";
        pythonLine = pythonLine.trim();

        const auto root = harpRootEditor.getText().trim();

        String script;
#if JUCE_WINDOWS
        script << "@echo off\r\n";
        script << "cd /d " << quoteArg(root) << "\r\n";
        script << "set PYTHONDONTWRITEBYTECODE=1\r\n";
        script << pythonLine << "\r\n";
#else
        script << "cd " << quoteArg(root) << " || exit 1\n";
        script << "export PYTHONDONTWRITEBYTECODE=1\n";
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
        // Login shell so the user's PATH (e.g. conda/pyenv) is available.
        args.add("/bin/bash");
        args.add("-l");
        args.add(scriptFile.getFullPathName());
#endif
        return args;
    }

    void refreshPreview()
    {
        String error;
        previewEditor.setText(buildPreview(error), dontSendNotification);
        validateInputs();
    }

    void validateInputs()
    {
        const auto root = File(harpRootEditor.getText().trim());

        String message;
        bool valid = true;

        if (harpRootEditor.getText().trim().isEmpty()
            || ! root.getChildFile("tools/model_agent").isDirectory())
        {
            valid = false;
            message = "HARP root must contain tools/model_agent.";
        }
        else
        {
            String error;
            collectArguments(error);
            if (error.isNotEmpty())
            {
                valid = false;
                message = error;
            }
            else if (currentCommand().runsModelCode)
            {
                message = "Note: this command can execute downloaded model code.";
            }
            else
            {
                message = "Ready.";
            }
        }

        const bool warn = valid && currentCommand().runsModelCode;
        validationLabel.setText(message, dontSendNotification);
        validationLabel.setColour(Label::textColourId,
                                  valid ? (warn ? Colours::orange : Colours::lightgreen)
                                        : Colours::orange);
        runButton.setEnabled(valid && ! isRunning());
    }

    void runCommand()
    {
        validateInputs();
        if (! runButton.isEnabled())
            return;

        String error;
        const auto tokens = buildPythonTokens(error);
        if (error.isNotEmpty())
        {
            statusLabel.setText(error, dontSendNotification);
            return;
        }

        const auto script = buildScript(tokens);

#if JUCE_WINDOWS
        const String extension = ".bat";
#else
        const String extension = ".sh";
#endif
        auto scriptFile = File::getSpecialLocation(File::tempDirectory)
                              .getChildFile("harp_model_agent_"
                                            + String(Time::getMillisecondCounter()) + extension);
        scriptFile.replaceWithText(script);

        logEditor.clear();
        logEditor.setText("$ " + tokens.joinIntoString(" ") + "\n\n", dontSendNotification);
        statusLabel.setText("Running " + currentCommand().name + "...", dontSendNotification);
        progressValue = -1.0;
        runButton.setEnabled(false);
        cancelButton.setEnabled(true);

        agentThread = std::make_unique<AgentThread>(shellInvocation(scriptFile), scriptFile);
        agentThread->startThread();
        startTimerHz(10);
    }

    void cancelCommand()
    {
        if (agentThread != nullptr)
        {
            statusLabel.setText("Canceling...", dontSendNotification);
            agentThread->signalThreadShouldExit();
        }
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
            statusLabel.setText(succeeded ? currentCommand().name + " complete"
                                          : currentCommand().name + " failed",
                                dontSendNotification);
            progressValue = succeeded ? 1.0 : 0.0;
            agentThread.reset();
            stopTimer();
            cancelButton.setEnabled(false);
            validateInputs();
        }
    }

    /* ---- Members ---- */

    std::vector<CommandSpec> commands;

    Label titleLabel;
    Label commandLabel;
    Label descriptionLabel;
    Label pythonLabel;
    Label harpRootLabel;
    Label previewLabel;
    Label statusLabel;
    Label validationLabel;

    ComboBox commandSelector;
    TextEditor pythonEditor;
    TextEditor harpRootEditor;
    TextButton harpRootBrowse;

    std::array<ParamRow, maxRows> rows;

    TextEditor previewEditor;

    TextButton runButton;
    TextButton cancelButton;
    TextButton clearButton;

    TextEditor logEditor;

    double progressValue = 0.0;
    ProgressBar progressBar;

    std::unique_ptr<FileChooser> fileChooser;
    std::unique_ptr<AgentThread> agentThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModelAgentWidget)
};
