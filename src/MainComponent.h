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
#include "MediaClipboardWidget.h"
#include "ThreadPoolJob.h"
#include "TrackAreaWidget.h"
#include "WebModel.h"

#include "gui/CustomPathDialog.h"
#include "gui/HoverHandler.h"
#include "gui/ModelAuthorLabel.h"
#include "gui/MultiButton.h"
#include "gui/StatusComponent.h"
#include "gui/TitledTextBox.h"
#include "client/Client.h"

#include "HarpLogger.h"
#include "external/magic_enum.hpp"
// #include "media/AudioDisplayComponent.h"
// #include "media/MediaDisplayComponent.h"
// #include "media/MidiDisplayComponent.h"

#include "AppSettings.h"
#include "settings/SettingsBox.h"
#include "windows/AboutWindow.h"
#include "utils.h"

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
        about = 0x2000,
        open = 0x2001,
        save = 0x2002,
        saveAs = 0x2003,
        undo = 0x2004,
        redo = 0x2005,
        loginHF = 0x2006,
        settings = 0x2007,
        loginStability = 0x2008,
        viewMediaClipboard = 0x3000
       
  
    };

    //StringArray getMenuBarNames() override
//{
    //DBG("getMenuBarNames() called");
   //return { "File" };
//}

        //loginHF = 0x2006,
        //loginStability = 0x2008,
        // settings = 0x2007,
       // viewMediaClipboard = 0x3000
    //};

    StringArray getMenuBarNames() override
    {
        StringArray menuBarNames;

        menuBarNames.add("File");
        // menuBarNames.add("Edit");
        menuBarNames.add("View");
        menuBarNames.add("Settings"); //added Settings menu

        return menuBarNames;
    }

    // In mac, we want the "about" command to be in the application menu ("HARP" tab)
    // For now, this is not used, as the extra commands appear grayed out
    std::unique_ptr<PopupMenu> getMacExtraMenu()
    {
        auto menu = std::make_unique<PopupMenu>();
        menu->addCommandItem(&commandManager, CommandIDs::about);
        //menu->addCommandItem(&commandManager, CommandIDs::settings);
        //menu->addCommandItem(&commandManager, CommandIDs::loginHF);
        //menu->addCommandItem(&commandManager, CommandIDs::loginStability);
        return menu;
    }

    PopupMenu getMenuForIndex([[maybe_unused]] int menuIndex, const String& menuName) override
    {
        PopupMenu menu;

        if (menuName == "File")
        {
            menu.addCommandItem(&commandManager, CommandIDs::open);
            //menu.addCommandItem(&commandManager, CommandIDs::save);
            menu.addCommandItem(&commandManager, CommandIDs::saveAs);
            //menu.addCommandItem(&commandManager, CommandIDs::undo);
            //menu.addCommandItem(&commandManager, CommandIDs::redo);
            menu.addSeparator();
            // menu.addCommandItem (&commandManager, CommandIDs::settings);
            // menu.addSeparator()
           // menu.addCommandItem(&commandManager, CommandIDs::about);
            menu.addSeparator();
            //menu.addCommandItem(&commandManager, CommandIDs::loginHF);
            //menu.addCommandItem(&commandManager, CommandIDs::loginStability);
        }
        else if (menuName == "View")
        {
            menu.addCommandItem(&commandManager, CommandIDs::viewMediaClipboard);
        }
        else if (menuName == "Settings")
        {
            menu.addCommandItem(&commandManager, CommandIDs::settings);
            menu.addSeparator();
            menu.addCommandItem(&commandManager, CommandIDs::about);
            menu.addCommandItem(&commandManager, CommandIDs::loginHF);
            menu.addCommandItem(&commandManager, CommandIDs::loginStability);

        }

        return menu;
    }

    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override
    {
        DBG("menuItemID: " << menuItemID);
        DBG("topLevelMenuIndex: " << topLevelMenuIndex);
    }

    ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }

    // Fills the commands array with the commands that this component/target supports
    void getAllCommands(Array<CommandID>& commands) override
    {
        const CommandID ids[] = {
            CommandIDs::open,  CommandIDs::save,           CommandIDs::saveAs,
            CommandIDs::undo,  CommandIDs::redo,           CommandIDs::loginHF,
            CommandIDs::about, CommandIDs::loginStability, CommandIDs::viewMediaClipboard,
            CommandIDs::settings 
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
            case CommandIDs::loginHF:
                result.setInfo(
                    "Login to Hugging Face", "Authenticate with a Hugging Face token", "Login", 0);
                break;
            case CommandIDs::loginStability:
                result.setInfo(
                    "Login to Stability AI", "Authenticate with a Stability AI token", "Login", 0);
                break;
            case CommandIDs::viewMediaClipboard:
                result.setInfo("Media Clipboard", "Toggles display of media clipboard", "View", 0);
                result.setTicked(showMediaClipboard);
                break;
            case CommandIDs::about:
                result.setInfo("About HARP", "Shows information about the application", "About", 0);
                break;
            case CommandIDs::settings:
                result.setInfo("Preferences...", "Open the settings window", "Settings", 0);
                break;
        }
    }

    bool perform(const InvocationInfo& info) override
    {
        DBG("perform() called");
        switch (info.commandID)
        {
            case CommandIDs::open:
                DBG("Open command invoked");
                openFileChooser();
                break;
            /*case CommandIDs::save:
                DBG("Save command invoked");
                saveCallback();
                break;*/
            case CommandIDs::saveAs:
                DBG("Save As command invoked");
                mediaClipboardWidget.saveFileCallback();
                break;
            case CommandIDs::undo:
                DBG("Undo command invoked");
                undoCallback();
                break;
            case CommandIDs::redo:
                DBG("Redo command invoked");
                redoCallback();
                break;
            case CommandIDs::loginHF:
                DBG("Login command invoked");
                loginToProvider("huggingface");
                break;
            case CommandIDs::about:
                DBG("About command invoked");
                showAboutDialog();
                break;
            case CommandIDs::settings:
                DBG("Settings command invoked");
                showSettingsDialog();
                break;
            case CommandIDs::loginStability:
                DBG("Login to Stability command invoked");
                loginToProvider("stability");
                break;
            case CommandIDs::viewMediaClipboard:
                DBG("ViewMediaClipboard command invoked");
                viewMediaClipboardCallback();
                break;
            default:
                return false;
        }
        return true;
    }

    void showAboutDialog()
    {
        auto aboutComponent = std::make_unique<AboutWindow>();

        DialogWindow::LaunchOptions dialog;
        dialog.content.setOwned(aboutComponent.release());
        dialog.dialogTitle = "About " + String(APP_NAME);
        dialog.dialogBackgroundColour = Colours::grey;
        dialog.escapeKeyTriggersCloseButton = true;
        dialog.useNativeTitleBar = true;
        dialog.resizable = false;

        dialog.launchAsync();
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

        if (isProcessing)
        {
            DBG("Can't undo while processing occurring!");
            juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
            return;
        }

        // Iterate over all inputMediaDisplays and call the iteratePreviousTempFile()
        auto& inputMediaDisplays = inputTrackAreaWidget.getMediaDisplays();

        /*for (auto& inputMediaDisplay : inputMediaDisplays)
        {
            if (! inputMediaDisplay->iteratePreviousTempFile())
            {
                DBG("Nothing to undo!");
                // juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
            }
            else
            {
                saveEnabled = true;
                DBG("Undo callback completed successfully");
            }
        }*/
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

        if (isProcessing)
        {
            DBG("Can't redo while processing occurring!");
            juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
            return;
        }

        // Iterate over all inputMediaDisplays and call the iterateNextTempFile()
        auto& inputMediaDisplays = inputTrackAreaWidget.getMediaDisplays();

        /*for (auto& inputMediaDisplay : inputMediaDisplays)
        {
            if (! inputMediaDisplay->iterateNextTempFile())
            {
                DBG("Nothing to redo!");
                // juce::LookAndFeel::getDefaultLookAndFeel().playAlertSound();
            }
            else
            {
                saveEnabled = true;
                DBG("Redo callback completed successfully");
            }
        }*/
    }

    void tryLoadSavedToken()
    {
        if (model == nullptr || model->getStatus() == ModelStatus::INITIALIZED)
            return;

        auto& client = model->getClient();
        const auto spaceInfo = client.getSpaceInfo();

        if (spaceInfo.status == SpaceInfo::Status::GRADIO
            || spaceInfo.status == SpaceInfo::Status::HUGGINGFACE)
        {
            auto token = AppSettings::getString("huggingFaceToken", "");
            if (! token.isEmpty())
            {
                client.setToken(token);
                setStatus("Applied saved Hugging Face token.");
            }
        }
        else if (spaceInfo.status == SpaceInfo::Status::STABILITY)
        {
            auto token = AppSettings::getString("stabilityToken", "");
            if (! token.isEmpty())
            {
                client.setToken(token);
                setStatus("Applied saved Stability token.");
            }
        }
    }  
    
   
    void loginToProvider(const juce::String& providerName)
    {
        bool isHuggingFace = (providerName == "huggingface");
        bool isStability = (providerName == "stability");

        if (! isHuggingFace && ! isStability)
        {
            DBG("Invalid provider name passed to loginToProvider()");
            return;
        }

        // Set provider-specific values
        juce::String title =
            "Login to " + juce::String(isHuggingFace ? "Hugging Face" : "Stability AI");
        juce::String message =
            "Paste your "
            + juce::String(isHuggingFace ? "Hugging Face access token" : "Stability AI API token")
            + " below.\n\nClick 'Get Token' to open the token page.";
        juce::String tokenLabel = isHuggingFace ? "Access Token:" : "API Token:";
        juce::String tokenURL = isHuggingFace ? "https://huggingface.co/settings/tokens"
                                              : "https://platform.stability.ai/account/keys";
        juce::String storageKey = isHuggingFace ? "huggingFaceToken" : "stabilityToken";

        // Create token prompt window
        auto* prompt = new juce::AlertWindow(title, message, juce::AlertWindow::NoIcon);
        prompt->addTextEditor("token", "", tokenLabel);
        prompt->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
        prompt->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        prompt->addButton("Get Token", 2);

        auto rememberCheckbox = std::make_unique<juce::ToggleButton>();
        rememberCheckbox->setButtonText("Remember this token");
        rememberCheckbox->setSize(200, 24);
        prompt->addCustomComponent(rememberCheckbox.get());

        prompt->enterModalState(
            true,
            juce::ModalCallbackFunction::create(
                [this,
                 prompt,
                 rememberCheckboxPtr = std::move(rememberCheckbox),
                 isHuggingFace,
                 isStability,
                 storageKey,
                 tokenURL](int choice)
                {
                    if (choice == 1)
                    {
                        auto token = prompt->getTextEditor("token")->getText().trim();
                        if (token.isNotEmpty())
                        {
                            // Validate token
                            OpResult result = isHuggingFace
                                                  ? GradioClient().validateToken(token)
                                                  : StabilityClient().validateToken(token);

                            if (result.failed())
                            {
                                Error err = result.getError();
                                Error::fillUserMessage(err);
                                LogAndDBG("Invalid token:\n" + err.devMessage.toStdString());
                                AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                                                 "Invalid Token",
                                                                 "The provided token is invalid:\n"
                                                                     + err.userMessage);
                                setStatus("Invalid token.");
                            }
                            else
                            {
                                if (rememberCheckboxPtr->getToggleState())
                                {
                                    AppSettings::setValue(storageKey, token);
                                    AppSettings::saveIfNeeded();
                                    setStatus(
                                        juce::String(isHuggingFace ? "Hugging Face" : "Stability")
                                        + " token saved.");
                                }
                                else
                                {
                                    AppSettings::removeValue(storageKey);
                                    setStatus("Token accepted but not saved.");
                                }

                                // Apply to model if appropriate model is already loaded
                                if (model != nullptr
                                    && model->getStatus() != ModelStatus::INITIALIZED)
                                {
                                    auto& client = model->getClient();
                                    const auto spaceInfo = client.getSpaceInfo();

                                    if ((isHuggingFace
                                         && spaceInfo.status == SpaceInfo::Status::GRADIO)
                                        || (isStability
                                            && spaceInfo.status == SpaceInfo::Status::STABILITY))
                                    {
                                        client.setToken(token);
                                        setStatus("Token applied to loaded model.");
                                    }
                                }
                            }
                        }
                        else
                        {
                            setStatus("No token entered.");
                        }
                    }
                    else if (choice == 2)
                    {
                        juce::URL(tokenURL).launchInDefaultBrowser();
                        loginToProvider(isHuggingFace ? "huggingface"
                                                      : "stability"); // Reopen prompt
                    }
                    else
                    {
                        setStatus("Login cancelled.");
                    }

                    delete prompt;
                }),
            false);
    }

    void loadModelCallback()
    {
        // Get the URL/path the user provided in the comboBox
        std::string pathURL;
        int selectedId = modelPathComboBox.getSelectedId();
        if (selectedId == 1) // index 0 is "custom path..."
            pathURL = customPath;
        else if (modelPathIdToURL.count(selectedId) > 0)
            pathURL = modelPathIdToURL[selectedId];
        else
            pathURL = modelPathComboBox.getItemText(selectedId - 1).toStdString();

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

                            //debugging
                            Component* comp = Component::getCurrentlyFocusedComponent();
                            DBG("Currently focused component: " + (comp ? comp->getName() : "none"));
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
                            SpaceInfo spaceInfo = model->getClient().getSpaceInfo();
                            URL spaceUrl;
                            
                            if (spaceInfo.status == SpaceInfo::Status::GRADIO)
                            {
                                URL spaceUrl = this->model->getClient().getSpaceInfo().gradio;
                                spaceUrl.launchInDefaultBrowser();
                            }
                            else if (spaceInfo.status == SpaceInfo::Status::HUGGINGFACE)
                            {
                                URL spaceUrl = this->model->getClient().getSpaceInfo().huggingface;
                                spaceUrl.launchInDefaultBrowser();
                            }
                            else if (spaceInfo.status == SpaceInfo::Status::LOCALHOST)
                            {
                                // either choose hugingface or gradio, they are the same
                                URL spaceUrl = this->model->getClient().getSpaceInfo().huggingface;
                                spaceUrl.launchInDefaultBrowser();
                            }
                            else if (!customPath.empty())
                                    spaceUrl = URL(customPath);

                                //  Prevent crash: check before launching
                                if (spaceUrl.isWellFormed() && !spaceUrl.isEmpty())
                                {
                                    spaceUrl.launchInDefaultBrowser();
                                }
                                else
                                {
                                    AlertWindow::showMessageBoxAsync(
                                        AlertWindow::WarningIcon,
                                        "Invalid Space URL",
                                        "The space appears to be sleeping, and no valid URL is available.\n"
                                        "Please wake it on Hugging Face manually and try again."
                                    );
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
                        if (loadingError.userMessage.containsIgnoreCase("sleeping"))
                        {
                             MessageManager::callAsync([this] {
                            addCustomPathToDropdown(customPath, true); // mark as sleeping
                            });
                        }
                        //NEW: reopen custom path dialog if sleeping or 404
                        if (loadingError.type == ErrorType::InvalidURL || 
                            loadingError.devMessage.contains("404") ||
                            loadingError.userMessage.containsIgnoreCase("sleeping"))
                        {
                            MessageManager::callAsync([this] {
                                openCustomPathDialog(customPath);
                            });
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
    void openCustomPathDialog(const std::string& prefillPath = "")
    {
        std::function<void(const juce::String&)> loadCallback =
         [this](const juce::String& customPath2)
        {
             this->customPath = customPath2.toStdString();
             loadModelButton.triggerClick(); // Trigger load
         };

        std::function<void()> cancelCallback = [this]()
        {
            if (lastLoadedModelItemIndex != -1)
                 modelPathComboBox.setSelectedId(lastLoadedModelItemIndex + 1);
            else if (lastSelectedItemIndex != -1)
                 modelPathComboBox.setSelectedId(lastSelectedItemIndex + 1);
            else
                 resetModelPathComboBox();
         };

        CustomPathDialog* dialog = new CustomPathDialog(loadCallback, cancelCallback);
        if (!prefillPath.empty())
             dialog->setTextFieldValue(prefillPath); 
        }

    void viewMediaClipboardCallback()
    {
        // Toggle media clipboard visibility state
        showMediaClipboard = ! showMediaClipboard;

        // Find top-level window for resizing
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
        {
            // Determine which display contains HARP
            auto* currentDisplay =
                juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds());

            int currentDisplayWidth;

            if (currentDisplay != nullptr)
            {
                if (window->isFullScreen())
                {
                    currentDisplayWidth = currentDisplay->totalArea.getWidth();
                }
                else
                {
                    currentDisplayWidth = currentDisplay->userArea.getWidth();
                }
            }

            //int totalDesktopWidth = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getBounds())->totalArea.getWidth();

            // Get current bounds of top-level window
            Rectangle<int> windowBounds = window->getBounds();

            if (showMediaClipboard)
            {
                // Scale bounds to extend window by 40% of main width
                windowBounds.setWidth(
                    jmin(currentDisplayWidth, static_cast<int>(1.4 * windowBounds.getWidth())));
            }
            else
            {
                if (! window->isFullScreen())
                {
                    // Scale bounds to reduce window to main width
                    windowBounds.setWidth(static_cast<int>(windowBounds.getWidth() / 1.4));
                }
            }

            // Set extended or reduced bounds
            window->setBounds(windowBounds);
        }

        // Add view preference to persistent settings
        AppSettings::setValue("showMediaClipboard", showMediaClipboard ? "1" : "0");
        AppSettings::saveIfNeeded();

        // Send status message to add check to file menu
        commandManager.commandStatusChanged();

        resized();
    }

    void resetModelPathComboBox()
    {
        modelPathIdToURL.clear(); // clear stored raw URLs
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

    // Adds a path to the model dropdown if it's not already present
    void addCustomPathToDropdown(const std::string& path, bool wasSleeping = false)
    {
        juce::String displayStr(path);
        if (wasSleeping)
            displayStr += " (sleeping)";

        // Check against stored raw URLs instead of display strings
        bool alreadyExists = false;
        for (const auto& [id, storedPath] : modelPathIdToURL)
        {
            if (juce::String(storedPath).equalsIgnoreCase(juce::String(path)))
            {
                alreadyExists = true;
                break;
            }

        }

        if (!alreadyExists)
        {
            int newID = modelPathComboBox.getNumItems() + 1;
            modelPathComboBox.addItem(displayStr, newID);
            modelPathIdToURL[newID] = path; // store raw path separately
        }

        modelPathComboBox.setText(displayStr, juce::dontSendNotification);
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

    void showSettingsDialog()
    {
         DBG("Settings command invoked");

        juce::DialogWindow::LaunchOptions options;
        options.dialogTitle = "Settings";
        options.content.setOwned(new SettingsBox());
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.escapeKeyTriggersCloseButton = true;
        options.dialogBackgroundColour = juce::Colours::lightgrey;
        options.launchAsync();
    }

    void initMenuBar()
    {
        // init the menu bar
        menuBar.reset(new MenuBarComponent(this));
        addAndMakeVisible(menuBar.get());
        setApplicationCommandManagerToWatch(&commandManager);
        // Register commands
        commandManager.registerAllCommandsForTarget(this);
        commandManager.setFirstCommandTarget(this);

        // commandManager.setFirstCommandTarget(this);
        addKeyListener(commandManager.getKeyMappings());

#if JUCE_MAC
        // Not used for now
        //auto extraMenu = getMacExtraMenu();
       // MenuBarModel::setMacMainMenu(this);
       auto macExtraMenu = getMacExtraMenu();
       MenuBarModel::setMacMainMenu(this, macExtraMenu.get());
#endif

        menuBar->setVisible(true);
        menuItemsChanged();
    }

    /*
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
    }*/

    void initProcessCancelButton()
    {
        // The Process/Cancel button
        processButtonInfo = MultiButton::Mode {
            "Process",
            [this] { processCallback(); },
            Colours::orangered,
            "Click to send the input for processing",
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
            // "Load Model",
            "Load",
            [this] { loadModelCallback(); },
            Colours::lightgrey,
            "Click to load the selected model path",
            MultiButton::DrawingMode::TextOnly
            // MultiButton::DrawingMode::IconOnly,
            // fontawesome::Download,
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
            // "hugggof/vampnet-music",  "lllindsey0615/pyharp_demucs",
            // "lllindsey0615/pyharp_AMT", "npruyne/timbre-trap",    "xribene/harmonic_percussive_v5",
            // "lllindsey0615/DEMUCS_GPU", "cwitkowitz/timbre-trap",
            // "npruyne/audio_similarity",
            // "xribene/pitch_shifter",
            // "xribene/midi_pitch_shifter",
            // "xribene/pitch_shifter_slow",
            "stability/text-to-audio",
            "stability/audio-to-audio",
            "lllindsey0615/text2midi-HARP3",
            "lllindsey0615/demucs-gen-input-output-harp-v3",
            "lllindsey0615/solo-piano-audio-to-midi-transcription",
            "lllindsey0615/AMT_HARP3",
            "lllindsey0615/vampnet-music-HARP-V3",
            "lllindsey0615/harmonic-percussive-HARP-3",
            // "xribene/HARP-UI-TEST-v3"
            // "http://localhost:7860",
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
                    [this](const juce::String& customPath2)
                {
                    DBG("Custom path entered: " + customPath2);
                    this->customPath = customPath2.toStdString(); // Store the custom path
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
                new CustomPathDialog(loadCallback, cancelCallback);
            }
            else
            {
                lastSelectedItemIndex = modelPathComboBox.getSelectedItemIndex();
            }
            loadModelButton.setEnabled(true);
        };

        addAndMakeVisible(modelPathComboBox);
    }

    // explicit MainComponent(const URL& initialFilePath = URL()) : jobsFinished(0), totalJobs(0)
    explicit MainComponent() //: jobsFinished(0), totalJobs(0)
    //   jobProcessorThread(customJobs, jobsFinished, totalJobs, processBroadcaster)
    {
        HarpLogger::getInstance()->initializeLogger();
        fontaudioHelper = std::make_shared<fontaudio::IconHelper>();
        fontawesomeHelper = std::make_shared<fontawesome::IconHelper>();

        // initSomeButtons();
        // initPlayStopButton();

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

        showMediaClipboard = AppSettings::getBoolValue("showMediaClipboard", false);

        initProcessCancelButton();

        initLoadModelButton();

        // add a status timer to update the status label periodically
        mModelStatusTimer = std::make_unique<ModelStatusTimer>(model);
        mModelStatusTimer->addChangeListener(this);
        mModelStatusTimer->startTimer(50); // 100 ms interval

        initModelPathComboBox();

        addAndMakeVisible(mediaClipboardWidget);

        // model controls
        controlAreaWidget.setModel(model);
        addAndMakeVisible(controlAreaWidget);
        controlAreaWidget.populateControls();

        inputTracksLabel.setJustificationType(juce::Justification::centred);
        inputTracksLabel.setFont(juce::Font(20.0f, juce::Font::bold));
        addAndMakeVisible(inputTracksLabel);

        outputTracksLabel.setJustificationType(juce::Justification::centred);
        outputTracksLabel.setFont(juce::Font(20.0f, juce::Font::bold));
        addAndMakeVisible(outputTracksLabel);

        populateTracks();
        addAndMakeVisible(inputTrackAreaWidget);
        addAndMakeVisible(outputTrackAreaWidget);

        addAndMakeVisible(descriptionLabel);
        // addAndMakeVisible(tagsLabel);
        // addAndMakeVisible(audioOrMidiLabel);

        addAndMakeVisible(statusBox);
        addAndMakeVisible(instructionBox);

        // model card component
        // Get the modelCard from the EditorView
        auto& card = model->card();
        setModelCard(card);

        // jobProcessorThread.startThread();
        //tryLoadSavedToken();

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

        // jobProcessorThread.signalThreadShouldExit();
        // This will not actually run any processing task
        // It'll just make sure that the thread is not waiting
        // and it'll allow it to check for the threadShouldExit flag
        // jobProcessorThread.signalTask();
        // jobProcessorThread.waitForThreadToExit(-1);

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
            LogAndDBG(cancelResult.getError().devMessage.toStdString());
            AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                             "Cancel Error",
                                             "An error occurred while cancelling the processing: \n"
                                                 + cancelResult.getError().devMessage);
            return;
        }
        // Update current process to empty
        processMutex.lock();
        DBG("Cancel ProcessID: " + currentProcessID);
        currentProcessID = "";
        processMutex.unlock();
        // We already added a temp file, so we need to undo that
        // TODO: this is functionality that I need to add back // #TODO
        // mediaDisplay->iteratePreviousTempFile();
        // mediaDisplay->clearFutureTempFiles();

        // processCancelButton.setEnabled(false); // this is the og v3
        resetProcessingButtons(); // This is the new way


    }


    void processCallback()
    {
        // return;
        // if (dynamic_cast<AudioDisplayComponent*>(mediaDisplay.get()))
        // // check if the file is loaded
        // if (! mediaDisplay->isFileLoaded())
        // {
        //     String fileTypeString;

        //     if (model->card().midi_in)
        //     {
        //         fileTypeString = "midi";
        //     }
        //     else
        //     {
        //         fileTypeString = "audio";
        //     }

        //     AlertWindow::showMessageBoxAsync(
        //         AlertWindow::WarningIcon,
        //         "Error",
        //         fileTypeString.substring(0, 1).toUpperCase()
        //         + fileTypeString.substring(1).toLowerCase()
        //         + " file is not loaded. Please load "
        //         + fileTypeString + " file first.");
        //     return;
        // }


        if (model == nullptr)
        {
            AlertWindow("Error",
                        "Model is not loaded. Please load a model first.",
                        AlertWindow::WarningIcon);
            return;
        }

        // Get new processID
        String processID = juce::Uuid().toString();
        processMutex.lock();
        currentProcessID = processID;
        DBG("Set Process ID: " + processID);
        processMutex.unlock();

        processCancelButton.setEnabled(true);
        processCancelButton.setMode(cancelButtonInfo.label);
        loadModelButton.setEnabled(false);
        modelPathComboBox.setEnabled(false);
        saveEnabled = false;
        isProcessing = true;

        // mediaDisplay->addNewTempFile();
        auto& inputMediaDisplays = inputTrackAreaWidget.getMediaDisplays();

        // Get all the getTempFilePaths from the inputMediaDisplays
        // and store them in a map/dictionary with the track name as the key
        std::vector<std::tuple<Uuid, String, File>> localInputTrackFiles;
        for (auto& inputMediaDisplay : inputMediaDisplays)
        {
            if (! inputMediaDisplay->isFileLoaded() && inputMediaDisplay->isRequired())
            {
                AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                                 "Error",
                                                 "Input file is not loaded for track "
                                                     + inputMediaDisplay->getTrackName()
                                                     + ". Please load an input file first.");
                processCancelButton.setMode(processButtonInfo.label);
                isProcessing = false;
                saveEnabled = true;
                loadModelButton.setEnabled(true);
                modelPathComboBox.setEnabled(true);
                return;
            }
            if (inputMediaDisplay->isFileLoaded())
            {
                //inputMediaDisplay->addNewTempFile();
                localInputTrackFiles.push_back(
                    std::make_tuple(inputMediaDisplay->getDisplayID(),
                                    inputMediaDisplay->getTrackName(),
                                    //inputMediaDisplay->getTempFilePath().getLocalFile()));
                                    inputMediaDisplay->getOriginalFilePath().getLocalFile()));
            }
        }

        // Directly add the job to the thread pool
        jobProcessorThread.addJob(
            new CustomThreadPoolJob(
                [this, localInputTrackFiles](String jobProcessID) { // &jobsFinished, totalJobs
                    // Individual job code for each iteration
                    // copy the audio file, with the same filename except for an added _harp to the stem
                    OpResult processingResult =
                        model->process(localInputTrackFiles);
                    processMutex.lock();
                    if (jobProcessID != currentProcessID)
                    {
                        DBG("ProcessID " + jobProcessID + " not found");
                        DBG("NumJobs: " + std::to_string(jobProcessorThread.getNumJobs()));
                        DBG("NumThrds: " + std::to_string(jobProcessorThread.getNumThreads()));
                        processMutex.unlock();
                        return;
                    }
                    if (processingResult.failed())
                    {
                        Error processingError = processingResult.getError();
                        Error::fillUserMessage(processingError);
                        LogAndDBG("Error in Processing:\n"
                                  + processingError.devMessage.toStdString());
                        AlertWindow::showMessageBoxAsync(
                            AlertWindow::WarningIcon,
                            "Processing Error",
                            "An error occurred while processing the audio file: \n"
                                + processingError.userMessage);
                        // cb: I commented this out, and it doesn't seem to change anything
                        // it was also causing a crash. If we need it, it needs to run on
                        // the message thread using MessageManager::callAsync
                        // hy: Now this line works.
                        // resetProcessingButtons();
                        // cb: Needs to be in the message thread or else it crashes
                        // It's used when the processing fails to reset the process/cancel
                        // button back to the process mode.
                        MessageManager::callAsync([this] { resetProcessingButtons(); });
                        processMutex.unlock();
                        return;
                    }
                    // load the audio file again
                    DBG("ProcessID " + jobProcessID + " succeed");
                    currentProcessID = "";
                    model->setStatus(ModelStatus::FINISHED);
                    processBroadcaster.sendChangeMessage();
                    processMutex.unlock();
                },
                processID),
            true);
        DBG("NumJobs: " + std::to_string(jobProcessorThread.getNumJobs()));
        DBG("NumThrds: " + std::to_string(jobProcessorThread.getNumThreads()));
    }

    /*
    Entry point for importing new files into the application.
    */
    void importNewFile(File mediaFile, bool fromDAW = false)
    {
        mediaClipboardWidget.addTrackFromFilePath(URL(mediaFile), fromDAW);

        if (! showMediaClipboard)
        {
            viewMediaClipboardCallback();
        }
    }
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
    // }

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
        StringArray validExtensions = MediaDisplayComponent::getSupportedExtensions();
        String filePatternsAllowed = "*" + validExtensions.joinIntoString(";*");

        openFileBrowser =
            std::make_unique<FileChooser>("Select a media file...", File(), filePatternsAllowed);

       
            openFileBrowser->launchAsync(FileBrowserComponent::openMode
                                         | FileBrowserComponent::canSelectFiles,
                                        [this](const FileChooser& browser)
                                        {
                                            File chosenFile = browser.getResult();
                                            if (chosenFile != File {})
                                            {
                                                importNewFile(chosenFile);
                                            }
                                        });
    }

    void paint(Graphics& g) override
    {
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        auto mainArea = getLocalBounds();

#if not JUCE_MAC
        menuBar->setBounds(
            mainArea.removeFromTop(LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));
#endif
       auto margin = 2; // Adjusted margin value for top and bottom spacing

        juce::FlexBox fullWindow;
        fullWindow.flexDirection = juce::FlexBox::Direction::row;

        juce::FlexBox mainPanel;
        mainPanel.flexDirection = juce::FlexBox::Direction::column;
        mainPanel.alignContent = juce::FlexBox::AlignContent::flexStart;
        mainPanel.alignItems = juce::FlexBox::AlignItems::stretch;
        mainPanel.justifyContent = juce::FlexBox::JustifyContent::flexStart;

        // Row 1: Model Path ComboBox and Load Model Button
        juce::FlexBox row1;
        row1.flexDirection = juce::FlexBox::Direction::row;
        row1.items.add(juce::FlexItem(modelPathComboBox).withFlex(8).withMargin(margin));
        row1.items.add(juce::FlexItem(loadModelButton).withFlex(1).withMargin(margin));
        mainPanel.items.add(juce::FlexItem(row1).withHeight(30));

        // Row 2: ModelName / AuthorName Labels
        juce::FlexBox row2;
        row2.flexDirection = juce::FlexBox::Direction::row;
        row2.items.add(juce::FlexItem(modelAuthorLabel).withFlex(0.5).withMargin(margin));
        row2.items.add(juce::FlexItem().withFlex(0.5).withMargin(margin));
        mainPanel.items.add(juce::FlexItem(row2).withHeight(30).withMargin(margin));

        // Row 3: Description
        auto font = Font(15.0f);
        descriptionLabel.setFont(font);
        // descriptionLabel.setColour(Label::backgroundColourId, Colours::red);
        auto maxLabelWidth = mainArea.getWidth() - 2 * margin;
        auto numberOfLines =
            font.getStringWidthFloat(descriptionLabel.getText(false)) / maxLabelWidth;
        float textHeight =
            (font.getHeight() + 5) * (std::floor(numberOfLines) + 1) + font.getHeight();
        mainPanel.items.add(
            juce::FlexItem(descriptionLabel).withHeight(textHeight).withMargin(margin));

        // Row 4: Control Area Widget
        // TODO - set min/max height based on limits of control element scaling
        mainPanel.items.add(juce::FlexItem(controlAreaWidget).withFlex(1).withMargin(margin));

        // Row 5: Process Cancel Button
        juce::FlexBox rowProcessCancelButton;
        rowProcessCancelButton.flexDirection = juce::FlexBox::Direction::row;
        rowProcessCancelButton.justifyContent = juce::FlexBox::JustifyContent::center;
        rowProcessCancelButton.items.add(juce::FlexItem().withFlex(1));
        rowProcessCancelButton.items.add(
            juce::FlexItem(processCancelButton).withWidth(150).withMargin(margin));
        rowProcessCancelButton.items.add(juce::FlexItem().withFlex(1));
        mainPanel.items.add(
            juce::FlexItem(rowProcessCancelButton).withHeight(30).withMargin(margin));

        // Row 6: Input Tracks Area Widget
        float numInputTracks = inputTrackAreaWidget.getNumTracks();
        float numOutputTracks = outputTrackAreaWidget.getNumTracks();
        float totalTracks = numInputTracks + numOutputTracks;

        if (numInputTracks > 0)
        {
            float inputTrackAreaFlex = 4 * (numInputTracks / totalTracks);
            mainPanel.items.add(juce::FlexItem(inputTracksLabel).withHeight(20).withMargin(margin));
            mainPanel.items.add(
                juce::FlexItem(inputTrackAreaWidget).withFlex(inputTrackAreaFlex).withMargin(margin));
        }
        else
        {
            inputTracksLabel.setBounds(0, 0, 0, 0);
            inputTrackAreaWidget.setBounds(0, 0, 0, 0);
        }

        // Row 7: Output Tracks Area Widget
        if (numOutputTracks > 0)
        {
            float outputTrackAreaFlex = 4 * (numOutputTracks / totalTracks);
            mainPanel.items.add(juce::FlexItem(outputTracksLabel).withHeight(20).withMargin(margin));
            mainPanel.items.add(
                juce::FlexItem(outputTrackAreaWidget).withFlex(outputTrackAreaFlex).withMargin(margin));
        }
        else
        {
            outputTracksLabel.setBounds(0, 0, 0, 0);
            outputTrackAreaWidget.setBounds(0, 0, 0, 0);
        }

        // Row 8: Instructions Area and Status Area
        juce::FlexBox row8;
        row8.flexDirection = juce::FlexBox::Direction::row;
        row8.items.add(juce::FlexItem(*instructionBox).withFlex(1).withMargin(margin));
        row8.items.add(juce::FlexItem(*statusBox).withFlex(1).withMargin(margin));
        // TODO - fix maximum height?
        mainPanel.items.add(juce::FlexItem(row8).withFlex(0.4f));

        fullWindow.items.add(juce::FlexItem(mainPanel).withFlex(1));

        // Right Column: Media Clipboard Area
        if (showMediaClipboard)
        {
            fullWindow.items.add(juce::FlexItem(mediaClipboardWidget).withFlex(0.4));
        }
        else
        {
            mediaClipboardWidget.setBounds(0, 0, 0, 0);
        }

        // Apply the FlexBox layout to the full area
        fullWindow.performLayout(mainArea);
    }

    void resetUI()
    {
        controlAreaWidget.resetUI();
        inputTrackAreaWidget.resetUI();
        outputTrackAreaWidget.resetUI();
        // Also clear the model card components
        ModelCard empty;
        setModelCard(empty);
        // modelAuthorLabelHandler.detach();
    }

    void setModelCard(const ModelCard& card)
    {
        modelAuthorLabel.setModelText(String(card.name));
        descriptionLabel.setText(String(card.description), dontSendNotification);
        // set the author label text to "by {author}" only if {author} isn't empty
        card.author.empty() ? modelAuthorLabel.setAuthorText("")
                            : modelAuthorLabel.setAuthorText("by " + String(card.author));
        modelAuthorLabel.resized();
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

    ApplicationCommandManager commandManager;
    // MenuBar
    std::unique_ptr<MenuBarComponent> menuBar;

    std::unique_ptr<ModelStatusTimer> mModelStatusTimer { nullptr };

    ComboBox modelPathComboBox;

    std::string customPath;

    // Two usefull variables to keep track of the selected item in the modelPathComboBox
    // and the item index of the last loaded model
    // These are used to restore the selected item in the modelPathComboBox
    // after a failed attempt to load a new model
    int lastSelectedItemIndex = -1;
    int lastLoadedModelItemIndex = -1;
    HoverHandler modelPathComboBoxHandler { modelPathComboBox };

    // TextButton chooseFileButton { "Open File" };
    // HoverHandler chooseFileButtonHandler { chooseFileButton };

    // TextButton saveFileButton { "Save File" };
    // HoverHandler saveFileButtonHandler { saveFileButton };

    ModelAuthorLabel modelAuthorLabel;
    MultiButton loadModelButton;
    MultiButton::Mode loadButtonInfo;

    // model card
    // Label nameLabel, authorLabel,
    Label descriptionLabel;
    // Label tagsLabel;
    // Label audioOrMidiLabel;

    MultiButton processCancelButton;
    MultiButton::Mode processButtonInfo;
    MultiButton::Mode cancelButtonInfo;

    //MultiButton playStopButton;
    //MultiButton::Mode playButtonInfo;
    //MultiButton::Mode stopButtonInfo;

    // Label statusLabel;
    // A flag that indicates if the audio file can be saved
    bool saveEnabled = true;
    bool isProcessing = false;

    ControlAreaWidget controlAreaWidget;

    Label inputTracksLabel { "Input Tracks", "Input Tracks" };
    TrackAreaWidget inputTrackAreaWidget { DisplayMode::Input };

    Label outputTracksLabel { "Output Tracks", "Output Tracks" };
    TrackAreaWidget outputTrackAreaWidget { DisplayMode::Output };

    MediaClipboardWidget mediaClipboardWidget;

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

    // std::unique_ptr<HoverHandler> mediaDisplayHandler;
    // std::unique_ptr<HoverHandler> outputMediaDisplayHandler;

    String currentProcessID;
    std::mutex processMutex;

    /// CustomThreadPoolJob
    // This one is used for Loading the models
    // The thread pull for Processing lives inside the JobProcessorThread
    ThreadPool threadPool { 1 };
    // int jobsFinished;
    // int totalJobs;
    // JobProcessorThread jobProcessorThread;
    ThreadPool jobProcessorThread { 10 };
    std::deque<CustomThreadPoolJob*> customJobs;

    ChangeBroadcaster loadBroadcaster;
    ChangeBroadcaster processBroadcaster;

    bool showMediaClipboard;

    std::shared_ptr<fontawesome::IconHelper> fontawesomeHelper;
    std::shared_ptr<fontaudio::IconHelper> fontaudioHelper;
    juce::String savedStabilityToken;

    std::map<int, std::string> modelPathIdToURL;  // map the ComboBox item ID to the model URL


   

    /*void play()
    {
        // if (! mediaDisplay->isPlaying())
        // {
        //     mediaDisplay->start();
        //     playStopButton.setMode(stopButtonInfo.label);
        // }
        // visit all the mediaDisplays and check each of them
        for (auto& display : inputTrackAreaWidget.getMediaDisplays())
        {
            if (! display->isPlaying())
            {
                display->start();
            }
        }
        // playStopButton.setMode(stopButtonInfo.label);
    }

    void stop()
    {
        // if (mediaDisplay->isPlaying())
        // {
        //     mediaDisplay->stop();
        //     playStopButton.setMode(playButtonInfo.label);
        // }
        for (auto& display : inputTrackAreaWidget.getMediaDisplays())
        {
            if (display->isPlaying())
            {
                display->stop();
            }
        }
        // playStopButton.setMode(playButtonInfo.label);
    }*/

    void resetProcessingButtons()
    {
        processCancelButton.setMode(processButtonInfo.label);
        processCancelButton.setEnabled(true);
        saveEnabled = true;
        isProcessing = false;
        loadModelButton.setEnabled(true);
        modelPathComboBox.setEnabled(true);
        repaint();
    }

    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        // The processBroadcaster should be also replaced in a similar way
        // as the loadBroadcaster (see processLoadingResult)
        if (source == &processBroadcaster)
        {
            // // refresh the display for the new updated file
            // URL tempFilePath = outputMediaDisplays[0]->getTempFilePath();
            // outputMediaDisplays[0]->updateDisplay(tempFilePath);

            // // extract generated labels from the model

            // // add the labels to the display component
            // outputMediaDisplays[0]->addLabels(labels);

            // The above commented code was for the case of a single output media display.
            // Now, we get from model all the outputPaths using model->getOutputPaths()
            // and we iterate over both outputMediaDisplays and outputPaths to update the display

            // Additionally, we filter the labels to only show the audio labels to audio output media displays
            // and midi labels to midi output media displays.

            LabelList& labels = model->getLabels();
            auto outputProcessedPaths = model->getOutputFilePaths();
            auto& outputMediaDisplays = outputTrackAreaWidget.getMediaDisplays();
            for (size_t i = 0; i < outputMediaDisplays.size(); ++i)
            {
                URL tempFile = outputProcessedPaths[i];
                outputMediaDisplays[i]->initializeDisplay(tempFile);
                outputMediaDisplays[i]->addLabels(labels);
            }
            // URL tempFilePath = outputProcessedPaths[0];
            // outputMediaDisplays[0]->setupDisplay(tempFilePath);

            // now, we can enable the process button
            resetProcessingButtons();
            return;
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

    void populateTracks()
    {
        for (const ComponentInfo& info : model->getInputTracksInfo())
        {
            inputTrackAreaWidget.addTrackFromComponentInfo(info);
        }

        for (const ComponentInfo& info : model->getOutputTracksInfo())
        {
            outputTrackAreaWidget.addTrackFromComponentInfo(info);
        }
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

            populateTracks();

            //Apply saved Stability token
            SpaceInfo spaceInfo = model->getClient().getSpaceInfo();
            if (spaceInfo.status == SpaceInfo::Status::STABILITY && ! savedStabilityToken.isEmpty())
            {
                model->getClient().setToken(savedStabilityToken);
                setStatus("Applied saved Stability AI token to loaded model.");
            }
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
            tryLoadSavedToken();
        }

        loadModelButton.setEnabled(true);
        modelPathComboBox.setEnabled(true);
        loadModelButton.setButtonText("Load");

        // Set the focus to the process button
        // so that the user can press SPACE to trigger the playback
        // cb: I don't understand this.
        processCancelButton.grabKeyboardFocus();
        resized();
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
