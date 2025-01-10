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

#include "ControlAreaWidget.h"
#include "ThreadPoolJob.h"
#include "TrackAreaWidget.h"
#include "WebModel.h"

#include "gui/CustomPathDialog.h"
#include "gui/HoverHandler.h"
#include "gui/ModelAuthorLabel.h"
#include "gui/MultiButton.h"
#include "gui/StatusComponent.h"
#include "gui/TitledTextBox.h"

#include "gradio/GradioClient.h"

#include "HarpLogger.h"
#include "external/magic_enum.hpp"
// #include "media/AudioDisplayComponent.h"
// #include "media/MediaDisplayComponent.h"
// #include "media/MidiDisplayComponent.h"
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
        // if (saveEnabled)
        // {
        //     DBG("HARPProcessorEditor::buttonClicked save button listener activated");
        //     mediaDisplay->overwriteTarget();

        //     saveEnabled = false;
        //     setStatus("File saved successfully");
        // }
        // else
        // {
        //     DBG("save button is disabled");
        //     setStatus("Nothing to save");
        // }
    }

    void saveAsCallback()
    {
        // if (mediaDisplay->isFileLoaded())
        // {
        //     StringArray validExtensions = mediaDisplay->getInstanceExtensions();
        //     String filePatternsAllowed = "*" + validExtensions.joinIntoString(";*");
        //     saveFileBrowser = std::make_unique<FileChooser>(
        //         "Select a media file...", File(), filePatternsAllowed);
        //     // Launch the file chooser dialog asynchronously
        //     saveFileBrowser->launchAsync(
        //         FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles,
        //         [this](const FileChooser& browser)
        //         {
        //             StringArray validExtensions = mediaDisplay->getInstanceExtensions();
        //             File newFile = browser.getResult();
        //             if (newFile != File {})
        //             {
        //                 if (newFile.getFileExtension().compare("") == 0)
        //                 {
        //                     newFile = newFile.withFileExtension(validExtensions[0]);
        //                 }
        //                 if (validExtensions.contains(newFile.getFileExtension()))
        //                 {
        //                     URL tempFilePath = mediaDisplay->getTempFilePath();

        //                     // Attempt to save the file to the new location
        //                     bool saveSuccessful = tempFilePath.getLocalFile().copyFileTo(newFile);
        //                     if (saveSuccessful)
        //                     {
        //                         // Inform the user of success
        //                         AlertWindow::showMessageBoxAsync(AlertWindow::InfoIcon,
        //                                                          "Save As",
        //                                                          "File successfully saved as:\n"
        //                                                              + newFile.getFullPathName(),
        //                                                          "OK");

        //                         // Update any necessary internal state
        //                         // currentAudioFile = AudioFile(newFile); // Assuming a wrapper, adjust accordingly
        //                         DBG("File successfully saved as " << newFile.getFullPathName());
        //                         loadMediaDisplay(newFile);
        //                     }
        //                     else
        //                     {
        //                         // Inform the user of failure
        //                         AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
        //                                                          "Save As Failed",
        //                                                          "Failed to save file as:\n"
        //                                                              + newFile.getFullPathName(),
        //                                                          "OK");
        //                         DBG("Failed to save file as " << newFile.getFullPathName());
        //                     }
        //                 }
        //                 else
        //                 {
        //                     // Inform the user of failure
        //                     AlertWindow::showMessageBoxAsync(
        //                         AlertWindow::WarningIcon,
        //                         "Save As Failed",
        //                         "Can't save file with extension " + newFile.getFileExtension()
        //                             + " \n Valid extensions are: "
        //                             + validExtensions.joinIntoString(";"),
        //                         "OK");
        //                 }
        //             }
        //             else
        //             {
        //                 DBG("Save As operation was cancelled by the user.");
        //             }
        //         });
        // }
        // else
        // {
        //     setStatus("Nothing to save. Please load an audio file first.");
        // }
    }

    void undoCallback()
    {
        // DBG("Undoing last edit");

        // // check if the audio file is loaded
        // if (! mediaDisplay->isFileLoaded())
        // {
        //     // TODO - gray out undo option in this case?
        //     // Fail with beep, we should just ignore this if it doesn't make sense
        //     DBG("No file loaded to perform operation on");
        //     juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
        //     return;
        // }

        // if (isProcessing)
        // {
        //     DBG("Can't undo while processing occurring!");
        //     juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
        //     return;
        // }

        // if (! mediaDisplay->iteratePreviousTempFile())
        // {
        //     DBG("Nothing to undo!");
        //     juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
        // }
        // else
        // {
        //     saveEnabled = true;
        //     DBG("Undo callback completed successfully");
        // }
    }

    void redoCallback()
    {
        // DBG("Redoing last edit");

        // // check if the audio file is loaded
        // if (! mediaDisplay->isFileLoaded())
        // {
        //     // TODO - gray out undo option in this case?
        //     // Fail with beep, we should just ignore this if it doesn't make sense
        //     DBG("No file loaded to perform operation on");
        //     juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
        //     return;
        // }

        // if (isProcessing)
        // {
        //     DBG("Can't redo while processing occurring!");
        //     juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
        //     return;
        // }

        // if (! mediaDisplay->iterateNextTempFile())
        // {
        //     DBG("Nothing to redo!");
        //     juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
        // }
        // else
        // {
        //     saveEnabled = true;
        //     DBG("Redo callback completed successfully");
        // }
    }

    void loadModelCallback()
    {
        // Get the URL/path the user provided in the comboBox
        std::string pathURL;
        if (modelPathComboBox.getSelectedItemIndex() == 0)
            pathURL = customPath;
        else
            pathURL = modelPathComboBox.getText().toStdString();

        std::map<std::string, std::any> params = {
            { "url", pathURL },
        };
        // resetUI();

        // disable the load button until the model is loaded
        loadModelButton.setEnabled(false);
        modelPathComboBox.setEnabled(false);
        loadModelButton.setButtonText("loading...");

        // disable the process button until the model is loaded
        processCancelButton.setEnabled(false);

        // loading happens asynchronously.
        threadPool.addJob(
            [this, params]
            {
                try
                {
                    juce::String loadingError;
                    /*
                        cb: This is an idea, that might be useful for the future
                        Whenever trying to load a new gradio app, we could create a new WebModel
                        if loading is successful, we could replace the old model with the new one
                        This prevents the confussion of having ModelStatuses in the same class that
                        correspond to different Gradio Apps. 
                        For example, in the scenario the new model fails to load, we want to go back to 
                        the one we had before. However we got a 
                        ModelStatus::ERROR, but the old model is still loaded, so the status should change
                        to ModelStatus::LOADED (or whatever it was before the failed attempt). 
                        For now, we added a lastStatus variable to the WebModel class to keep track of the
                        status of the model before the attempt to load a new model.
                    */

                    // set the last status to the current status
                    // If loading of the new model fails,
                    // we want to go back to the status we had before the failed attempt
                    model->setLastStatus(model->getStatus());

                    OpResult loadingResult = model->load(params);
                    if (loadingResult.failed())
                    {
                        throw loadingResult.getError();
                    }

                    // loading succeeded
                    // Do some UI stuff to add the new model to the comboBox
                    // if it's not already there
                    // and update the lastSelectedItemIndex and lastLoadedModelItemIndex
                    MessageManager::callAsync(
                        [this, loadingResult]
                        {
                            resetUI();
                            if (modelPathComboBox.getSelectedItemIndex() == 0)
                            {
                                bool alreadyInComboBox = false;

                                for (int i = 0; i < modelPathComboBox.getNumItems(); ++i)
                                {
                                    if (modelPathComboBox.getItemText(i)
                                        == (juce::String) customPath)
                                    {
                                        alreadyInComboBox = true;
                                        modelPathComboBox.setSelectedId(i + 1);
                                        lastSelectedItemIndex = i;
                                        lastLoadedModelItemIndex = i;
                                    }
                                }

                                if (! alreadyInComboBox)
                                {
                                    int new_id = modelPathComboBox.getNumItems() + 1;
                                    modelPathComboBox.addItem(customPath, new_id);
                                    modelPathComboBox.setSelectedId(new_id);
                                    lastSelectedItemIndex = new_id - 1;
                                    lastLoadedModelItemIndex = new_id - 1;
                                }
                            }
                            else
                            {
                                lastLoadedModelItemIndex = modelPathComboBox.getSelectedItemIndex();
                            }
                            processLoadingResult(loadingResult);
                        });
                }
                catch (Error& loadingError)
                {
                    Error::fillUserMessage(loadingError);
                    LogAndDBG("Error in Model Loading:\n" + loadingError.devMessage);
                    auto msgOpts =
                        MessageBoxOptions()
                            .withTitle("Loading Error")
                            .withIconType(AlertWindow::WarningIcon)
                            .withTitle("Error")
                            .withMessage("An error occurred while loading the WebModel: \n"
                                         + loadingError.userMessage);
                    // if (! String(e.what()).contains("404")
                    //     && ! String(e.what()).contains("Invalid URL"))
                    if (loadingError.type != ErrorType::InvalidURL)
                    {
                        msgOpts = msgOpts.withButton("Open Space URL");
                    }

                    msgOpts = msgOpts.withButton("Open HARP Logs").withButton("Ok");
                    auto alertCallback = [this, msgOpts, loadingError](int result)
                    {
                        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                        // NOTE (hugo): there's something weird about the button indices assigned by the msgOpts here
                        // DBG("ALERT-CALLBACK: buttonClicked alertCallback listener activated: chosen: " << chosen);
                        // auto chosen = msgOpts.getButtonText(result);
                        // they're not the same as the order of the buttons in the alert
                        // this is the order that I actually observed them to be.
                        // UPDATE/TODO (xribene): This should be fixed in Juce v8
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

                        if (chosen == "Open HARP Logs")
                        {
                            HarpLogger::getInstance()->getLogFile().revealToUser();
                        }
                        else if (chosen == "Open Space URL")
                        {
                            // get the spaceInfo
                            SpaceInfo spaceInfo = model->getGradioClient().getSpaceInfo();
                            if (spaceInfo.status == SpaceInfo::Status::GRADIO)
                            {
                                URL spaceUrl = this->model->getGradioClient().getSpaceInfo().gradio;
                                spaceUrl.launchInDefaultBrowser();
                            }
                            else if (spaceInfo.status == SpaceInfo::Status::HUGGINGFACE)
                            {
                                URL spaceUrl =
                                    this->model->getGradioClient().getSpaceInfo().huggingface;
                                spaceUrl.launchInDefaultBrowser();
                            }
                            else if (spaceInfo.status == SpaceInfo::Status::LOCALHOST)
                            {
                                // either choose hugingface or gradio, they are the same
                                URL spaceUrl =
                                    this->model->getGradioClient().getSpaceInfo().huggingface;
                                spaceUrl.launchInDefaultBrowser();
                            }
                            // URL spaceUrl =
                            //     this->model->getGradioClient().getSpaceInfo().huggingface;
                            // spaceUrl.launchInDefaultBrowser();
                        }

                        if (lastLoadedModelItemIndex == -1)
                        {
                            // If before the failed attempt to load a new model, we HAD NO model loaded
                            // TODO: these two functions we call here might be an overkill for this case
                            // we need to simplify
                            MessageManager::callAsync(
                                [this, loadingError]
                                {
                                    resetModelPathComboBox();
                                    model->setStatus(ModelStatus::INITIALIZED);
                                    processLoadingResult(OpResult::fail(loadingError));
                                });
                        }
                        else
                        {
                            // If before the failed attempt to load a new model, we HAD a model loaded
                            MessageManager::callAsync(
                                [this, loadingError]
                                {
                                    // We set the status to
                                    // the status of the model before the failed attempt
                                    model->setStatus(model->getLastStatus());
                                    processLoadingResult(OpResult::fail(loadingError));
                                });
                        }

                        // This if/elseif/else block is responsible for setting the selected item
                        // in the modelPathComboBox to the correct item (i.e the model/path/app that
                        // was selected before the failed attempt to load a new model)
                        // cb: sometimes setSelectedId it doesn't work and I dont know why.
                        // I've tried nesting it in MessageManage::callAsync, but still nothing.
                        if (lastLoadedModelItemIndex != -1)
                        {
                            modelPathComboBox.setSelectedId(lastLoadedModelItemIndex + 1);
                        }
                        else if (lastLoadedModelItemIndex == -1 && lastSelectedItemIndex != -1)
                        {
                            modelPathComboBox.setSelectedId(lastSelectedItemIndex + 1);
                        }
                        else
                        {
                            resetModelPathComboBox();
                            MessageManager::callAsync([this, loadingError]
                                                      { loadModelButton.setEnabled(false); });
                        }
                    };

                    AlertWindow::showAsync(msgOpts, alertCallback);
                    saveEnabled = false;
                }
                catch (const std::exception& e)
                {
                    // Catch any other standard exceptions (like std::runtime_error)
                    DBG("Caught std::exception: " << e.what());
                    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                                     "Error",
                                                     "An unexpected error occurred: "
                                                         + juce::String(e.what()));
                }
                catch (...) // Catch any other exceptions
                {
                    DBG("Caught unknown exception");
                    AlertWindow::showMessageBoxAsync(
                        AlertWindow::WarningIcon, "Error", "An unexpected error occurred.");
                }
            });
    }

    void resetModelPathComboBox()
    {
        // cb: why do we resetUI inside a function named resetModelPathComboBox ?
        resetUI();
        //should I clear this?
        // spaceUrlButton.setButtonText("");
        // spaceUrlButtonHandler.detach();

        int numItems = modelPathComboBox.getNumItems();
        std::vector<std::string> options;

        for (int i = 0; i < numItems; ++i) // item indexes are 1-based in JUCE
        {
            String itemText = modelPathComboBox.getItemText(i);
            options.push_back(itemText.toStdString());
            DBG("Item index" << i << ": " << itemText);
        }

        modelPathComboBox.clear();

        modelPathComboBox.setTextWhenNothingSelected("choose a model");
        for (auto i = 0u; i < options.size(); ++i)
        {
            modelPathComboBox.addItem(options[i], static_cast<int>(i) + 1);
        }
        lastSelectedItemIndex = -1;
    }

    void focusCallback()
    {
        // if (mediaDisplay->isFileLoaded())
        // {
        //     Time lastModTime =
        //         mediaDisplay->getTargetFilePath().getLocalFile().getLastModificationTime();
        //     if (lastModTime > lastLoadTime)
        //     {
        //         // Create an AlertWindow
        //         auto* reloadCheckWindow = new AlertWindow(
        //             "File has been modified",
        //             "The loaded file has been modified in a different editor! Would you like HARP to load the new version of the file?\nWARNING: This will clear the undo log and cause all unsaved edits to be lost!",
        //             AlertWindow::QuestionIcon);

        //         reloadCheckWindow->addButton("Yes", 1, KeyPress(KeyPress::returnKey));
        //         reloadCheckWindow->addButton("No", 0, KeyPress(KeyPress::escapeKey));

        //         // Show the window and handle the result asynchronously
        //         reloadCheckWindow->enterModalState(
        //             true,
        //             new CustomPathAlertCallback(
        //                 [this, reloadCheckWindow](int result)
        //                 {
        //                     if (result == 1)
        //                     { // Yes was clicked
        //                         DBG("Reloading file");
        //                         loadMediaDisplay(mediaDisplay->getTargetFilePath().getLocalFile());
        //                     }
        //                     else
        //                     { // No was clicked or the window was closed
        //                         DBG("Not reloading file");
        //                         lastLoadTime =
        //                             Time::getCurrentTime(); //Reset time so we stop asking
        //                     }
        //                     delete reloadCheckWindow;
        //                 }),
        //             true);
        //     }
        // }
    }

    void initMenuBar()
    {
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
    }

    void initSomeButtons()
    {
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
    }

    void initPlayStopButton()
    {
        playButtonInfo = MultiButton::Mode {
            "Play",
            [this] { play(); },
            juce::Colours::limegreen,
            "Click to start playback",
            MultiButton::DrawingMode::IconOnly,
            fontaudio::Stop,
        };
        stopButtonInfo = MultiButton::Mode {
            "Stop",
            [this] { stop(); },
            Colours::orangered,
            "Click to stop playback",
            MultiButton::DrawingMode::IconOnly,
            fontaudio::Stop,
        };
        playStopButton.addMode(playButtonInfo);
        playStopButton.addMode(stopButtonInfo);
        playStopButton.setMode(playButtonInfo.label);
        playStopButton.setEnabled(false);
        addAndMakeVisible(playStopButton);
    }

    void initProcessCancelButton()
    {
        // The Process/Cancel button
        processButtonInfo = MultiButton::Mode {
            "Process",
            [this] { processCallback(); },
            Colours::orangered,
            "Click to send the audio file for processing",
            MultiButton::DrawingMode::TextOnly,
            fontaudio::Pause,
        };

        cancelButtonInfo = MultiButton::Mode {
            "Cancel",
            [this] { cancelCallback(); },
            Colours::lightgrey,
            "Click to cancel the processing",
            MultiButton::DrawingMode::TextOnly,
            fontaudio::Pause,
        };

        processCancelButton.addMode(processButtonInfo);
        processCancelButton.addMode(cancelButtonInfo);
        processCancelButton.setMode(processButtonInfo.label);
        processCancelButton.setEnabled(false);
        addAndMakeVisible(processCancelButton);

        processBroadcaster.addChangeListener(this);
        saveEnabled = false;

        ModelStatus currentStatus = model->getStatus();
        if (currentStatus == ModelStatus::LOADED || currentStatus == ModelStatus::FINISHED)
        {
            processCancelButton.setEnabled(true);
            processCancelButton.setMode(processButtonInfo.label);
        }
        else if (currentStatus == ModelStatus::PROCESSING || currentStatus == ModelStatus::STARTING
                 || currentStatus == ModelStatus::SENDING)
        {
            processCancelButton.setEnabled(true);
            processCancelButton.setMode(cancelButtonInfo.label);
        }
        setStatus(currentStatus);
    }

    void initLoadModelButton()
    {
        loadButtonInfo = MultiButton::Mode {
            "Load Model",
            [this] { loadModelCallback(); },
            Colours::lightgrey,
            "Click to load the selected model path",
            MultiButton::DrawingMode::IconOnly,
            fontawesome::Download,
        };
        loadModelButton.addMode(loadButtonInfo);
        loadModelButton.setMode(loadButtonInfo.label);
        loadModelButton.setEnabled(false);
        addAndMakeVisible(loadModelButton);
        loadBroadcaster.addChangeListener(this);
    }

    void initModelPathComboBox()
    {
        // model path textbox
        std::vector<std::string> modelPaths = {
            "custom path...",
            "cwitkowitz/timbre-trap",
            "npruyne/audio_similarity",
            "hugggof/vampnet-music",
            "hugggof/vampnet-percussion",
            "hugggof/vampnet-n64",
            "hugggof/vampnet-choir",
            "hugggof/vampnet-opera",
            "hugggof/vampnet-machines",
            "hugggof/vampnet-birds",
            // "descript/vampnet",
            // "pharoAIsanders420/micro-musicgen-jungle",
            "hugggof/nesquik",
            // "hugggof/pitch_shifter",
            "hugggof/harmonic_percussive",
            // "xribene/pitch_shifter",
            "xribene/pitch_shifter_awake",
            "xribene/midi_pitch_shifter",
            // "xribene/pitch_shifter_slow",
            "http://localhost:7860",
            // "https://xribene-midi-pitch-shifter.hf.space/",
            // "https://huggingface.co/spaces/xribene/midi_pitch_shifter",
            // "xribene/midi_pitch_shifter",
            // "https://huggingface.co/spaces/xribene/pitch_shifter",
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
                // Create and show the custom path dialog with a callback
                std::function<void(const juce::String&)> loadCallback =
                    [this](const juce::String& customPath)
                {
                    DBG("Custom path entered: " + customPath);
                    this->customPath = customPath.toStdString(); // Store the custom path
                    loadModelButton.triggerClick(); // Trigger the load model button click
                };
                std::function<void()> cancelCallback = [this]()
                {
                    // modelPathComboBox.setSelectedId(lastSelectedItemIndex);
                    if (lastLoadedModelItemIndex != -1)
                    {
                        modelPathComboBox.setSelectedId(lastLoadedModelItemIndex + 1);
                    }
                    else if (lastLoadedModelItemIndex == -1 && lastSelectedItemIndex != -1)
                    {
                        modelPathComboBox.setSelectedId(lastSelectedItemIndex + 1);
                    }
                    else
                    {
                        resetModelPathComboBox();
                        MessageManager::callAsync([this] { loadModelButton.setEnabled(false); });
                    }
                };
                CustomPathDialog::showDialogWindow(loadCallback, cancelCallback);
            }
            else
            {
                lastSelectedItemIndex = modelPathComboBox.getSelectedItemIndex();
            }
            loadModelButton.setEnabled(true);
        };

        addAndMakeVisible(modelPathComboBox);
    }

    explicit MainComponent(const URL& initialFilePath = URL())
        : jobsFinished(0),
          totalJobs(0),
          jobProcessorThread(customJobs, jobsFinished, totalJobs, processBroadcaster)
    {
        HarpLogger::getInstance()->initializeLogger();
        fontaudioHelper = std::make_shared<fontaudio::IconHelper>();
        fontawesomeHelper = std::make_shared<fontawesome::IconHelper>();

        initSomeButtons();
        initPlayStopButton();

        // Initialize default media display
        // initializeMediaDisplay(0, mediaDisplay);
        // Create a new mediaDisplay and add it in the inputMediaDisplays vector
        // inputMediaDisplays.push_back(std::make_unique<AudioDisplayComponent>());
        // outputMediaDisplays.push_back(std::make_unique<MidiDisplayComponent>());
        // Initialize all the inputMediaDisplays
        // for (int i = 0; i < inputMediaDisplays.size(); ++i)
        // {
        //     initializeMediaDisplay(0, inputMediaDisplays[i]);
        // }
        // for (int i = 0; i < outputMediaDisplays.size(); ++i)
        // {
        //     initializeMediaDisplay(1, outputMediaDisplays[i]);
        // }

        // TODO: check how it behaves when running with a file as input
        // if (initialFilePath.isLocalFile())
        // {
        //     // TODO - it seems command line args are handled through Main.cpp and this is never hit
        //     // Load initial file into matching media display
        //     loadMediaDisplay(initialFilePath.getLocalFile(), mediaDisplay);
        // }

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

        initMenuBar();

        initProcessCancelButton();

        initLoadModelButton();

        // add a status timer to update the status label periodically
        mModelStatusTimer = std::make_unique<ModelStatusTimer>(model);
        mModelStatusTimer->addChangeListener(this);
        mModelStatusTimer->startTimer(50); // 100 ms interval

        initModelPathComboBox();

        // model controls
        controlAreaWidget.setModel(model);
        addAndMakeVisible(controlAreaWidget);
        controlAreaWidget.populateControls();

        trackAreaWidget.setModel(model);
        addAndMakeVisible(trackAreaWidget);
        trackAreaWidget.populateTracks();

        addAndMakeVisible(descriptionLabel);
        // addAndMakeVisible(tagsLabel);
        // addAndMakeVisible(audioOrMidiLabel);

        addAndMakeVisible(statusBox);
        addAndMakeVisible(instructionBox);

        // model card component
        // Get the modelCard from the EditorView
        auto& card = model->card();
        setModelCard(card);

        jobProcessorThread.startThread();

        setOpaque(true);
        setSize(800, 2000);
        // set to full screen
        // setFullScreen(true);
        resized();
    }

    ~MainComponent() override
    {
        // mediaDisplay->removeChangeListener(this);
        // for (auto& inputMediaDisplay : inputMediaDisplays)
        // {
        //     inputMediaDisplay->removeChangeListener(this);
        // }
        // for (auto& outputMediaDisplay : outputMediaDisplays)
        // {
        //     outputMediaDisplay->removeChangeListener(this);
        // }

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
        OpResult cancelResult = model->cancel();
        if (cancelResult.failed())
        {
            // This "if" block hasn't been tested

            LogAndDBG(cancelResult.getError().devMessage.toStdString());
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                             "Cancel Error",
                                             "An error occurred while cancelling the processing: \n"
                                                 + cancelResult.getError().devMessage);
            // processCancelButton.setEnabled(true);
            // processCancelButton.setMode(processButtonInfo.label);
            resetProcessingButtons();
            return;
        }
        // We already added a temp file, so we need to undo that
        // TODO: this is functionality that I need to add back
        // mediaDisplay->iteratePreviousTempFile();
        // mediaDisplay->clearFutureTempFiles();
        processCancelButton.setEnabled(false);
    }

    void processCallback()
    {
        return;

        // DBG("HARPProcessorEditor::buttonClicked button listener activated");

        // // check if the audio file is loaded for the first element of the inputMediaDisplays
        // if (! inputMediaDisplays[0]->isFileLoaded())
        // {
        //     AlertWindow::showMessageBoxAsync(
        //         AlertWindow::WarningIcon,
        //         "Error",
        //         "Audio file is not loaded. Please load an audio file first.");
        //     return;
        // }

        // processCancelButton.setEnabled(true);
        // processCancelButton.setMode(cancelButtonInfo.label);

        // saveEnabled = false;
        // isProcessing = true;

        // if (model == nullptr)
        // {
        //     AlertWindow("Error",
        //                 "Model is not loaded. Please load a model first.",
        //                 AlertWindow::WarningIcon);
        //     isProcessing = false;
        //     return;
        // }

        // bool matchingModel = true;

        // // if (dynamic_cast<AudioDisplayComponent*>(inputMediaDisplays[0].get()))
        // // {
        // //     matchingModel = ! model->card().midi_in; //&& ! model->card().midi_out;
        // // }
        // // else
        // // {
        // //     matchingModel = model->card().midi_in; //&& model->card().midi_out;
        // // }

        // // Check if the model's type (Audio or MIDI) matches the input file's type
        // // If not, show an error message and ask the user to either use another model
        // // or another appropriate file
        // if (! matchingModel)
        // {
        //     LogAndDBG("Model and file type mismatch");
        //     AlertWindow::showMessageBoxAsync(
        //         AlertWindow::WarningIcon,
        //         "Processing Error",
        //         "Model and file type mismatch. Please use an appropriate model or file.");
        //     // processBroadcaster.sendChangeMessage();
        //     resetProcessingButtons();
        //     return;
        // }

        // inputMediaDisplays[0]->addNewTempFile();
        // outputMediaDisplays[0]->addNewTempFile();
        // // print how many jobs are currently in the threadpool
        // LogAndDBG("threadPool.getNumJobs: " + std::to_string(threadPool.getNumJobs()));

        // // empty customJobs
        // customJobs.clear();

        // customJobs.push_back(new CustomThreadPoolJob([this] { // &jobsFinished, totalJobs
        //     // Individual job code for each iteration
        //     // copy the audio file, with the same filename except for an added _harp to the stem
        //     OpResult processingResult =
        //         model->process(inputMediaDisplays[0]->getTempFilePath().getLocalFile(),
        //                         outputMediaDisplays[0]->getTempFilePath().getLocalFile());
        //     if (processingResult.failed())
        //     {
        //         Error processingError = processingResult.getError();
        //         Error::fillUserMessage(processingError);
        //         LogAndDBG("Error in Processing:\n" + processingError.devMessage.toStdString());
        //         AlertWindow::showMessageBoxAsync(
        //             AlertWindow::WarningIcon,
        //             "Processing Error",
        //             "An error occurred while processing the audio file: \n"
        //                 + processingError.userMessage);
        //         // cb: I commented this out, and it doesn't seem to change anything
        //         // it was also causing a crash. If we need it, it needs to run on
        //         // the message thread using MessageManager::callAsync
        //         // resetProcessingButtons();
        //         return;
        //     }
        //     // load the audio file again
        //     processBroadcaster.sendChangeMessage();

        // }));

        // // Now the customJobs are ready to be added to be run in the threadPool
        // jobProcessorThread.signalTask();
    }

    // void initializeMediaDisplay(int mediaType, std::unique_ptr<MediaDisplayComponent>& cur_mediaDisplay)
    // {
    //     if (mediaType == 1)
    //     {
    //         cur_mediaDisplay = std::make_unique<MidiDisplayComponent>();
    //     }
    //     else
    //     {
    //         // Default to audio display
    //         cur_mediaDisplay = std::make_unique<AudioDisplayComponent>();
    //     }
    //     addAndMakeVisible(cur_mediaDisplay.get());
    //     cur_mediaDisplay->addChangeListener(this);
    //     // mediaDisplayHandler = std::make_unique<HoverHandler>(*mediaDisplay);
    //     // mediaDisplayHandler->onMouseEnter = [this]() { mediaDisplayHandler->onMouseMove(); };
    //     // mediaDisplayHandler->onMouseMove = [this]()
    //     // { setInstructions(mediaDisplay->getMediaHandlerInstructions()); };
    //     // mediaDisplayHandler->onMouseExit = [this]() { clearInstructions(); };
    //     // mediaDisplayHandler->attach();
    // }

    void loadMediaDisplay(File mediaFile, std::unique_ptr<MediaDisplayComponent>& cur_mediaDisplay)
    {
        return;
        // // Check the file extension to determine type
        // String extension = mediaFile.getFileExtension();

        // bool matchingDisplay = true;

        // if (dynamic_cast<AudioDisplayComponent*>(cur_mediaDisplay.get()))
        // {
        //     matchingDisplay = audioExtensions.contains(extension);
        // }
        // else
        // {
        //     matchingDisplay = midiExtensions.contains(extension);
        // }

        // if (! matchingDisplay)
        // {
        //     // Remove the existing media display
        //     removeChildComponent(cur_mediaDisplay.get());
        //     cur_mediaDisplay->removeChangeListener(this);
        //     // mediaDisplayHandler->detach();

        //     int mediaType = 0;

        //     if (audioExtensions.contains(extension))
        //     {
        //     }
        //     else if (midiExtensions.contains(extension))
        //     {
        //         mediaType = 1;
        //     }
        //     else
        //     {
        //         DBG("MainComponent::loadMediaDisplay: Unsupported file type \'" << extension
        //                                                                         << "\'.");

        //         AlertWindow("Error", "Unsupported file type.", AlertWindow::WarningIcon);
        //     }

        //     // Initialize a matching display
        //     initializeMediaDisplay(mediaType, cur_mediaDisplay);
        // }

        // cur_mediaDisplay->setupDisplay(URL(mediaFile));

        // lastLoadTime = Time::getCurrentTime();

        // playStopButton.setEnabled(true);

        // resized();
    }
    // TODO: ignore that for now. Load files using drag n drop which works fine
    // for multiple mediaDisplays
    // void loadMediaDisplay3(File mediaFile)
    // {
    //     // Check the file extension to determine type
    //     String extension = mediaFile.getFileExtension();

    //     bool matchingDisplay = true;

    //     if (dynamic_cast<AudioDisplayComponent*>(mediaDisplay.get()))
    //     {
    //         matchingDisplay = audioExtensions.contains(extension);
    //     }
    //     else
    //     {
    //         matchingDisplay = midiExtensions.contains(extension);
    //     }

    //     if (! matchingDisplay)
    //     {
    //         // Remove the existing media display
    //         removeChildComponent(mediaDisplay.get());
    //         mediaDisplay->removeChangeListener(this);
    //         mediaDisplayHandler->detach();

    //         int mediaType = 0;

    //         if (audioExtensions.contains(extension))
    //         {
    //         }
    //         else if (midiExtensions.contains(extension))
    //         {
    //             mediaType = 1;
    //         }
    //         else
    //         {
    //             DBG("MainComponent::loadMediaDisplay: Unsupported file type \'" << extension
    //                                                                             << "\'.");

    //             AlertWindow("Error", "Unsupported file type.", AlertWindow::WarningIcon);
    //         }

    //         // Initialize a matching display
    //         initializeMediaDisplay(mediaType);
    //     }

    //     mediaDisplay->setupDisplay(URL(mediaFile));

    //     lastLoadTime = Time::getCurrentTime();

    //     playStopButton.setEnabled(true);

    //     resized();
    // }

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
                                             //  loadMediaDisplay(chosenFile, mediaDisplay);
                                             // loadMediaDisplay(chosenFile, inputMediaDisplays[0]);
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

        // Create a FlexBox container
        juce::FlexBox flexBox;
        flexBox.flexDirection = juce::FlexBox::Direction::column;
        flexBox.alignContent = juce::FlexBox::AlignContent::stretch;
        flexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

        // Row 1: Model Path ComboBox and Load Model Button
        juce::FlexBox row1;
        row1.flexDirection = juce::FlexBox::Direction::row;
        row1.items.add(juce::FlexItem(modelPathComboBox).withFlex(8).withMargin(margin));
        row1.items.add(juce::FlexItem(loadModelButton).withFlex(1).withMargin(margin));
        flexBox.items.add(juce::FlexItem(row1).withFlex(0.2));

        // Row 2: ModelName / AuthorName Labels
        juce::FlexBox row2;
        row2.flexDirection = juce::FlexBox::Direction::row;
        row2.items.add(juce::FlexItem(modelAuthorLabel).withFlex(0.5).withMargin(margin));
        row2.items.add(juce::FlexItem().withFlex(0.5).withMargin(margin));
        flexBox.items.add(juce::FlexItem(row2).withFlex(0.5));

        flexBox.performLayout(area);

        // Row 3: Description
        auto font = Font(15.0f);
        descriptionLabel.setFont(font);
        // descriptionLabel.setColour(Label::backgroundColourId, Colours::red);
        auto maxLabelWidth = area.getWidth() - 2 * margin;
        auto numberOfLines =
            font.getStringWidthFloat(descriptionLabel.getText(false)) / maxLabelWidth;
        float textHeight =
            (font.getHeight() + 5) * (std::floor(numberOfLines) + 1) + font.getHeight();
        flexBox.items.add(
            juce::FlexItem(descriptionLabel).withHeight(textHeight).withMargin(margin));

        // Row 4: Control Area Widget
        flexBox.items.add(juce::FlexItem(controlAreaWidget).withFlex(1).withMargin(margin));

        // Row 5: Process Cancel Button
        // Row for Process Cancel Button
        juce::FlexBox rowProcessCancelButton;
        rowProcessCancelButton.flexDirection = juce::FlexBox::Direction::row;
        rowProcessCancelButton.justifyContent = juce::FlexBox::JustifyContent::center;
        rowProcessCancelButton.items.add(juce::FlexItem().withFlex(1));
        rowProcessCancelButton.items.add(
            juce::FlexItem(processCancelButton).withWidth(area.getWidth() / 4).withMargin(margin));
        rowProcessCancelButton.items.add(juce::FlexItem().withFlex(1));
        flexBox.items.add(juce::FlexItem(rowProcessCancelButton).withFlex(0.25));

        // Row 6: Input and Output Tracks Area Widget
        flexBox.items.add(juce::FlexItem(trackAreaWidget).withFlex(4).withMargin(margin));

        // Row 7: Play/Stop Button, Open File Button, and Save File Button
        // juce::FlexBox row7;
        // row7.flexDirection = juce::FlexBox::Direction::row;
        // row7.items.add(juce::FlexItem(playStopButton).withFlex(1).withMargin(margin));
        // row7.items.add(juce::FlexItem(chooseFileButton).withFlex(1).withMargin(margin));
        // row7.items.add(juce::FlexItem(saveFileButton).withFlex(1).withMargin(margin));
        // flexBox.items.add(juce::FlexItem(row7).withFlex(1));

        // Row 8: Instructions Area and Status Area
        juce::FlexBox row8;
        row8.flexDirection = juce::FlexBox::Direction::row;
        row8.items.add(juce::FlexItem(*instructionBox).withFlex(1).withMargin(margin));
        row8.items.add(juce::FlexItem(*statusBox).withFlex(1).withMargin(margin));
        flexBox.items.add(juce::FlexItem(row8).withFlex(0.4));

        // Apply the FlexBox layout to the main area
        flexBox.performLayout(area);
    }
    void resetUI()
    {
        controlAreaWidget.resetUI();
        trackAreaWidget.resetUI();
        // Also clear the model card components
        ModelCard empty;
        setModelCard(empty);
        // modelAuthorLabelHandler.detach();
    }

    void setModelCard(const ModelCard& card)
    {
        // Set the text for the labels
        // nameLabel.setText(String(card.name), dontSendNotification);
        modelAuthorLabel.setModelText(String(card.name));
        descriptionLabel.setText(String(card.description), dontSendNotification);
        // set the author label text to "by {author}" only if {author} isn't empty
        card.author.empty() ? modelAuthorLabel.setAuthorText("")
                            : modelAuthorLabel.setAuthorText("by " + String(card.author));
        modelAuthorLabel.resized();
        // It is assumed we only support wav2wav or midi2midi models for now
        // if (card.midi_in && card.midi_out && ! card.author.empty())
        // {
        //     audioOrMidiLabel.setText("Midi-to-Midi", dontSendNotification);
        // }
        // else if (! card.midi_in && ! card.midi_out && ! card.author.empty())
        // {
        //     audioOrMidiLabel.setText("Wav-to-Wav", dontSendNotification);
        // }
        // else
        // {
        //     audioOrMidiLabel.setText("", dontSendNotification);
        // }
        // audioOrMidiLabel.setText("No need for that", dontSendNotification);
    }

    void setStatus(const ModelStatus& status)
    {
        juce::String statusName = std::string(magic_enum::enum_name(status)).c_str();
        statusBox->setStatusMessage("ModelStatus::" + statusName);
    }

    void setStatus(const juce::String& message) { statusBox->setStatusMessage(message); }

    void clearStatus() { statusBox->clearStatusMessage(); }

    void setInstructions(const juce::String& message) { instructionBox->setStatusMessage(message); }

    void clearInstructions() { instructionBox->clearStatusMessage(); }

