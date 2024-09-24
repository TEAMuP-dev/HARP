#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "CtrlComponent.h"
#include "ThreadPoolJob.h"
#include "WebModel.h"

#include "gui/HoverHandler.h"
#include "gui/MultiButton.h"
#include "gui/StatusComponent.h"
#include "gui/TitledTextBox.h"

#include "gradio/GradioClient.h"

#include "HarpLogger.h"
#include "media/AudioDisplayComponent.h"
#include "media/MediaDisplayComponent.h"
#include "media/MidiDisplayComponent.h"

using namespace juce;

// this only calls the callback ONCE
class TimedCallback : public Timer
{
public:
    TimedCallback(std::function<void()> callback, int interval)
        : mCallback(callback), mInterval(interval)
    {
        startTimer(mInterval);
    }

    ~TimedCallback() override { stopTimer(); }

    void timerCallback() override
    {
        mCallback();
        stopTimer();
    }

private:
    std::function<void()> mCallback;
    int mInterval;
};

inline Colour getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour uiColour,
                                     Colour fallback = Colour(0xff4d4d4d)) noexcept
{
    if (auto* v4 = dynamic_cast<LookAndFeel_V4*>(&LookAndFeel::getDefaultLookAndFeel()))
        return v4->getCurrentColourScheme().getUIColour(uiColour);

    return fallback;
}

inline std::unique_ptr<OutputStream> makeOutputStream(const URL& url)
{
    if (const auto doc = AndroidDocument::fromDocument(url))
        return doc.createOutputStream();

#if ! JUCE_IOS
    if (url.isLocalFile())
        return url.getLocalFile().createOutputStream();
#endif

    return url.createOutputStream();
}

//this is the callback for the add new path popup alert
class CustomPathAlertCallback : public juce::ModalComponentManager::Callback
{
public:
    CustomPathAlertCallback(std::function<void(int)> const& callback) : userCallback(callback) {}

    void modalStateFinished(int result) override
    {
        if (userCallback != nullptr)
        {
            userCallback(result);
        }
    }

private:
    std::function<void(int)> userCallback;
};

//==============================================================================
class MainComponent : public Component,
#if (JUCE_ANDROID || JUCE_IOS)
                      private Button::Listener,
#endif
                      private ChangeListener,
                      public MenuBarModel,
                      public ApplicationCommandTarget
{
public:
    enum CommandIDs
    {
        open = 0x2000,
        save = 0x2001,
        saveAs = 0x2002,
        about = 0x2003,
        undo = 0x2005,
        redo = 0x2006
        // settings = 0x2004,
    };

    StringArray getMenuBarNames() override { return { "File" }; }

    // In mac, we want the "about" command to be in the application menu ("HARP" tab)
    // For now, this is not used, as the extra commands appear grayed out
    std::unique_ptr<PopupMenu> getMacExtraMenu()
    {
        auto menu = std::make_unique<PopupMenu>();
        menu->addCommandItem(&commandManager, CommandIDs::about);
        return menu;
    }

    PopupMenu getMenuForIndex([[maybe_unused]] int menuIndex, const String& menuName) override
    {
        PopupMenu menu;

        if (menuName == "File")
        {
            menu.addCommandItem(&commandManager, CommandIDs::open);
            menu.addCommandItem(&commandManager, CommandIDs::save);
            menu.addCommandItem(&commandManager, CommandIDs::saveAs);
            menu.addCommandItem(&commandManager, CommandIDs::undo);
            menu.addCommandItem(&commandManager, CommandIDs::redo);
            menu.addSeparator();
            // menu.addCommandItem (&commandManager, CommandIDs::settings);
            // menu.addSeparator();
            menu.addCommandItem(&commandManager, CommandIDs::about);
        }
        return menu;
    }
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override
    {
        DBG("menuItemSelected: " << menuItemID);
        DBG("topLevelMenuIndex: " << topLevelMenuIndex);
    }

    ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }

    // Fills the commands array with the commands that this component/target supports
    void getAllCommands(Array<CommandID>& commands) override
    {
        const CommandID ids[] = {
            CommandIDs::open, CommandIDs::save, CommandIDs::saveAs,
            CommandIDs::undo, CommandIDs::redo, CommandIDs::about,
        };
        commands.addArray(ids, numElementsInArray(ids));
    }

    // Gets the information about a specific command
    void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override
    {
        switch (commandID)
        {
            case CommandIDs::open:
                // The third argument here doesn't indicate the command position in the menu
                // it rather serves as a tag to categorize the command
                result.setInfo("Open...", "Opens a file", "File", 0);
                result.addDefaultKeypress('o', ModifierKeys::commandModifier);
                break;
            case CommandIDs::save:
                result.setInfo("Save", "Saves the current document", "File", 0);
                result.addDefaultKeypress('s', ModifierKeys::commandModifier);
                break;
            case CommandIDs::saveAs:
                result.setInfo(
                    "Save As...", "Saves the current document with a new name", "File", 0);
                result.addDefaultKeypress(
                    's', ModifierKeys::shiftModifier | ModifierKeys::commandModifier);
                break;
            case CommandIDs::undo:
                result.setInfo("Undo", "Undoes the most recent operation", "File", 0);
                result.addDefaultKeypress('z', ModifierKeys::commandModifier);
                break;
            case CommandIDs::redo:
                result.setInfo("Redo", "Redoes the most recent operation", "File", 0);
                result.addDefaultKeypress(
                    'z', ModifierKeys::shiftModifier | ModifierKeys::commandModifier);
                break;
            case CommandIDs::about:
                result.setInfo("About HARP", "Shows information about the application", "About", 0);
                break;
        }
    }

    // Callback for the save and saveAs commands
    bool perform(const InvocationInfo& info) override
    {
        switch (info.commandID)
        {
            case CommandIDs::save:
                DBG("Save command invoked");
                saveCallback();
                break;
            case CommandIDs::saveAs:
                DBG("Save As command invoked");
                saveAsCallback();
                break;
            case CommandIDs::open:
                DBG("Open command invoked");
                openFileChooser();
                break;
            case CommandIDs::undo:
                DBG("Undo command invoked");
                undoCallback();
                break;
            case CommandIDs::redo:
                DBG("Redo command invoked");
                redoCallback();
                break;
            case CommandIDs::about:
                DBG("About command invoked");
                showAboutDialog();
                // URL("https://harp-plugin.netlify.app/").launchInDefaultBrowser();
                // URL("https://github.com/TEAMuP-dev/harp").launchInDefaultBrowser();
                break;
            default:
                return false;
        }
        return true;
    }

    void showAboutDialog()
    {
        // Maybe create a new class for the about dialog
        auto* aboutComponent = new Component();
        aboutComponent->setSize(400, 300);

        // label for the about text
        auto* aboutText = new Label();
        aboutText->setText(String(APP_NAME) + "\nVersion: " + String(APP_VERSION) + "\n\n",
                           dontSendNotification);
        aboutText->setJustificationType(Justification::centred);
        aboutText->setSize(380, 100);

        // hyperlink buttons
        auto* modelGlossaryButton = new HyperlinkButton(
            "Model Glossary", URL("https://github.com/TEAMuP-dev/HARP#available-models"));
        modelGlossaryButton->setSize(380, 24);
        modelGlossaryButton->setTopLeftPosition(10, 110);
        modelGlossaryButton->setJustificationType(Justification::centred);
        modelGlossaryButton->setColour(HyperlinkButton::textColourId, Colours::blue);

        auto* visitWebpageButton =
            new HyperlinkButton("Visit HARP webpage", URL("https://harp-plugin.netlify.app/"));
        visitWebpageButton->setSize(380, 24);
        visitWebpageButton->setTopLeftPosition(10, 140);
        visitWebpageButton->setJustificationType(Justification::centred);
        visitWebpageButton->setColour(HyperlinkButton::textColourId, Colours::blue);

        auto* reportIssueButton = new HyperlinkButton(
            "Report an issue", URL("https://github.com/TEAMuP-dev/harp/issues"));
        reportIssueButton->setSize(380, 24);
        reportIssueButton->setTopLeftPosition(10, 170);
        reportIssueButton->setJustificationType(Justification::centred);
        reportIssueButton->setColour(HyperlinkButton::textColourId, Colours::blue);

        // label for the copyright
        auto* copyrightLabel = new Label();
        copyrightLabel->setText(String(APP_COPYRIGHT) + "\n\n", dontSendNotification);
        copyrightLabel->setJustificationType(Justification::centred);
        copyrightLabel->setSize(380, 100);
        copyrightLabel->setTopLeftPosition(10, 200);

        // Add components to the main component
        aboutComponent->addAndMakeVisible(aboutText);
        aboutComponent->addAndMakeVisible(modelGlossaryButton);
        aboutComponent->addAndMakeVisible(visitWebpageButton);
        aboutComponent->addAndMakeVisible(reportIssueButton);
        aboutComponent->addAndMakeVisible(copyrightLabel);

        // The dialog window with the custom component as its content
        DialogWindow::LaunchOptions dialog;
        dialog.content.setOwned(aboutComponent);
        dialog.dialogTitle = "About " + String(APP_NAME);
        dialog.dialogBackgroundColour = Colours::grey;
        dialog.escapeKeyTriggersCloseButton = true;
        dialog.useNativeTitleBar = true;
        dialog.resizable = false;

        dialog.launchAsync();
    }

    void saveCallback()
    {
        if (saveEnabled)
        {
            DBG("HARPProcessorEditor::buttonClicked save button listener activated");
            mediaDisplay->overwriteTarget();

            saveEnabled = false;
            setStatus("File saved successfully");
        }
        else
        {
            DBG("save button is disabled");
            setStatus("Nothing to save");
        }
    }

    void saveAsCallback()
    {
        if (mediaDisplay->isFileLoaded())
        {
            StringArray validExtensions = mediaDisplay->getInstanceExtensions();
            String filePatternsAllowed = "*" + validExtensions.joinIntoString(";*");
            saveFileBrowser = std::make_unique<FileChooser>(
                "Select a media file...", File(), filePatternsAllowed);
            // Launch the file chooser dialog asynchronously
            saveFileBrowser->launchAsync(
                FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles,
                [this](const FileChooser& browser)
                {
                    StringArray validExtensions = mediaDisplay->getInstanceExtensions();
                    File newFile = browser.getResult();
                    if (newFile != File {})
                    {
                        if (newFile.getFileExtension().compare("") == 0)
                        {
                            newFile = newFile.withFileExtension(validExtensions[0]);
                        }
                        if (validExtensions.contains(newFile.getFileExtension()))
                        {
                            URL tempFilePath = mediaDisplay->getTempFilePath();

                            // Attempt to save the file to the new location
                            bool saveSuccessful = tempFilePath.getLocalFile().copyFileTo(newFile);
                            if (saveSuccessful)
                            {
                                // Inform the user of success
                                AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon,
                                                                 "Save As",
                                                                 "File successfully saved as:\n"
                                                                     + newFile.getFullPathName(),
                                                                 "OK");

                                // Update any necessary internal state
                                // currentAudioFile = AudioFile(newFile); // Assuming a wrapper, adjust accordingly
                                DBG("File successfully saved as " << newFile.getFullPathName());
                                loadMediaDisplay(newFile);
                            }
                            else
                            {
                                // Inform the user of failure
                                AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                                                 "Save As Failed",
                                                                 "Failed to save file as:\n"
                                                                     + newFile.getFullPathName(),
                                                                 "OK");
                                DBG("Failed to save file as " << newFile.getFullPathName());
                            }
                        }
                        else
                        {
                            // Inform the user of failure
                            AlertWindow::showMessageBoxAsync(
                                AlertWindow::WarningIcon,
                                "Save As Failed",
                                "Can't save file with extension " + newFile.getFileExtension()
                                    + " \n Valid extensions are: "
                                    + validExtensions.joinIntoString(";"),
                                "OK");
                        }
                    }
                    else
                    {
                        DBG("Save As operation was cancelled by the user.");
                    }
                });
        }
        else
        {
            setStatus("Nothing to save. Please load an audio file first.");
        }
    }

    void undoCallback()
    {
        DBG("Undoing last edit");

        // check if the audio file is loaded
        if (! mediaDisplay->isFileLoaded())
        {
            // TODO - gray out undo option in this case?
            // Fail with beep, we should just ignore this if it doesn't make sense
            DBG("No file loaded to perform operation on");
            juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
            return;
        }

        if (isProcessing)
        {
            DBG("Can't undo while processing occurring!");
            juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
            return;
        }

        if (! mediaDisplay->iteratePreviousTempFile())
        {
            DBG("Nothing to undo!");
            juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
        }
        else
        {
            saveEnabled = true;
            DBG("Undo callback completed successfully");
        }
    }

    void redoCallback()
    {
        DBG("Redoing last edit");

        // check if the audio file is loaded
        if (! mediaDisplay->isFileLoaded())
        {
            // TODO - gray out undo option in this case?
            // Fail with beep, we should just ignore this if it doesn't make sense
            DBG("No file loaded to perform operation on");
            juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
            return;
        }

        if (isProcessing)
        {
            DBG("Can't redo while processing occurring!");
            juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
            return;
        }

        if (! mediaDisplay->iterateNextTempFile())
        {
            DBG("Nothing to redo!");
            juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
        }
        else
        {
            saveEnabled = true;
            DBG("Redo callback completed successfully");
        }
    }

    void loadModelCallback()
    {
        DBG("HARPProcessorEditor::buttonClicked load model button listener activated");

        // collect input parameters for the model.

        const std::string hf_url = "https://huggingface.co/spaces/";

        std::string path_url;
        if (modelPathComboBox.getSelectedItemIndex() == 0)
            path_url = customPath;
        else
            path_url = modelPathComboBox.getText().toStdString();

        std::map<std::string, std::any> params = {
            { "url", path_url },
        };
        resetUI();
        // loading happens asynchronously.
        // the document controller trigger a change listener callback, which will update the UI

        threadPool.addJob(
            [this, params]
            {
                DBG("executeLoad!!");
                try
                {
                    // timeout after 10 seconds
                    // TODO: this callback needs to be cleaned up in the destructor in case we quit
                    std::atomic<bool> success = false;
                    // TimedCallback timedCallback(
                    //     [this, &success]
                    //     {
                    //         if (success)
                    //             return;
                    //         DBG("TIMED-CALLBACK: buttonClicked timedCallback listener activated");
                    //         AlertWindow::showMessageBoxAsync(
                    //             AlertWindow::WarningIcon,
                    //             "Loading Error",
                    //             "An error occurred while loading the WebModel: TIMED OUT! Please check that the space is awake.");
                    //         MessageManager::callAsync([this] { resetModelPathComboBox(); });
                    //         model.reset(new WebModel());
                    //         loadBroadcaster.sendChangeMessage();
                    //         // saveButton.setEnabled(false);
                    //         saveEnabled = false;
                    //     },
                    //     10000);
                    juce::String loadingError;
                    model->load(params, loadingError);
                    if (! loadingError.isEmpty())
                    {
                        LogAndDBG("Error: " + loadingError);
                        throw std::runtime_error(loadingError.toStdString());
                    }
                    success = true;
                    MessageManager::callAsync(
                        [this]
                        {
                            if (modelPathComboBox.getSelectedItemIndex() == 0)
                            {
                                bool alreadyInComboBox = false;

                                for (int i = 1; i <= modelPathComboBox.getNumItems(); ++i)
                                {
                                    if (modelPathComboBox.getItemText(i) == (String) customPath)
                                    {
                                        alreadyInComboBox = true;
                                        modelPathComboBox.setSelectedId(i + 1);
                                    }
                                }

                                if (! alreadyInComboBox)
                                {
                                    int new_id = modelPathComboBox.getNumItems() + 1;
                                    modelPathComboBox.addItem(customPath, new_id);
                                    modelPathComboBox.setSelectedId(new_id);
                                }
                            }
                        });
                    DBG("executeLoad done!!");
                    loadBroadcaster.sendChangeMessage();
                    // since we're on a helper thread,
                    // it's ok to sleep for 10s
                    // to let the timeout callback do its thing
                    //Thread::sleep(10000);
                    //Ryan: I commented this out because when the model succesfully loads but you close within 10 seconds it throws a error
                }
                catch (const std::runtime_error& e)
                {
                    DBG("Caught exception: " << e.what());

                    auto msgOpts =
                        MessageBoxOptions()
                            .withTitle("Loading Error")
                            .withIconType(AlertWindow::WarningIcon)
                            .withTitle("Error")
                            .withMessage("An error occurred while loading the WebModel: \n"
                                         + String(e.what()));
                    if (! String(e.what()).contains("404")
                        && ! String(e.what()).contains("Invalid URL"))
                    {
                        msgOpts = msgOpts.withButton("Open Space URL");
                    }

                    msgOpts = msgOpts.withButton("Open HARP Logs").withButton("Ok");
                    auto alertCallback = [this, msgOpts](int result)
                    {
                        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                        // NOTE (hugo): there's something weird about the button indices assigned by the msgOpts here
                        // DBG("ALERT-CALLBACK: buttonClicked alertCallback listener activated: chosen: " << chosen);
                        // auto chosen = msgOpts.getButtonText(result);
                        // they're not the same as the order of the buttons in the alert
                        // this is the order that I actually observed them to be.
                        // UPDATE (xribene): This should be fixed in Juce v8
                        // see: https://forum.juce.com/t/wrong-callback-value-for-alertwindow-showokcancelbox/55671/2
                        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                        std::map<int, std::string> observedButtonIndicesMap = {};
                        if (msgOpts.getNumButtons() == 3)
                        {
                            observedButtonIndicesMap.insert(
                                { 1, "Open Space URL" }); // should actually be 0 right?
                        }
                        observedButtonIndicesMap.insert(
                            { msgOpts.getNumButtons() - 1,
                              "Open HARP Logs" }); // should actually be 1
                        observedButtonIndicesMap.insert({ 0, "Ok" }); // should be 2

                        auto chosen = observedButtonIndicesMap[result];

                        // auto chosen = msgOpts.getButtonText();
                        if (chosen == "Open HARP Logs")
                        {
                            // logger->getLogFile().revealToUser();
                            HarpLogger::getInstance().getLogFile().revealToUser();
                        }
                        else if (chosen == "Open Space URL")
                        {
                            // URL spaceUrl = GradioClient::parseSpaceAddress(modelPathComboBox.getText().toStdString()).huggingface;
                            // URL spaceUrl = model->gradioClient->getSpaceUrl()
                            // URL spaceUrl = model->getGradioClient().getSpaceInfo().huggingface;
                            URL spaceUrl = this->model->getGradioClient().getSpaceInfo().huggingface;
                            spaceUrl.launchInDefaultBrowser();
                        }
                        MessageManager::callAsync([this] 
                        { 
                            resetModelPathComboBox(); 
                            model.reset(new WebModel());
                            loadBroadcaster.sendChangeMessage();
                            // saveButton.setEnabled(false);
                        });
                    };

                    AlertWindow::showAsync(msgOpts, alertCallback);
                    saveEnabled = false;
                }
            });

        // disable the load button until the model is loaded
        loadModelButton.setEnabled(false);
        modelPathComboBox.setEnabled(false);
        loadModelButton.setButtonText("loading...");

        // disable the process button until the model is loaded
        processCancelButton.setEnabled(false);

        // set the descriptionLabel to "loading {url}..."
        // TODO: we need to get rid of the params map, and just pass the url around instead
        // since it looks like we're sticking to webmodels.
        String url = String(std::any_cast<std::string>(params.at("url")));
        descriptionLabel.setText(
            "loading " + url
                + "...\n if this takes a while, check if the huggingface space is sleeping by visiting the space url below. Once the huggingface space is awake, try again.",
            dontSendNotification);
    }

    void resetModelPathComboBox()
    {
        resetUI();
        //should I clear this?
        spaceUrlButton.setButtonText("");
        int numItems = modelPathComboBox.getNumItems();
        std::vector<std::string> options;

        for (int i = 1; i <= numItems; ++i) // item indexes are 1-based in JUCE
        {
            String itemText = modelPathComboBox.getItemText(i - 1);
            options.push_back(itemText.toStdString());
            DBG("Item " << i << ": " << itemText);
        }

        modelPathComboBox.clear();

        modelPathComboBox.setTextWhenNothingSelected("choose a model");
        for (auto i = 0u; i < options.size(); ++i)
        {
            modelPathComboBox.addItem(options[i], static_cast<int>(i) + 1);
        }
    }

    void focusCallback()
    {
        if (mediaDisplay->isFileLoaded())
        {
            Time lastModTime =
                mediaDisplay->getTargetFilePath().getLocalFile().getLastModificationTime();
            if (lastModTime > lastLoadTime)
            {
                // Create an AlertWindow
                auto* reloadCheckWindow = new AlertWindow(
                    "File has been modified",
                    "The loaded file has been modified in a different editor! Would you like HARP to load the new version of the file?\nWARNING: This will clear the undo log and cause all unsaved edits to be lost!",
                    AlertWindow::QuestionIcon);

                reloadCheckWindow->addButton("Yes", 1, KeyPress(KeyPress::returnKey));
                reloadCheckWindow->addButton("No", 0, KeyPress(KeyPress::escapeKey));

                // Show the window and handle the result asynchronously
                reloadCheckWindow->enterModalState(
                    true,
                    new CustomPathAlertCallback(
                        [this, reloadCheckWindow](int result)
                        {
                            if (result == 1)
                            { // Yes was clicked
                                DBG("Reloading file");
                                loadMediaDisplay(mediaDisplay->getTargetFilePath().getLocalFile());
                            }
                            else
                            { // No was clicked or the window was closed
                                DBG("Not reloading file");
                                lastLoadTime =
                                    Time::getCurrentTime(); //Reset time so we stop asking
                            }
                            delete reloadCheckWindow;
                        }),
                    true);
            }
        }
    }

    explicit MainComponent(const URL& initialFilePath = URL())
        : jobsFinished(0),
          totalJobs(0),
          jobProcessorThread(customJobs, jobsFinished, totalJobs, processBroadcaster)
    {
        // logger.reset(juce::FileLogger::createDefaultAppLogger("HARP", "harp.log", "hello, harp!"));
        HarpLogger::getInstance().initializeLogger();

        addAndMakeVisible(chooseFileButton);
        chooseFileButton.onClick = [this] { openFileChooser(); };
        chooseFileButtonHandler.onMouseEnter = [this]()
        { setInstructions("Click to choose an audio file"); };
        chooseFileButtonHandler.onMouseExit = [this]() { clearInstructions(); };
        chooseFileButtonHandler.attach();

        addAndMakeVisible(saveFileButton);
        saveFileButton.onClick = [this] { saveCallback(); };
        saveFileButtonHandler.onMouseEnter = [this]()
        { setInstructions("Click to save results to original audio file"); };
        saveFileButtonHandler.onMouseExit = [this]() { clearInstructions(); };
        saveFileButtonHandler.attach();

        // Initialize default media display
        initializeMediaDisplay();

        if (initialFilePath.isLocalFile())
        {
            // TODO - it seems command line args are handled through Main.cpp and this is never hit
            // Load initial file into matching media display
            loadMediaDisplay(initialFilePath.getLocalFile());
        }

        // addAndMakeVisible (startStopButton);
        playStopButton.addMode(playButtonInfo);
        playStopButton.addMode(stopButtonInfo);
        playStopButton.setMode(playButtonInfo.label);
        playStopButton.setEnabled(false);
        addAndMakeVisible(playStopButton);
        playStopButton.onMouseEnter = [this]
        {
            if (playStopButton.getModeName() == playButtonInfo.label)
                setInstructions("Click to start playback");
            else if (playStopButton.getModeName() == stopButtonInfo.label)
                setInstructions("Click to stop playback");
        };
        playStopButton.onMouseExit = [this] { clearInstructions(); };

        // initialize HARP UI
        // TODO: what happens if the model is nullptr rn?
        if (model == nullptr)
        {
            DBG("FATAL HARPProcessorEditor::HARPProcessorEditor: model is null");
            jassertfalse;
            return;
        }

        // Set setWantsKeyboardFocus to true for this component
        // Doing that, everytime we click outside the modelPathTextBox,
        // the focus will be taken away from the modelPathTextBox
        setWantsKeyboardFocus(true);

        // init the menu bar
        menuBar.reset(new MenuBarComponent(this));
        addAndMakeVisible(menuBar.get());
        setApplicationCommandManagerToWatch(&commandManager);
        // Register commands
        commandManager.registerAllCommandsForTarget(this);
        // commandManager.setFirstCommandTarget(this);
        addKeyListener(commandManager.getKeyMappings());

#if JUCE_MAC
        // Not used for now
        // auto extraMenu = getMacExtraMenu();
        MenuBarModel::setMacMainMenu(this);
#endif

        menuBar->setVisible(true);
        menuItemsChanged();

        // The Process/Cancel button
        processCancelButton.addMode(processButtonInfo);
        processCancelButton.addMode(cancelButtonInfo);
        processCancelButton.setMode(processButtonInfo.label);
        processCancelButton.setEnabled(false);
        addAndMakeVisible(processCancelButton);
        processCancelButton.onMouseEnter = [this]
        {
            if (processCancelButton.getModeName() == processButtonInfo.label)
                setInstructions("Click to send the audio file for processing");
            else if (processCancelButton.getModeName() == cancelButtonInfo.label)
                setInstructions("Click to cancel the processing");
        };
        processCancelButton.onMouseExit = [this]
        {
            // processCancelButton.setColour (TextButton::buttonColourId, getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::buttonOnColour));
            clearInstructions();
        };

        processBroadcaster.addChangeListener(this);
        saveEnabled = false;

        loadModelButton.addMode(loadButtonInfo);
        loadModelButton.setMode(loadButtonInfo.label);
        // loadModelButton.setButtonText("load");
        addAndMakeVisible(loadModelButton);
        loadModelButton.onMouseEnter = [this]
        { setInstructions("Loads the model and populates the UI with the model's parameters"); };
        loadModelButton.onMouseExit = [this] { clearInstructions(); };

        loadBroadcaster.addChangeListener(this);

        juce::String currentStatus = model->getStatus();
        if (currentStatus == "Status.LOADED" || currentStatus == "Status.FINISHED")
        {
            processCancelButton.setEnabled(true);
            processCancelButton.setMode(processButtonInfo.label);
        }
        else if (currentStatus == "Status.PROCESSING" || currentStatus == "Status.STARTING"
                 || currentStatus == "Status.SENDING")
        {
            processCancelButton.setEnabled(true);
            processCancelButton.setMode(cancelButtonInfo.label);
        }

        setStatus(currentStatus);

        // add a status timer to update the status label periodically
        mModelStatusTimer = std::make_unique<ModelStatusTimer>(model);
        mModelStatusTimer->addChangeListener(this);
        mModelStatusTimer->startTimer(100); // 100 ms interval

        // model path textbox
        std::vector<std::string> modelPaths = {
            "custom path...",
            // "hugggof/vampnet-music",
            // "cwitkowitz/timbre-trap",
            // "hugggof/vampnet-percussion",
            // "hugggof/vampnet-n64",
            // "hugggof/vampnet-choir",
            // "hugggof/vampnet-opera",
            // "hugggof/vampnet-machines",
            // "hugggof/vampnet-birds",
            // "descript/vampnet",
            // "pharoAIsanders420/micro-musicgen-jungle",
            // "hugggof/nesquik",
            // "hugggof/pitch_shifter",
            // "hugggof/harmonic_percussive",
            "xribene/pitch_shifter",
            "http://localhost:7860",
            "https://xribene-midi-pitch-shifter.hf.space/",
            "https://huggingface.co/spaces/xribene/midi_pitch_shifter",
            "xribene/midi_pitch_shifter",
        };

        modelPathComboBox.setTextWhenNothingSelected("choose a model");
        for (auto i = 0u; i < modelPaths.size(); ++i)
        {
            modelPathComboBox.addItem(modelPaths[i], static_cast<int>(i) + 1);
        }
        modelPathComboBoxHandler.onMouseEnter = [this]()
        {
            setInstructions(
                "A drop-down menu with some available models. Any new model you add will automatically be added to the list");
        };
        modelPathComboBoxHandler.onMouseExit = [this]() { clearInstructions(); };
        modelPathComboBoxHandler.attach();

        // Usage within your existing onChange handler
        modelPathComboBox.onChange = [this]
        {
            // Check if the 'custom path...' option is selected
            if (modelPathComboBox.getSelectedItemIndex() == 0)
            {
                // Create an AlertWindow
                auto* customPathWindow =
                    new AlertWindow("Enter Custom Path",
                                    "Please enter the path to the gradio endpoint:",
                                    AlertWindow::NoIcon);

                customPathWindow->addTextEditor("customPath", "", "Path:");
                customPathWindow->addButton("Load", 1, KeyPress(KeyPress::returnKey));
                customPathWindow->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));

                // Show the window and handle the result asynchronously
                customPathWindow->enterModalState(
                    true,
                    new CustomPathAlertCallback(
                        [this, customPathWindow](int result)
                        {
                            if (result == 1)
                            { // OK was clicked
                                // Retrieve the entered path
                                customPath = customPathWindow->getTextEditor("customPath")
                                                 ->getText()
                                                 .toStdString();
                                // Use the custom path as needed
                                DBG("Custom path entered: " + customPath);
                                loadModelButton.triggerClick();
                            }
                            else
                            { // Cancel was clicked or the window was closed
                                DBG("Custom path entry was canceled.");
                                resetModelPathComboBox();
                            }
                            delete customPathWindow;
                        }),
                    true);
            }
        };

        addAndMakeVisible(modelPathComboBox);

        // model controls
        ctrlComponent.setModel(model);
        addAndMakeVisible(ctrlComponent);
        ctrlComponent.populateGui();

        addAndMakeVisible(nameLabel);
        addAndMakeVisible(authorLabel);
        addAndMakeVisible(descriptionLabel);
        addAndMakeVisible(tagsLabel);
        addAndMakeVisible(audioOrMidiLabel);

        addAndMakeVisible(statusArea);
        addAndMakeVisible(instructionsArea);
        // model card component
        // Get the modelCard from the EditorView
        auto& card = model->card();
        setModelCard(card);

        jobProcessorThread.startThread();

        
        // ARA requires that plugin editors are resizable to support tight integration
        // into the host UI
        setOpaque(true);
        setSize(800, 800);
        resized();
    }

    ~MainComponent() override
    {
        mediaDisplay->removeChangeListener(this);

        // remove listeners
        mModelStatusTimer->removeChangeListener(this);
        loadBroadcaster.removeChangeListener(this);
        processBroadcaster.removeChangeListener(this);

        jobProcessorThread.signalThreadShouldExit();
        // This will not actually run any processing task
        // It'll just make sure that the thread is not waiting
        // and it'll allow it to check for the threadShouldExit flag
        jobProcessorThread.signalTask();
        jobProcessorThread.waitForThreadToExit(-1);

#if JUCE_MAC
        MenuBarModel::setMacMainMenu(nullptr);
#endif
        // commandManager.setFirstCommandTarget (nullptr);
    }

    void cancelCallback()
    {
        DBG("HARPProcessorEditor::buttonClicked cancel button listener activated");
        juce::String cancelError;
        model->cancel(cancelError);
        if (! cancelError.isEmpty())
        {
            LogAndDBG(cancelError.toStdString());
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                             "Cancel Error",
                                             "An error occurred while cancelling the processing: \n"
                                                 + cancelError);
            // processCancelButton.setEnabled(true);
            // processCancelButton.setMode(processButtonInfo.label);
            resetProcessingButtons();
            return;
        }
        // We already added a temp file, so we need to undo that
        mediaDisplay->iteratePreviousTempFile();
        mediaDisplay->clearFutureTempFiles();
        processCancelButton.setEnabled(false);
    }

    void processCallback()
    {
        DBG("HARPProcessorEditor::buttonClicked button listener activated");

        // check if the audio file is loaded
        if (! mediaDisplay->isFileLoaded())
        {
            // AlertWindow("Error", "Audio file is not loaded. Please load an audio file first.", AlertWindow::WarningIcon);
            //ShowMEssageBoxAsync
            AlertWindow::showMessageBoxAsync(
                AlertWindow::WarningIcon,
                "Error",
                "Audio file is not loaded. Please load an audio file first.");
            return;
        }

        processCancelButton.setEnabled(true);
        processCancelButton.setMode(cancelButtonInfo.label);

        saveEnabled = false;
        isProcessing = true;

        if (model == nullptr)
        {
            AlertWindow("Error",
                        "Model is not loaded. Please load a model first.",
                        AlertWindow::WarningIcon);
            isProcessing = false;
            return;
        }

        bool matchingModel = true;

        if (dynamic_cast<AudioDisplayComponent*>(mediaDisplay.get()))
        {
            matchingModel = ! model->card().midi_in && ! model->card().midi_out;
        }
        else
        {
            matchingModel = model->card().midi_in && model->card().midi_out;
        }
        // Check if the model's type (Audio or MIDI) matches the input file's type
        // If not, show an error message and ask the user to either use another model
        // or another appropriate file
        if (! matchingModel)
        {
            LogAndDBG("Model and file type mismatch");
            AlertWindow::showMessageBoxAsync(
                AlertWindow::WarningIcon,
                "Processing Error",
                "Model and file type mismatch. Please use an appropriate model or file.");
            // processBroadcaster.sendChangeMessage();
            resetProcessingButtons();
            return;
        }

        mediaDisplay->addNewTempFile();

        // print how many jobs are currently in the threadpool
        LogAndDBG("threadPool.getNumJobs: " + std::to_string(threadPool.getNumJobs()));

        // empty customJobs
        customJobs.clear();

        customJobs.push_back(new CustomThreadPoolJob([this] { // &jobsFinished, totalJobs
            // Individual job code for each iteration
            // copy the audio file, with the same filename except for an added _harp to the stem
            juce::String processingError;
            model->process(mediaDisplay->getTempFilePath().getLocalFile(), processingError);
            if (! processingError.isEmpty())
            {
                LogAndDBG(processingError.toStdString());
                AlertWindow::showMessageBoxAsync(
                    AlertWindow::WarningIcon,
                    "Processing Error",
                    "An error occurred while processing the audio file: \n" + processingError);
                resetProcessingButtons();
                return;
            }
            // load the audio file again
            processBroadcaster.sendChangeMessage();

        }));

        // Now the customJobs are ready to be added to be run in the threadPool
        jobProcessorThread.signalTask();
    }

    void initializeMediaDisplay(int mediaType = 0)
    {
        if (mediaType == 1)
        {
            mediaDisplay = std::make_unique<MidiDisplayComponent>();
        }
        else
        {
            // Default to audio display
            mediaDisplay = std::make_unique<AudioDisplayComponent>();
        }

        addAndMakeVisible(mediaDisplay.get());
        mediaDisplay->addChangeListener(this);

        mediaDisplayHandler = std::make_unique<HoverHandler>(*mediaDisplay);
        mediaDisplayHandler->onMouseEnter = [this]()
        { setInstructions(mediaDisplay->getMediaHandlerInstructions()); };
        mediaDisplayHandler->onMouseExit = [this]() { clearInstructions(); };
        mediaDisplayHandler->attach();
    }

    void loadMediaDisplay(File mediaFile)
    {
        // Check the file extension to determine type
        String extension = mediaFile.getFileExtension();

        bool matchingDisplay = true;

        if (dynamic_cast<AudioDisplayComponent*>(mediaDisplay.get()))
        {
            matchingDisplay = audioExtensions.contains(extension);
        }
        else
        {
            matchingDisplay = midiExtensions.contains(extension);
        }

        if (! matchingDisplay)
        {
            // Remove the existing media display
            removeChildComponent(mediaDisplay.get());
            mediaDisplay->removeChangeListener(this);
            mediaDisplayHandler->detach();

            int mediaType = 0;

            if (audioExtensions.contains(extension))
            {
            }
            else if (midiExtensions.contains(extension))
            {
                mediaType = 1;
            }
            else
            {
                DBG("MainComponent::loadMediaDisplay: Unsupported file type \'" << extension
                                                                                << "\'.");

                AlertWindow("Error", "Unsupported file type.", AlertWindow::WarningIcon);
            }

            // Initialize a matching display
            initializeMediaDisplay(mediaType);
        }

        mediaDisplay->setupDisplay(URL(mediaFile));

        lastLoadTime = Time::getCurrentTime();

        playStopButton.setEnabled(true);

        resized();
    }

    void openFileChooser()
    {
        StringArray allExtensions = StringArray(audioExtensions);
        allExtensions.mergeArray(midiExtensions);

        String filePatternsAllowed = "*" + allExtensions.joinIntoString(";*");

        openFileBrowser =
            std::make_unique<FileChooser>("Select a media file...", File(), filePatternsAllowed);

        openFileBrowser->launchAsync(FileBrowserComponent::openMode
                                         | FileBrowserComponent::canSelectFiles,
                                     [this](const FileChooser& browser)
                                     {
                                         File chosenFile = browser.getResult();
                                         if (chosenFile != File {})
                                         {
                                             loadMediaDisplay(chosenFile);
                                         }
                                     });
    }

    void paint(Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        auto area = getLocalBounds();

#if not JUCE_MAC
        menuBar->setBounds(
            area.removeFromTop(LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));
#endif
        auto margin = 5; // Adjusted margin value for top and bottom spacing
        auto docViewHeight = 1;
        auto mainArea = area.removeFromTop(area.getHeight() - docViewHeight);
        // auto documentViewArea = area; // what remains is the 15% area for documentView
        // Row 1: Model Path TextBox and Load Model Button
        auto row1 = mainArea.removeFromTop(30); // adjust height as needed
        modelPathComboBox.setBounds(
            row1.removeFromLeft(static_cast<int>(row1.getWidth() * 0.8f)).reduced(margin));
        //modelPathTextBox.setBounds(row1.removeFromLeft(row1.getWidth() * 0.8f).reduced(margin));
        loadModelButton.setBounds(row1.reduced(margin));
        // Row 2: Name and Author Labels
        auto row2a = mainArea.removeFromTop(35); // adjust height as needed
        nameLabel.setBounds(row2a.removeFromLeft(row2a.getWidth() / 2).reduced(margin));
        nameLabel.setFont(Font(20.0f, Font::bold));
        // nameLabel.setColour(Label::textColourId, mHARPLookAndFeel.textHeaderColor);

        auto row2b = mainArea.removeFromTop(20);
        authorLabel.setBounds(row2b.reduced(margin));
        authorLabel.setFont(Font(10.0f));

        auto row2c = mainArea.removeFromTop(30);
        audioOrMidiLabel.setBounds(row2c.reduced(margin));
        audioOrMidiLabel.setFont(Font(10.0f, Font::bold));
        audioOrMidiLabel.setColour(Label::textColourId, Colours::bisque);

        // Row 3: Description Label

        // A way to dynamically adjust the height of the description label
        // doesn't work perfectly yet, but it's good for now.
        auto font = Font(15.0f);
        descriptionLabel.setFont(font);
        // descriptionLabel.setColour(Label::backgroundColourId, Colours::red);
        auto maxLabelWidth = mainArea.getWidth() - 2 * margin;
        auto numberOfLines =
            font.getStringWidthFloat(descriptionLabel.getText(false)) / maxLabelWidth;
        float textHeight =
            (font.getHeight() + 5) * (std::floor(numberOfLines) + 1) + font.getHeight();

        if (textHeight < 80)
        {
            textHeight = 80;
        }
        auto row3 = mainArea.removeFromTop((int) textHeight).reduced(margin);
        descriptionLabel.setBounds(row3);

        // Row 4: Space URL Hyperlink
        auto row4 = mainArea.removeFromTop(22); // adjust height as needed
        spaceUrlButton.setBounds(row4.reduced(margin).removeFromLeft(row4.getWidth() / 2));
        spaceUrlButton.setFont(Font(11.0f), false, Justification::centredLeft);

        // Row 5: CtrlComponent (flexible height)
        auto row5 = mainArea.removeFromTop(195); // the remaining area is for row 4
        ctrlComponent.setBounds(row5.reduced(margin));

        // An empty space of 20px between the ctrl component and the process button
        mainArea.removeFromTop(10);

        // Row 6: Process Button (taken out in advance to preserve its height)
        auto row6Height = 20; // adjust height as needed
        auto row6 = mainArea.removeFromTop(row6Height);

        // Assign bounds to processButton
        processCancelButton.setBounds(
            row6.withSizeKeepingCentre(100, 20)); // centering the button in the row
        // place the status label to the left of the process button (justified left)
        // statusLabel.setBounds(processCancelButton.getBounds().translated(-200, 0));

        // An empty space of 30px between the process button and the thumbnail area
        mainArea.removeFromTop(30);

        // Row 7: thumbnail area
        auto row7 = mainArea.removeFromTop(150).reduced(margin / 2); // adjust height as needed
        mediaDisplay->setBounds(row7);

        // Row 8: Buttons for Play/Stop and Open File
        auto row8 = mainArea.removeFromTop(50); // adjust height as needed
        playStopButton.setBounds(row8.removeFromLeft(row8.getWidth() / 3).reduced(margin));
        chooseFileButton.setBounds(row8.removeFromLeft(row8.getWidth() / 2).reduced(margin));
        saveFileButton.setBounds(row8.reduced(margin));

        // Status area
        auto row9 = mainArea.removeFromBottom(80);
        // Split row9 to two columns
        auto row9a = row9.removeFromLeft(row9.getWidth() / 2);
        auto row9b = row9;
        instructionsArea.setBounds(row9a.reduced(margin));
        statusArea.setBounds(row9b.reduced(margin));
    }

    void resetUI()
    {
        ctrlComponent.resetUI();
        // Also clear the model card components
        ModelCard empty;
        setModelCard(empty);
    }

    void setModelCard(const ModelCard& card)
    {
        // Set the text for the labels
        nameLabel.setText(String(card.name), dontSendNotification);
        descriptionLabel.setText(String(card.description), dontSendNotification);
        // set the author label text to "by {author}" only if {author} isn't empty
        card.author.empty()
            ? authorLabel.setText("", dontSendNotification)
            : authorLabel.setText("by " + String(card.author), dontSendNotification);
        // It is assumed we only support wav2wav or midi2midi models for now
        if (card.midi_in && card.midi_out && ! card.author.empty())
        {
            audioOrMidiLabel.setText("Midi-to-Midi", dontSendNotification);
        }
        else if (! card.midi_in && ! card.midi_out && ! card.author.empty())
        {
            audioOrMidiLabel.setText("Wav-to-Wav", dontSendNotification);
        }
        else
        {
            audioOrMidiLabel.setText("", dontSendNotification);
        }
    }

    void setStatus(const juce::String& message) { statusArea.setStatusMessage(message); }

    void clearStatus() { statusArea.clearStatusMessage(); }

    void setInstructions(const juce::String& message)
    {
        instructionsArea.setStatusMessage(message);
    }

    void clearInstructions() { instructionsArea.clearStatusMessage(); }