private:
    // HARP UI
    std::unique_ptr<ModelStatusTimer> mModelStatusTimer { nullptr };

    ComboBox modelPathComboBox;
    // Two usefull variables to keep track of the selected item in the modelPathComboBox
    // and the item index of the last loaded model
    // These are used to restore the selected item in the modelPathComboBox
    // after a failed attempt to load a new model
    int lastSelectedItemIndex = -1;
    int lastLoadedModelItemIndex = -1;
    HoverHandler modelPathComboBoxHandler { modelPathComboBox };

    TextButton chooseFileButton { "Open File" };
    HoverHandler chooseFileButtonHandler { chooseFileButton };

    TextButton saveFileButton { "Save File" };
    HoverHandler saveFileButtonHandler { saveFileButton };

    ModelAuthorLabel modelAuthorLabel;

    MultiButton loadModelButton;
    MultiButton::Mode loadButtonInfo;

    MultiButton processCancelButton;
    MultiButton::Mode processButtonInfo;
    MultiButton::Mode cancelButtonInfo;

    MultiButton playStopButton;
    MultiButton::Mode playButtonInfo;
    MultiButton::Mode stopButtonInfo;

    // Label statusLabel;
    // A flag that indicates if the audio file can be saved
    bool saveEnabled = true;
    bool isProcessing = false;

    std::string customPath;
    ControlAreaWidget controlAreaWidget;
    TrackAreaWidget trackAreaWidget;

    // model card
    // Label nameLabel, authorLabel,
    Label descriptionLabel;
    // Label tagsLabel;
    // Label audioOrMidiLabel;
    // StatusComponent statusBox { 15.0f, juce::Justification::centred };
    // InstructionStatus instructionBox { 13.0f, juce::Justification::centredLeft };
    // std::shared_ptr<InstructionBox> instructionBox;
    // std::shared_ptr<StatusBox> statusBox;
    juce::SharedResourcePointer<InstructionBox> instructionBox;
    juce::SharedResourcePointer<StatusBox> statusBox;

    Time lastLoadTime;

    // the model itself
    std::shared_ptr<WebModel> model { new WebModel() };

    std::unique_ptr<FileChooser> openFileBrowser;
    std::unique_ptr<FileChooser> saveFileBrowser;

    // std::unique_ptr<MediaDisplayComponent> mediaDisplay;
    // std::unique_ptr<MediaDisplayComponent> outputMediaDisplay;
    // // A list of input media displays
    // std::vector<std::unique_ptr<MediaDisplayComponent>> inputMediaDisplays;
    // // A list of output media displays
    // std::vector<std::unique_ptr<MediaDisplayComponent>> outputMediaDisplays;

    std::unique_ptr<HoverHandler> mediaDisplayHandler;
    std::unique_ptr<HoverHandler> outputMediaDisplayHandler;

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

    std::shared_ptr<fontawesome::IconHelper> fontawesomeHelper;
    std::shared_ptr<fontaudio::IconHelper> fontaudioHelper;

    void play()
    {
        // if (! mediaDisplay->isPlaying())
        // {
        //     mediaDisplay->start();
        //     playStopButton.setMode(stopButtonInfo.label);
        // }
        // visit all the mediaDisplays and check each of them
        for (auto& display : trackAreaWidget.getInputMediaDisplays())
        {
            if (! display->isPlaying())
            {
                display->start();
            }
        }
        playStopButton.setMode(stopButtonInfo.label);
    }

    void stop()
    {
        // if (mediaDisplay->isPlaying())
        // {
        //     mediaDisplay->stop();
        //     playStopButton.setMode(playButtonInfo.label);
        // }
        for (auto& display : trackAreaWidget.getInputMediaDisplays())
        {
            if (display->isPlaying())
            {
                display->stop();
            }
        }
        playStopButton.setMode(playButtonInfo.label);
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
        // // Check if the source is one of the inputMediaDisplays
        // for (auto& display : trackAreaWidget.getInputMediaDisplays())
        // {
        //     if (source == display.get())
        //     {
        //         if (display->isFileDropped())
        //         {
        //             URL droppedFilePath = display->getDroppedFilePath();
        //             display->clearDroppedFile();
        //             // Reload an appropriate display for dropped file
        //             loadMediaDisplay(droppedFilePath.getLocalFile(), display);
        //         }
        //         else if (display->isFileLoaded() && !display->isPlaying())
        //         {
        //             playStopButton.setMode(playButtonInfo.label);
        //             playStopButton.setEnabled(true);
        //         }
        //         else if (display->isFileLoaded() && display->isPlaying())
        //         {
        //             playStopButton.setMode(stopButtonInfo.label);
        //         }
        //         else
        //         {
        //             playStopButton.setMode(playButtonInfo.label);
        //             playStopButton.setEnabled(false);
        //         }
        //         return;
        //     }
        // }

        // old

        // if (source == mediaDisplay.get())
        // {
        //     if (mediaDisplay->isFileDropped())
        //     {
        //         URL droppedFilePath = mediaDisplay->getDroppedFilePath();

        //         mediaDisplay->clearDroppedFile();

        //         // Reload an appropriate display for dropped file
        //         loadMediaDisplay(droppedFilePath.getLocalFile());
        //     }
        //     else if (mediaDisplay->isFileLoaded() && ! mediaDisplay->isPlaying())
        //     {
        //         playStopButton.setMode(playButtonInfo.label);
        //         playStopButton.setEnabled(true);
        //     }
        //     else if (mediaDisplay->isFileLoaded() && mediaDisplay->isPlaying())
        //     {
        //         playStopButton.setMode(stopButtonInfo.label);
        //     }
        //     else
        //     {
        //         playStopButton.setMode(playButtonInfo.label);
        //         playStopButton.setEnabled(false);
        //     }
        // }
        /*
        // The loadBroadcaster isn't used anymore. 
        // It's replaced by processLoadingResult
        // it's more usefull because I can pass the result of the loading
        // as argument to the callback
        // it'll be used like this:
        MessageManager::callAsync(
                                [this, loadingError]
                                {
                                    processLoadingResult(OpResult::fail(loadingError));
                                });
        */

        // else if (source == &loadBroadcaster)

        // The processBroadcaster should be also replaced in a similar way
        // as the loadBroadcaster
        if (source == &processBroadcaster)
        {
            return;
            // // refresh the display for the new updated file
            // URL tempFilePath = outputMediaDisplays[0]->getTempFilePath();
            // outputMediaDisplays[0]->updateDisplay(tempFilePath);

            // // extract generated labels from the model
            // LabelList& labels = model->getLabels();

            // // add the labels to the display component
            // outputMediaDisplays[0]->addLabels(labels);

            // // now, we can enable the process button
            // resetProcessingButtons();
            // return;
        }

        if (source == mModelStatusTimer.get())
        {
            // update the status label
            DBG("HARPProcessorEditor::changeListenerCallback: updating status label");
            // statusLabel.setText(model->getStatus(), dontSendNotification);
            setStatus(model->getStatus());
            return;
        }

        DBG("HARPProcessorEditor::changeListenerCallback: unhandled change broadcaster");
        return;
    }

    void processLoadingResult(OpResult result)
    {
        // return;
        if (result.wasOk())
        {
            setModelCard(model->card());
            controlAreaWidget.setModel(model);
            mModelStatusTimer->setModel(model);
            controlAreaWidget.populateControls();
            // inputTracksWidget.populateTracks();
            // outputTracksWidget.populateTracks();
            // trackAreaWidget.setModel(model);
            // trackAreaWidget.populateTracks();
            trackAreaWidget.setModel(model);
            // addAndMakeVisible(trackAreaWidget);
            trackAreaWidget.populateTracks();

            SpaceInfo spaceInfo = model->getGradioClient().getSpaceInfo();
            // juce::String spaceUrlButtonText;
            if (spaceInfo.status == SpaceInfo::Status::LOCALHOST)

            {
                // spaceUrlButton.setButtonText("open localhost in browser");
                // nameLabelButton.setURL(URL(spaceInfo.gradio));
                modelAuthorLabel.setURL(URL(spaceInfo.gradio));
            }
            else if (spaceInfo.status == SpaceInfo::Status::HUGGINGFACE)
            {
                // spaceUrlButton.setButtonText("open " + spaceInfo.userName + "/"
                //                              + spaceInfo.modelName + " in browser");
                // nameLabelButton.setURL(URL(spaceInfo.huggingface));
                modelAuthorLabel.setURL(URL(spaceInfo.huggingface));
            }
            else if (spaceInfo.status == SpaceInfo::Status::GRADIO)
            {
                // spaceUrlButton.setButtonText("open " + spaceInfo.userName + "-"
                //                              + spaceInfo.modelName + " in browser");
                // nameLabelButton.setURL(URL(spaceInfo.gradio));
                modelAuthorLabel.setURL(URL(spaceInfo.gradio));
            }
            // spaceUrlButton.setFont(Font(15.00f, Font::plain));
            addAndMakeVisible(modelAuthorLabel);
            // modelAuthorLabelHandler.onMouseEnter = [this]()
            // {
            //     setInstructions("Click to visit "
            //                     + model->getGradioClient().getSpaceInfo().getModelSlashUser()
            //                     + "\nin your browser");
            // };
            // modelAuthorLabelHandler.onMouseExit = [this]() { clearInstructions(); };
            // modelAuthorLabelHandler.attach();
        }

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
        repaint();
    }

    // void processProcessingResult(OpResult result)
    // {

    // }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