private:
    // HARP UI
    std::unique_ptr<ModelStatusTimer> mModelStatusTimer { nullptr };

    ComboBox modelPathComboBox;
    HoverHandler modelPathComboBoxHandler { modelPathComboBox };

    TextButton chooseFileButton { "Open File" };
    HoverHandler chooseFileButtonHandler { chooseFileButton };

    TextButton saveFileButton { "Save File" };
    HoverHandler saveFileButtonHandler { saveFileButton };

    // cb: TODO:
    // 1. Use HoverHandler for MultiButtons
    // 2. loadModelButton doesn't need to be a MultiButton
    // 3. Modify HoverHandler so that it needs less boilerplate code
    MultiButton loadModelButton;
    MultiButton processCancelButton;
    MultiButton playStopButton;
    MultiButton::Mode loadButtonInfo { "Load",
                                       [this] { loadModelCallback(); },
                                       getUIColourIfAvailable(
                                           LookAndFeel_V4::ColourScheme::UIColour::windowBackground,
                                           Colours::lightgrey) };
    MultiButton::Mode processButtonInfo {
        "Process",
        [this] { processCallback(); },
        getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground,
                               Colours::lightgrey)
    };
    MultiButton::Mode cancelButtonInfo {
        "Cancel",
        [this] { cancelCallback(); },
        getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground,
                               Colours::lightgrey)
    };
    MultiButton::Mode playButtonInfo { "Play", [this] { play(); }, Colours::limegreen };
    MultiButton::Mode stopButtonInfo { "Stop", [this] { stop(); }, Colours::orangered };

    // Label statusLabel;
    // A flag that indicates if the audio file can be saved
    bool saveEnabled = true;
    bool isProcessing = false;

    std::string customPath;
    CtrlComponent ctrlComponent;

    // model card
    Label nameLabel, authorLabel, descriptionLabel, tagsLabel;
    // For now it is assumed that both input and output types
    // of the model are the same (audio or midi)
    Label audioOrMidiLabel;
    HyperlinkButton spaceUrlButton;

    StatusComponent statusArea { 15.0f, juce::Justification::centred };
    StatusComponent instructionsArea { 13.0f, juce::Justification::centredLeft };

    Time lastLoadTime;

    // the model itself
    std::shared_ptr<WebModel> model { new WebModel() };

    std::unique_ptr<FileChooser> openFileBrowser;
    std::unique_ptr<FileChooser> saveFileBrowser;

    std::unique_ptr<MediaDisplayComponent> mediaDisplay;

    std::unique_ptr<HoverHandler> mediaDisplayHandler;

    StringArray audioExtensions = AudioDisplayComponent::getSupportedExtensions();
    StringArray midiExtensions = MidiDisplayComponent::getSupportedExtensions();

    /// CustomThreadPoolJob
    // This one is used for Loading the models
    // The thread pull for Processing lives inside the JobProcessorThread
    ThreadPool threadPool { 1 };
    int jobsFinished;
    int totalJobs;
    JobProcessorThread jobProcessorThread;
    std::vector<CustomThreadPoolJob*> customJobs;

    ChangeBroadcaster loadBroadcaster;
    ChangeBroadcaster processBroadcaster;

    ApplicationCommandManager commandManager;
    // MenuBar
    std::unique_ptr<MenuBarComponent> menuBar;
    // MenuBarPosition menuBarPosition = MenuBarPosition::window;

    // std::unique_ptr<juce::FileLogger> logger { nullptr };
    //==============================================================================

    // void LogAndDBG(const juce::String& message) const
    // {
    //     DBG(message);
    //     if (logger)
    //         logger->logMessage(message);
    // }

    void play()
    {
        if (! mediaDisplay->isPlaying())
        {
            mediaDisplay->start();
            playStopButton.setMode(stopButtonInfo.label);
        }
    }

    void stop()
    {
        if (mediaDisplay->isPlaying())
        {
            mediaDisplay->stop();
            playStopButton.setMode(playButtonInfo.label);
        }
    }

    void resetProcessingButtons()
    {
        processCancelButton.setMode(processButtonInfo.label);
        processCancelButton.setEnabled(true);
        saveEnabled = true;
        isProcessing = false;
        repaint();
    }

    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        if (source == mediaDisplay.get())
        {
            if (mediaDisplay->isFileDropped())
            {
                URL droppedFilePath = mediaDisplay->getDroppedFilePath();

                mediaDisplay->clearDroppedFile();

                // Reload an appropriate display for dropped file
                loadMediaDisplay(droppedFilePath.getLocalFile());
            }
            else if (mediaDisplay->isFileLoaded() && ! mediaDisplay->isPlaying())
            {
                playStopButton.setMode(playButtonInfo.label);
                playStopButton.setEnabled(true);
            }
            else if (mediaDisplay->isFileLoaded() && mediaDisplay->isPlaying())
            {
                playStopButton.setMode(stopButtonInfo.label);
            }
            else
            {
                playStopButton.setMode(playButtonInfo.label);
                playStopButton.setEnabled(false);
            }
        }
        else if (source == &loadBroadcaster)
        {
            DBG("Setting up model card, CtrlComponent, resizing.");
            setModelCard(model->card());
            ctrlComponent.setModel(model);
            ctrlComponent.populateGui();

            SpaceInfo spaceInfo = model->getGradioClient().getSpaceInfo();
            // if we are here, the status can't be ERROR or EMPTY
            if (spaceInfo.status == SpaceInfo::Status::LOCALHOST)
            {
                spaceUrlButton.setButtonText("open localhost in browser");
                spaceUrlButton.setURL(URL(spaceInfo.gradio));
            }
            else
            {
                spaceUrlButton.setButtonText("open " + spaceInfo.userName + "/"
                                             + spaceInfo.modelName + " in browser");
                spaceUrlButton.setURL(URL(spaceInfo.huggingface));
            }
            // spaceUrlButton.setFont(Font(15.00f, Font::plain));
            addAndMakeVisible(spaceUrlButton);

            repaint();

            // now, we can enable the buttons
            if (model->ready())
            {
                processCancelButton.setEnabled(true);
                processCancelButton.setMode(processButtonInfo.label);
            }

            loadModelButton.setEnabled(true);
            modelPathComboBox.setEnabled(true);
            loadModelButton.setButtonText("load");

            // Set the focus to the process button
            // so that the user can press SPACE to trigger the playback
            processCancelButton.grabKeyboardFocus();
            resized();
        }
        else if (source == &processBroadcaster)
        {
            // refresh the display for the new updated file
            URL tempFilePath = mediaDisplay->getTempFilePath();
            mediaDisplay->updateDisplay(tempFilePath);

            // TODO: update Label display
            // Here is how to access the labels
            LabelList& label_list = model->getLabels();
            for (auto& label : label_list)
            {
                // Check if the label is of type AudioLabel
                if (auto* audioLabel = dynamic_cast<AudioLabel*>(label.get()))
                {
                    // Access fields specific to AudioLabel
                    // amplitude is an optional value for audioLabels
                    // check if it has a value before accessing it
                    if (audioLabel->amplitude.has_value())
                        std::cout << "Amplitude: " << audioLabel->amplitude.value() << std::endl;
                    // Or you can use the to_y() method to get y.
                    std::optional<float> y = audioLabel->to_y();
                    if (y.has_value())
                        std::cout << "Y: " << y.value() << std::endl;
                }
                else if (auto* spectrogramLabel = dynamic_cast<SpectrogramLabel*>(label.get()))
                {
                    // similar
                }
                else if (auto* midiLabel = dynamic_cast<MidiLabel*>(label.get()))
                {
                    // similar
                }
                else
                {
                    // similar
                }
            }

            // now, we can enable the process button
            resetProcessingButtons();
        }
        else if (source == mModelStatusTimer.get())
        {
            // update the status label
            DBG("HARPProcessorEditor::changeListenerCallback: updating status label");
            // statusLabel.setText(model->getStatus(), dontSendNotification);
            setStatus(model->getStatus());
        }
        else
        {
            DBG("HARPProcessorEditor::changeListenerCallback: unhandled change broadcaster");
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
