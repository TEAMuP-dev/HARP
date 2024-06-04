/*
  ==============================================================================

   This file is part of the JUCE examples.
   Copyright (c) 2022 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             MainComponent
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Plays an audio file.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2022, linux_make, androidstudio, xcode_iphone

 type:             Component
 mainClass:        MainComponent

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/

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

#include "WebModel.h"
#include "CtrlComponent.h"
#include "TitledTextBox.h"
#include "ThreadPoolJob.h"

#include "gui/MultiButton.h"
#include "gui/StatusComponent.h"

using namespace juce;


// this only calls the callback ONCE
class TimedCallback : public Timer
{
public:
    TimedCallback(std::function<void()> callback, int interval) : mCallback(callback), mInterval(interval) {
        startTimer(mInterval);
    }

    ~TimedCallback() override {
        stopTimer();
    }

    void timerCallback() override {
        mCallback();
        stopTimer();
    }
private:
    std::function<void()> mCallback;
    int mInterval;
};

inline Colour getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour uiColour, Colour fallback = Colour (0xff4d4d4d)) noexcept
{
    if (auto* v4 = dynamic_cast<LookAndFeel_V4*> (&LookAndFeel::getDefaultLookAndFeel()))
        return v4->getCurrentColourScheme().getUIColour (uiColour);

    return fallback;
}


inline std::unique_ptr<InputSource> makeInputSource (const URL& url)
{
    if (const auto doc = AndroidDocument::fromDocument (url))
        return std::make_unique<AndroidDocumentInputSource> (doc);

   #if ! JUCE_IOS
    if (url.isLocalFile())
        return std::make_unique<FileInputSource> (url.getLocalFile());
   #endif

    return std::make_unique<URLInputSource> (url);
}

inline std::unique_ptr<OutputStream> makeOutputStream (const URL& url)
{
    if (const auto doc = AndroidDocument::fromDocument (url))
        return doc.createOutputStream();

   #if ! JUCE_IOS
    if (url.isLocalFile())
        return url.getLocalFile().createOutputStream();
   #endif

    return url.createOutputStream();
}



class ThumbnailComp  : public Component,
                           public ChangeListener,
                           public FileDragAndDropTarget,
                           public ChangeBroadcaster,
                           private ScrollBar::Listener,
                           private Timer
{
public:

    enum ActionType {
        FileDropped,
        TransportMoved,
        TransportStarted
    };
    

    ThumbnailComp (AudioFormatManager& formatManager,
                       AudioTransportSource& source,
                       Slider& slider)
        : transportSource (source),
          zoomSlider (slider),
          thumbnail (512, formatManager, thumbnailCache)
    {
        thumbnail.addChangeListener (this);

        addAndMakeVisible (scrollbar);
        scrollbar.setRangeLimits (visibleRange);
        scrollbar.setAutoHide (false);
        scrollbar.addListener (this);

        currentPositionMarker.setFill (Colours::white.withAlpha (0.85f));
        addAndMakeVisible (currentPositionMarker);
    }

    ~ThumbnailComp() override
    {
        scrollbar.removeListener (this);
        thumbnail.removeChangeListener (this);
    }

    void setURL (const URL& url)
    {
        if (auto inputSource = makeInputSource (url))
        {
            thumbnailCache.clear();
            thumbnail.setSource (inputSource.release());

            Range<double> newRange (0.0, thumbnail.getTotalLength());
            scrollbar.setRangeLimits (newRange);
            setRange (newRange);

            startTimerHz (40);
        }
    }

    URL getLastDroppedFile() const noexcept { return lastFileDropped; }
    ActionType getLastActionType() const noexcept { return lastActionType; }

    void setZoomFactor (double amount)
    {
        if (thumbnail.getTotalLength() > 0)
        {
            auto newScale = jmax (0.001, thumbnail.getTotalLength() * (1.0 - jlimit (0.0, 0.99, amount)));
            auto timeAtCentre = xToTime ((float) getWidth() / 2.0f);

            setRange ({ timeAtCentre - newScale * 0.5, timeAtCentre + newScale * 0.5 });
        }
    }

    void setRange (Range<double> newRange)
    {
        visibleRange = newRange;
        scrollbar.setCurrentRange (visibleRange);
        updateCursorPosition();
        repaint();
    }

    // void setFollowsTransport (bool shouldFollow)
    // {
    //     isFollowingTransport = shouldFollow;
    // }

    void paint (Graphics& g) override
    {
        g.fillAll (Colours::darkgrey);
        g.setColour (Colours::lightblue);

        if (thumbnail.getTotalLength() > 0.0)
        {
            auto thumbArea = getLocalBounds();

            thumbArea.removeFromBottom (scrollbar.getHeight() + 4);
            thumbnail.drawChannels (g, thumbArea.reduced (2),
                                    visibleRange.getStart(), visibleRange.getEnd(), 1.0f);
        }
        else
        {
            g.setFont (14.0f);
            g.drawFittedText ("(No audio file selected)", getLocalBounds(), Justification::centred, 2);
        }
    }

    void resized() override
    {
        scrollbar.setBounds (getLocalBounds().removeFromBottom (14).reduced (2));
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        // this method is called by the thumbnail when it has changed, so we should repaint it..
        repaint();
    }

    bool isInterestedInFileDrag (const StringArray& /*files*/) override
    {
        return true;
    }

    void filesDropped (const StringArray& files, int /*x*/, int /*y*/) override
    {
        lastFileDropped = URL (File (files[0]));
        lastActionType = FileDropped;
        sendChangeMessage();
    }

    void mouseDown (const MouseEvent& e) override
    {
        mouseDrag (e);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (canMoveTransport())
            transportSource.setPosition (jmax (0.0, xToTime ((float) e.x)));
            lastActionType = TransportMoved;
    }

    void mouseUp (const MouseEvent&) override
    {
        if (lastActionType == TransportMoved) {
            // transportSource.start();
            lastActionType = TransportStarted;
            sendChangeMessage();
        }
        
    }

    
    void mouseWheelMove (const MouseEvent&, const MouseWheelDetails& wheel) override
    {
        // DBG("Mouse wheel moved: deltaX=" << wheel.deltaX << ", deltaY=" << wheel.deltaY);
        if (thumbnail.getTotalLength() > 0.0)
        {
            if (std::abs(wheel.deltaX) > 2 * std::abs(wheel.deltaY)) {
                auto newStart = visibleRange.getStart() - wheel.deltaX * (visibleRange.getLength()) / 10.0;
                newStart = jlimit(0.0, jmax(0.0, thumbnail.getTotalLength() - visibleRange.getLength()), newStart);

                if (canMoveTransport())
                    setRange({ newStart, newStart + visibleRange.getLength() });
            } else if (std::abs(wheel.deltaY) > 2 * std::abs(wheel.deltaX)) {
                if (wheel.deltaY != 0) {
                    zoomSlider.setValue(zoomSlider.getValue() - wheel.deltaY);
                }
            } else {
                // Do nothing
            }
            repaint();
        }
    }


private:
    AudioTransportSource& transportSource;
    Slider& zoomSlider;
    ScrollBar scrollbar  { false };

    AudioThumbnailCache thumbnailCache  { 5 };
    AudioThumbnail thumbnail;
    Range<double> visibleRange;
    bool isFollowingTransport = true;
    URL lastFileDropped;
    ActionType lastActionType;

    DrawableRectangle currentPositionMarker;

    float timeToX (const double time) const
    {
        if (visibleRange.getLength() <= 0)
            return 0;

        return (float) getWidth() * (float) ((time - visibleRange.getStart()) / visibleRange.getLength());
    }

    double xToTime (const float x) const
    {
        return (x / (float) getWidth()) * (visibleRange.getLength()) + visibleRange.getStart();
    }

    bool canMoveTransport() const noexcept
    {
        return ! (isFollowingTransport && transportSource.isPlaying());
    }

    void scrollBarMoved (ScrollBar* scrollBarThatHasMoved, double newRangeStart) override
    {
        if (scrollBarThatHasMoved == &scrollbar)
            if (! (isFollowingTransport && transportSource.isPlaying()))
                setRange (visibleRange.movedToStartAt (newRangeStart));
    }

    void timerCallback() override
    {
        if (canMoveTransport())
            updateCursorPosition();
        else
            setRange (visibleRange.movedToStartAt (transportSource.getCurrentPosition() - (visibleRange.getLength() / 2.0)));
    }

    void updateCursorPosition()
    {
        currentPositionMarker.setVisible (transportSource.isPlaying() || isMouseButtonDown());

        currentPositionMarker.setRectangle (Rectangle<float> (timeToX (transportSource.getCurrentPosition()) - 0.75f, 0,
                                                              1.5f, (float) (getHeight() - scrollbar.getHeight())));
    }
};

//this is the callback for the add new path popup alert
class CustomPathAlertCallback : public juce::ModalComponentManager::Callback {
public:
    CustomPathAlertCallback(std::function<void(int)> const& callback) : userCallback(callback) {}

    void modalStateFinished(int result) override {
        if(userCallback != nullptr) {
            userCallback(result);
        }
    }
private:
    std::function<void(int)> userCallback;
};



//==============================================================================
class MainComponent  : public Component,
                          #if (JUCE_ANDROID || JUCE_IOS)
                           private Button::Listener,
                          #endif
                           private ChangeListener,
                           public MenuBarModel,
                           public ApplicationCommandTarget,
                           public Timer
                                     
{
public:

    enum CommandIDs {
        open = 0x2000,
        save = 0x2001,
        saveAs = 0x2002,
        about = 0x2003,
        // settings = 0x2004,
    };

    StringArray getMenuBarNames() override
    {
        return {"File"};
    }

    // In mac, we want the "about" command to be in the application menu ("HARP" tab)
    // For now, this is not used, as the extra commands appear grayed out
    std::unique_ptr<PopupMenu> getMacExtraMenu() {
        auto menu = std::make_unique<PopupMenu>();
        menu->addCommandItem (&commandManager, CommandIDs::about);
        return menu;
    }

    PopupMenu getMenuForIndex (int menuIndex, const String& menuName) override
    {
        PopupMenu menu;

        if (menuName == "File")
        {   
            menu.addCommandItem (&commandManager, CommandIDs::open);
            menu.addCommandItem (&commandManager, CommandIDs::save);
            menu.addCommandItem (&commandManager, CommandIDs::saveAs);
            menu.addSeparator();
            // menu.addCommandItem (&commandManager, CommandIDs::settings);
            // menu.addSeparator();
            menu.addCommandItem (&commandManager, CommandIDs::about);
        } 
        return menu;
    }
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override {
        DBG("menuItemSelected: " << menuItemID);
        DBG("topLevelMenuIndex: " << topLevelMenuIndex);
    }

    ApplicationCommandTarget* getNextCommandTarget() override {
        return nullptr;
    }

    // Fills the commands array with the commands that this component/target supports
    void getAllCommands(Array<CommandID>& commands) override {
        const CommandID ids[] = { 
            CommandIDs::open,
            CommandIDs::save, 
            CommandIDs::saveAs,
            CommandIDs::about,
            };
        commands.addArray(ids, numElementsInArray(ids));
    }

    // Gets the information about a specific command
    void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override {
        switch (commandID) {
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
                result.setInfo("Save As...", "Saves the current document with a new name", "File", 0);
                result.addDefaultKeypress('s', ModifierKeys::shiftModifier | ModifierKeys::commandModifier);
                break;
            case CommandIDs::about:
                result.setInfo("About HARP", "Shows information about the application", "About", 0);
                break;
        }
    }

    // Callback for the save and saveAs commands
    bool perform(const InvocationInfo& info) override {
        switch (info.commandID) {
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
        aboutText->setText(
            String(APP_NAME) + "\nVersion: " + String(APP_VERSION) + "\n\n",
            dontSendNotification);
        aboutText->setJustificationType(Justification::centred);
        aboutText->setSize(380, 100); 

        // hyperlink buttons
        auto* modelGlossaryButton = new HyperlinkButton("Model Glossary",
            URL("https://github.com/TEAMuP-dev/HARP#available-models"));
        modelGlossaryButton->setSize(380, 24); 
        modelGlossaryButton->setTopLeftPosition(10, 110); 
        modelGlossaryButton->setJustificationType(Justification::centred);
        modelGlossaryButton->setColour(HyperlinkButton::textColourId, Colours::blue);

        auto* visitWebpageButton = new HyperlinkButton("Visit HARP webpage",
            URL("https://harp-plugin.netlify.app/"));
        visitWebpageButton->setSize(380, 24); 
        visitWebpageButton->setTopLeftPosition(10, 140); 
        visitWebpageButton->setJustificationType(Justification::centred);
        visitWebpageButton->setColour(HyperlinkButton::textColourId, Colours::blue);

        auto* reportIssueButton = new HyperlinkButton("Report an issue",
            URL("https://github.com/TEAMuP-dev/harp/issues"));
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
    

    void saveCallback(){
        if (saveEnabled) {
            DBG("HARPProcessorEditor::buttonClicked save button listener activated");
            // copy the file to the target location
            DBG("copying from " << currentAudioFile.getLocalFile().getFullPathName() << " to " << currentAudioFileTarget.getLocalFile().getFullPathName());
            // make a backup for the undo button
            // rename the original file to have a _backup suffix
            File backupFile = File(
                currentAudioFileTarget.getLocalFile().getParentDirectory().getFullPathName() + "/"
                + currentAudioFileTarget.getLocalFile().getFileNameWithoutExtension() +
                + "_BACKUP" + currentAudioFileTarget.getLocalFile().getFileExtension()
            );

            if (currentAudioFileTarget.getLocalFile().copyFileTo(backupFile)) {
                DBG("made a backup of the original file at " << backupFile.getFullPathName());
            } else {
                DBG("failed to make a backup of the original file at " << backupFile.getFullPathName());
            }

            if (currentAudioFile.getLocalFile().moveFileTo(currentAudioFileTarget.getLocalFile())) {
                DBG("copied the file to " << currentAudioFileTarget.getLocalFile().getFullPathName());
            } else {
                DBG("failed to copy the file to " << currentAudioFileTarget.getLocalFile().getFullPathName());
            }
            
            addNewAudioFile(currentAudioFileTarget);
            // saveButton.setEnabled(false);
            saveEnabled = false;
            setStatus("File saved successfully");
        } else {
            DBG("save button is disabled");
            setStatus("Nothing to save");
        }
    }

    
    void saveAsCallback() {

        // Launch the file chooser dialog asynchronously
        fileChooser->launchAsync(
            FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles,
            [this](const FileChooser& chooser) {
                File newFile = chooser.getResult();
                if (newFile != File{}) {
                    // Attempt to save the file to the new location
                    bool saveSuccessful = currentAudioFile.getLocalFile().copyFileTo(newFile);
                    if (saveSuccessful) {
                        // Inform the user of success
                        AlertWindow::showMessageBoxAsync(
                            AlertWindow::InfoIcon,
                            "Save As",
                            "File successfully saved as:\n" + newFile.getFullPathName(),
                            "OK");

                        // Update any necessary internal state
                        // currentAudioFile = AudioFile(newFile); // Assuming a wrapper, adjust accordingly
                        DBG("File successfully saved as " << newFile.getFullPathName());
                    } else {
                        // Inform the user of failure
                        AlertWindow::showMessageBoxAsync(
                            AlertWindow::WarningIcon,
                            "Save As Failed",
                            "Failed to save file as:\n" + newFile.getFullPathName(),
                            "OK");
                        DBG("Failed to save file as " << newFile.getFullPathName());
                    }
                } else {
                    DBG("Save As operation was cancelled by the user.");
                }
            });
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
        for(int i = 0; i < options.size(); ++i) {
            modelPathComboBox.addItem(options[i], i + 1);
        }
    }

    // void loadAudioFile(const URL& audioURL) {
    //     addNewAudioFile(audioURL);
    // }
    explicit MainComponent(const URL& initialFileURL = URL()): jobsFinished(0), totalJobs(0),
        jobProcessorThread(customJobs, jobsFinished, totalJobs, processBroadcaster)
    {
        addAndMakeVisible (zoomLabel);
        zoomLabel.setFont (Font (15.00f, Font::plain));
        zoomLabel.setJustificationType (Justification::centredRight);
        zoomLabel.setEditable (false, false, false);
        zoomLabel.setColour (TextEditor::textColourId, Colours::black);
        zoomLabel.setColour (TextEditor::backgroundColourId, Colour (0x00000000));

        // addAndMakeVisible (followTransportButton);
        // followTransportButton.onClick = [this] { updateFollowTransportState(); };

        addAndMakeVisible(chooseFileButton);
        chooseFileButton.onClick = [this] { openFileChooser(); };

        addAndMakeVisible (zoomSlider);
        zoomSlider.setRange (0, 1, 0);
        zoomSlider.onValueChange = [this] { thumbnail->setZoomFactor (zoomSlider.getValue()); };
        zoomSlider.setSkewFactor (2);

        thumbnail = std::make_unique<ThumbnailComp> (formatManager, transportSource, zoomSlider);
        addAndMakeVisible (thumbnail.get());
        thumbnail->addChangeListener (this);

        // addAndMakeVisible (startStopButton);
        playStopButton.addMode(playButtonInfo);
        playStopButton.addMode(stopButtonInfo);
        playStopButton.setMode(playButtonInfo.label);
        playStopButton.setEnabled(false);
        addAndMakeVisible (playStopButton);
        playStopButton.onMouseEnter = [this] {
            if (playStopButton.getModeName() == playButtonInfo.label)
                setInstructions("Click to start playback");
            else if (playStopButton.getModeName() == stopButtonInfo.label)
                setInstructions("Click to stop playback");
        };
        playStopButton.onMouseExit = [this] {
            // playStopButton.setColour (TextButton::buttonColourId, getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::buttonOnColour));
            clearInstructions();
        };

        // audio setup
        formatManager.registerBasicFormats();

        thread.startThread (Thread::Priority::normal);

       #ifndef JUCE_DEMO_RUNNER
        audioDeviceManager.initialise (0, 2, nullptr, true, {}, nullptr);
       #endif

        audioDeviceManager.addAudioCallback (&audioSourcePlayer);
        audioSourcePlayer.setSource (&transportSource);


        // Load the initial file
        if (initialFileURL.isLocalFile())
        {
            addNewAudioFile (initialFileURL);
        }

        // initialize HARP UI
        // TODO: what happens if the model is nullptr rn?
        if (model == nullptr) {
            DBG("FATAL HARPProcessorEditor::HARPProcessorEditor: model is null");
            jassertfalse;
            return;
        }

        // Set setWantsKeyboardFocus to true for this component
        // Doing that, everytime we click outside the modelPathTextBox,
        // the focus will be taken away from the modelPathTextBox
        setWantsKeyboardFocus(true);

        // init the menu bar
        menuBar.reset (new MenuBarComponent (this));
        addAndMakeVisible (menuBar.get());
        setApplicationCommandManagerToWatch (&commandManager);
        // Register commands
        commandManager.registerAllCommandsForTarget(this);
        // commandManager.setFirstCommandTarget(this);
        addKeyListener (commandManager.getKeyMappings());

        
        #if JUCE_MAC
            // Not used for now
            // auto extraMenu = getMacExtraMenu();
            MenuBarModel::setMacMainMenu (this);
        #endif

        menuBar->setVisible (true);
        menuItemsChanged();

        // The Process/Cancel button        
        processCancelButton.addMode(processButtonInfo);
        processCancelButton.addMode(cancelButtonInfo);
        processCancelButton.setMode(processButtonInfo.label);
        processCancelButton.setEnabled(false);
        addAndMakeVisible(processCancelButton);
        processCancelButton.onMouseEnter = [this] {
            if (processCancelButton.getModeName() == processButtonInfo.label)
                setInstructions("Click to send the audio file for processing");
            else if (processCancelButton.getModeName() == cancelButtonInfo.label)
                setInstructions("Click to cancel the processing");
        };
        processCancelButton.onMouseExit = [this] {
            // processCancelButton.setColour (TextButton::buttonColourId, getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::buttonOnColour));
            clearInstructions();
        };

        processBroadcaster.addChangeListener(this);
        saveEnabled = false;

        loadModelButton.setButtonText("load");
        addAndMakeVisible(loadModelButton);
        loadModelButton.onClick = [this]{
            DBG("HARPProcessorEditor::buttonClicked load model button listener activated");

            // collect input parameters for the model.
                        
            const std::string hf_url = "https://huggingface.co/spaces/";

            std::string path_url;
            if (modelPathComboBox.getSelectedItemIndex() == 0) {
                if (customPath.find(hf_url) != std::string::npos) {
                    customPath.replace(customPath.find(hf_url), hf_url.length(), "");
                }
                path_url = customPath;
            }else{
                path_url = modelPathComboBox.getText().toStdString();
            }

            std::map<std::string, std::any> params = {
            {"url", path_url},
            };
            resetUI();
            // loading happens asynchronously.
            // the document controller trigger a change listener callback, which will update the UI

            threadPool.addJob([this, params] {
                DBG("executeLoad!!");
                try {
                    // timeout after 10 seconds
                    // TODO: this callback needs to be cleaned up in the destructor in case we quit
                    // cb: this timedCallback doesn't seem to run
                    std::atomic<bool> success = false;
                    TimedCallback timedCallback([this, &success] {
                        if (success)
                            return;
                        DBG("TIMED-CALLBACK: buttonClicked timedCallback listener activated");
                        AlertWindow::showMessageBoxAsync(
                            AlertWindow::WarningIcon,
                            "Loading Error",
                            "An error occurred while loading the WebModel: TIMED OUT! Please check that the space is awake."
                        );
                        MessageManager::callAsync([this] {
                            resetModelPathComboBox();
                        });
                        model.reset(new WebWave2Wave());
                        loadBroadcaster.sendChangeMessage();
                        // saveButton.setEnabled(false);
                        saveEnabled = false;
                    }, 10000);

                    model->load(params);
                    success = true;
                    MessageManager::callAsync([this] {
                        if (modelPathComboBox.getSelectedItemIndex() == 0) {
                            bool alreadyInComboBox = false;

                            for (int i = 1; i <= modelPathComboBox.getNumItems(); ++i) {
                                if (modelPathComboBox.getItemText(i) == (String) customPath) {
                                    alreadyInComboBox = true;
                                    modelPathComboBox.setSelectedId(i + 1);
                                }
                            }

                            if (!alreadyInComboBox) {
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
                } catch (const std::runtime_error& e) {
                    DBG("Caught exception: " << e.what());
                    
                    auto msgOpts = MessageBoxOptions().withTitle("Loading Error")
                        .withIconType(AlertWindow::WarningIcon)
                        .withTitle("Error")
                        .withMessage("An error occurred while loading the WebModel: \n" + String(e.what()));
                    if (!String(e.what()).contains("404")) {
                        msgOpts = msgOpts.withButton("Open Space URL");
                    }
                        msgOpts = msgOpts.withButton("Open HARP Logs").withButton("Ok");
                    auto alertCallback = [this, msgOpts](int result) {
                        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                        // NOTE (hugo): there's something weird about the button indices assigned by the msgOpts here
                        // DBG("ALERT-CALLBACK: buttonClicked alertCallback listener activated: chosen: " << chosen);
                        // auto chosen = msgOpts.getButtonText(result);
                        // they're not the same as the order of the buttons in the alert
                        // this is the order that I actually observed them to be. 
                        // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

                        std::map<int, std::string> observedButtonIndicesMap = {};
                        if (msgOpts.getNumButtons() == 3) {
                            observedButtonIndicesMap.insert({1, "Open Space URL"});// should actually be 0 right? 
                        }
                        observedButtonIndicesMap.insert({msgOpts.getNumButtons() - 1, "Open HARP Logs"});// should actually be 1
                        observedButtonIndicesMap.insert({0, "Ok"});// should be 2
 
                        auto chosen = observedButtonIndicesMap[result];

                        // auto chosen = msgOpts.getButtonText();
                        if (chosen == "Open HARP Logs") {
                            model->getLogFile().revealToUser();
                        } else if (chosen == "Open Space URL") {
                            URL spaceUrl = resolveSpaceUrl(modelPathComboBox.getText().toStdString());
                            bool success = spaceUrl.launchInDefaultBrowser();
                        }
                        MessageManager::callAsync([this] {
                            resetModelPathComboBox();
                        });
                    };
                    
                    
                    AlertWindow::showAsync(msgOpts,alertCallback);

                    model.reset(new WebWave2Wave());
                    loadBroadcaster.sendChangeMessage();
                    // saveButton.setEnabled(false);
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
            descriptionLabel.setText("loading " + url + "...\n if this takes a while, check if the huggingface space is sleeping by visiting the space url below. Once the huggingface space is awake, try again." , dontSendNotification);

            // TODO: here, we should also reset the highlighting of the playback regions 


            // add a hyperlink to the hf space for the model
            // TODO: make this less hacky? 
            // we might have to append a "https://huggingface.co/spaces" to the url
            // IF the url (doesn't have localhost) and (doesn't have huggingface.co) and (doesn't have http) in it 
            // and (has only one slash in it)
            String spaceUrl = resolveSpaceUrl(url);
            spaceUrlButton.setButtonText("open " + url + " in browser");
            spaceUrlButton.setURL(URL(spaceUrl));
            // set the font size 
            // spaceUrlButton.setFont(Font(15.00f, Font::plain));

            addAndMakeVisible(spaceUrlButton);
        };

        loadBroadcaster.addChangeListener(this);

        std::string currentStatus = model->getStatus();
        if (currentStatus == "Status.LOADED" || currentStatus == "Status.FINISHED") {
            processCancelButton.setEnabled(true);
            processCancelButton.setMode(processButtonInfo.label);
        } else if (currentStatus == "Status.PROCESSING" || currentStatus == "Status.STARTING" || currentStatus == "Status.SENDING") {
            processCancelButton.setEnabled(true);
            processCancelButton.setMode(cancelButtonInfo.label);
        }
        

        // status label
        // statusLabel.setText(currentStatus, dontSendNotification);
        // addAndMakeVisible(statusLabel);
        setStatus(currentStatus);

        // add a status timer to update the status label periodically
        mModelStatusTimer = std::make_unique<ModelStatusTimer>(model);
        mModelStatusTimer->addChangeListener(this);
        mModelStatusTimer->startTimer(100);  // 100 ms interval


       // model path textbox
       std::vector<std::string> modelPaths = {
        "custom path...",
        "hugggof/pitch_shifter",
        "hugggof/harmonic_percussive",
        "descript/vampnet",
        "hugggof/nesquik",
        "cwitkowitz/timbre-trap",
        };


        modelPathComboBox.setTextWhenNothingSelected("choose a model"); 
        for(int i = 0; i < modelPaths.size(); ++i) {
            modelPathComboBox.addItem(modelPaths[i], i + 1);
        }


        // Usage within your existing onChange handler
        modelPathComboBox.onChange = [this] {
            // Check if the 'custom path...' option is selected
            if (modelPathComboBox.getSelectedItemIndex() == 0) {
                // Create an AlertWindow
                auto* customPathWindow = new AlertWindow("Enter Custom Path",
                                                        "Please enter the path to the gradio endpoint:",
                                                        AlertWindow::NoIcon);

                customPathWindow->addTextEditor("customPath", "", "Path:");
                customPathWindow->addButton("Load", 1, KeyPress(KeyPress::returnKey));
                customPathWindow->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));
                
                // Show the window and handle the result asynchronously
                customPathWindow->enterModalState(true, new CustomPathAlertCallback([this, customPathWindow](int result) {
                    if (result == 1) { // OK was clicked
                        // Retrieve the entered path
                        customPath = customPathWindow->getTextEditor("customPath")->getText().toStdString();
                        // Use the custom path as needed
                        DBG("Custom path entered: " + customPath);
                        loadModelButton.triggerClick();

                    } else { // Cancel was clicked or the window was closed
                        DBG("Custom path entry was canceled.");
                        resetModelPathComboBox();
                    }
                    delete customPathWindow;
                }), true);
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

        addAndMakeVisible(statusArea);
        addAndMakeVisible(instructionsArea);
        // model card component
        // Get the modelCard from the EditorView
        auto &card = model->card();
        setModelCard(card);

        jobProcessorThread.startThread();

        startTimerHz(10);
        // ARA requires that plugin editors are resizable to support tight integration
        // into the host UI
        setOpaque (true);
        setSize(800, 800);
        resized();
    }


    ~MainComponent() override
    {
        transportSource  .setSource (nullptr);
        audioSourcePlayer.setSource (nullptr);

        audioDeviceManager.removeAudioCallback (&audioSourcePlayer);

        thumbnail->removeChangeListener (this);

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
            MenuBarModel::setMacMainMenu (nullptr);
        #endif
        // commandManager.setFirstCommandTarget (nullptr);
    }

    void timerCallback() override
    {
        if (!transportSource.isPlaying() && playStopButton.getModeName() == stopButtonInfo.label)
        {
            playStopButton.setMode(playButtonInfo.label);
            stopTimer();
            transportSource.setPosition (0.0);
            
        }
        
    }

    void cancelCallback()
    {
        DBG("HARPProcessorEditor::buttonClicked cancel button listener activated");
        model->cancel();
        processCancelButton.setEnabled(false);
    }
    
    void processCallback()
    {
        DBG("HARPProcessorEditor::buttonClicked button listener activated");

        // check if the audio file is loaded
        if (!currentAudioFile.isLocalFile()) {
            // AlertWindow("Error", "Audio file is not loaded. Please load an audio file first.", AlertWindow::WarningIcon);
            //ShowMEssageBoxAsync
            AlertWindow::showMessageBoxAsync(
                AlertWindow::WarningIcon,
                "Error",
                "Audio file is not loaded. Please load an audio file first."
            );
            return;
        }


        processCancelButton.setEnabled(true);
        processCancelButton.setMode(cancelButtonInfo.label);

        saveEnabled = false;
        isProcessing = true;

        // TODO: get the current audio file and process it
        // if we don't have one, let the user know
        // TODO: need to only be able to do this if we don't have any other jobs in the threadpool right?
        if (model == nullptr){
            DBG("unhandled exception: model is null. we should probably open an error window here.");
            AlertWindow("Error", "Model is not loaded. Please load a model first.", AlertWindow::WarningIcon);
            isProcessing = false;
            return;
        }

        // print how many jobs are currently in the threadpool
        DBG("threadPool.getNumJobs: " << threadPool.getNumJobs());

        // empty customJobs
        customJobs.clear();

        customJobs.push_back(new CustomThreadPoolJob(
            [this] { // &jobsFinished, totalJobs
                // Individual job code for each iteration
                // copy the audio file, with the same filename except for an added _harp to the stem
                model->process(currentAudioFile.getLocalFile());
                DBG("Processing finished");
                // load the audio file again
                processBroadcaster.sendChangeMessage();
                
            }
        ));

        // Now the customJobs are ready to be added to be run in the threadPool
        jobProcessorThread.signalTask();
    }
    

    String getAllAudioFileExtensions(AudioFormatManager& formatManager)
    {
        StringArray extensions;
        for (int i = 0; i < formatManager.getNumKnownFormats(); ++i)
        {
            auto* format = formatManager.getKnownFormat(i);
            auto formatExtensions = format->getFileExtensions();
            for (auto& ext : formatExtensions)
            {
                extensions.addTokens("*" + ext.trim(), ";", "\"");
            }
            // extensions.addTokens(format->getFileExtensions(), ";", "\"" );
        }
        extensions.removeDuplicates(false);
        return extensions.joinIntoString(";");
    }

    std::string getAllAudioFileExtensions2(AudioFormatManager& formatManager)
    {
        std::set<std::string> uniqueExtensions;

        for (int i = 0; i < formatManager.getNumKnownFormats(); ++i)
        {
            auto* format = formatManager.getKnownFormat(i);
            juce::String extensionsString = format->getFileExtensions()[0];
            StringArray extensions = StringArray::fromTokens(extensionsString, ";", "");
            // StringArray extensions = StringArray::fromTokens(format->getFileExtensions(), ";", "");

            for (auto& ext : extensions)
            {
                uniqueExtensions.insert(ext.toStdString());
            }
        }

        // Join all extensions into a single string with semicolons
        std::string allExtensions;
        for (auto it = uniqueExtensions.begin(); it != uniqueExtensions.end(); ++it)
        {
            if (it != uniqueExtensions.begin()) {
                allExtensions += ";";
            }
            allExtensions += *it;
        }

        return allExtensions;
    }
    void openFileChooser()
    {
        auto extensions = getAllAudioFileExtensions(formatManager);
        fileChooser = std::make_unique<FileChooser>(
            "Select an audio file...", 
            File(), 
            extensions);
        fileChooser->launchAsync(
            FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
            [this](const FileChooser& chooser)
            {
                File file = chooser.getResult();
                if (file != File{})
                {
                    URL fileURL = URL(file);
                    addNewAudioFile(fileURL);
                }
            });
    }


    void paint (Graphics& g) override
    {
        g.fillAll (getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        auto area = getLocalBounds();

        #if not JUCE_MAC
        menuBar->setBounds (area.removeFromTop (LookAndFeel::getDefaultLookAndFeel()
                                                    .getDefaultMenuBarHeight()));
        #endif
        auto margin = 10;  // Adjusted margin value for top and bottom spacing
        auto docViewHeight = 1;
        auto mainArea = area.removeFromTop(area.getHeight() - docViewHeight);
        auto documentViewArea = area;  // what remains is the 15% area for documentView
        // Row 1: Model Path TextBox and Load Model Button
        auto row1 = mainArea.removeFromTop(40);  // adjust height as needed
        modelPathComboBox.setBounds(row1.removeFromLeft(row1.getWidth() * 0.8f).reduced(margin));
        //modelPathTextBox.setBounds(row1.removeFromLeft(row1.getWidth() * 0.8f).reduced(margin));
        loadModelButton.setBounds(row1.reduced(margin));
        // Row 2: Name and Author Labels
        auto row2a = mainArea.removeFromTop(40);  // adjust height as needed
        nameLabel.setBounds(row2a.removeFromLeft(row2a.getWidth() / 2).reduced(margin));
        nameLabel.setFont(Font(20.0f, Font::bold));
        // nameLabel.setColour(Label::textColourId, mHARPLookAndFeel.textHeaderColor);
 
        auto row2b = mainArea.removeFromTop(30);
        authorLabel.setBounds(row2b.reduced(margin));
        authorLabel.setFont(Font(10.0f));

        // Row 3: Description Label
        
        // A way to dynamically adjust the height of the description label
        // doesn't work perfectly yet, but it's good for now.
        auto font = Font(15.0f); 
        descriptionLabel.setFont(font);
        // descriptionLabel.setColour(Label::backgroundColourId, Colours::red);
        auto maxLabelWidth = mainArea.getWidth() - 2 * margin;
        auto numberOfLines = font.getStringWidthFloat(descriptionLabel.getText(false)) / maxLabelWidth;
        int textHeight = (font.getHeight() + 5) * (std::floor(numberOfLines) + 1) + font.getHeight();

        if (textHeight < 80) {
            textHeight = 80;
        }
        auto row3 = mainArea.removeFromTop(textHeight).reduced(margin) ; 
        descriptionLabel.setBounds(row3);

        // Row 4: Space URL Hyperlink
        auto row4 = mainArea.removeFromTop(30);  // adjust height as needed
        spaceUrlButton.setBounds(row4.reduced(margin).removeFromLeft(row4.getWidth() / 2));
        spaceUrlButton.setFont(Font(11.0f), false, Justification::centredLeft);

        // Row 5: CtrlComponent (flexible height)
        auto row5 = mainArea.removeFromTop(150);  // the remaining area is for row 4
        ctrlComponent.setBounds(row5.reduced(margin));

        // An empty space of 30px between the ctrl component and the process button
        mainArea.removeFromTop(30);

        // Row 6: Process Button (taken out in advance to preserve its height)
        auto row6Height = 25;  // adjust height as needed
        auto row6 = mainArea.removeFromTop(row6Height);

        // Assign bounds to processButton
        processCancelButton.setBounds(row6.withSizeKeepingCentre(100, 20));  // centering the button in the row
        // place the status label to the left of the process button (justified left)
        // statusLabel.setBounds(processCancelButton.getBounds().translated(-200, 0));

        // An empty space of 30px between the process button and the thumbnail area
        mainArea.removeFromTop(30);
        
        // Row 7: thumbnail area
        auto row7 = mainArea.removeFromTop(150).reduced(margin / 2);  // adjust height as needed
        thumbnail->setBounds(row7);

        // Row 8: Buttons for Play/Stop and Open File
        auto row8 = mainArea.removeFromTop(70);  // adjust height as needed
        playStopButton.setBounds(row8.removeFromLeft(row8.getWidth() / 2).reduced(margin));
        chooseFileButton.setBounds(row8.reduced(margin));

        // Status area
        auto row9 = mainArea.removeFromBottom(80);
        // Split row9 to two columns
        auto row9a = row9.removeFromLeft(row9.getWidth() / 2);
        auto row9b = row9;
        statusArea.setBounds(row9a.reduced(margin));
        instructionsArea.setBounds(row9b.reduced(margin));
        
    }

    void resetUI(){
        ctrlComponent.resetUI();
        // Also clear the model card components
        ModelCard empty;
        setModelCard(empty);
    }

    void setModelCard(const ModelCard& card) {
        // Set the text for the labels
        nameLabel.setText(String(card.name), dontSendNotification);
        descriptionLabel.setText(String(card.description), dontSendNotification);
        // set the author label text to "by {author}" only if {author} isn't empty
        card.author.empty() ?
            authorLabel.setText("", dontSendNotification) :
            authorLabel.setText("by " + String(card.author), dontSendNotification);
    }

    void loadAudioFile(const URL& audioURL) {
        addNewAudioFile(audioURL);
    }

    void setStatus(const juce::String& message)
    {
        statusArea.setStatusMessage(message);
    }

    void clearStatus()
    {
        statusArea.clearStatusMessage();
    }

    void setInstructions(const juce::String& message)
    {
        instructionsArea.setStatusMessage(message);
    }

    void clearInstructions()
    {
        instructionsArea.clearStatusMessage();
    }
private:
    // HARP UI 
    std::unique_ptr<ModelStatusTimer> mModelStatusTimer {nullptr};
    ComboBox modelPathComboBox;
    TextButton loadModelButton;
    TextButton saveChangesButton {"save changes"};
    MultiButton processCancelButton;
    MultiButton playStopButton;
    MultiButton::Mode processButtonInfo{"Process", 
            [this] { processCallback(); }, 
            getUIColourIfAvailable(
                LookAndFeel_V4::ColourScheme::UIColour::windowBackground, 
                Colours::lightgrey)};
    MultiButton::Mode cancelButtonInfo{"Cancel", 
            [this] { cancelCallback(); },
            getUIColourIfAvailable(
                LookAndFeel_V4::ColourScheme::UIColour::windowBackground, 
                Colours::lightgrey)};
    MultiButton::Mode playButtonInfo{"Play", 
            [this] { play(); }, 
            Colours::limegreen};
    MultiButton::Mode stopButtonInfo{"Stop", 
            [this] { stop(); },
            Colours::orangered};

    // Label statusLabel;
    // A flag that indicates if the audio file can be saved
    bool saveEnabled = true;
    bool isProcessing = false;

    std::string customPath;
    CtrlComponent ctrlComponent;

    // model card
    Label nameLabel, authorLabel, descriptionLabel, tagsLabel;
    HyperlinkButton spaceUrlButton;

    StatusComponent statusArea;
    StatusComponent instructionsArea;

    // the model itself
    std::shared_ptr<WebWave2Wave> model {new WebWave2Wave()};

    // if this PIP is running inside the demo runner, we'll use the shared device manager instead
    #ifndef JUCE_DEMO_RUNNER
    AudioDeviceManager audioDeviceManager;
    #else
    AudioDeviceManager& audioDeviceManager { getSharedAudioDeviceManager (0, 2) };
    #endif


    std::unique_ptr<FileChooser> fileChooser;

    AudioFormatManager formatManager;
    TimeSliceThread thread  { "audio file preview" };

    TextButton chooseFileButton {"Open File", "Open an audio file for playback and editing"}; // Changed for desktop

    URL currentAudioFile;
    URL currentAudioFileTarget;
    AudioSourcePlayer audioSourcePlayer;
    AudioTransportSource transportSource;
    std::unique_ptr<AudioFormatReaderSource> currentAudioFileSource;

    std::unique_ptr<ThumbnailComp> thumbnail;
    Label zoomLabel                     { {}, "zoom:" };
    Slider zoomSlider                   { Slider::LinearHorizontal, Slider::NoTextBox };
    // ToggleButton followTransportButton  { "Follow Transport" };
    // TextButton startStopButton          { "Play/Stop" };


    /// CustomThreadPoolJob
    // This one is used for Loading the models
    // The thread pull for Processing lives inside the JobProcessorThread
    ThreadPool threadPool {1};
    int jobsFinished;
    int totalJobs;
    JobProcessorThread jobProcessorThread;
    std::vector<CustomThreadPoolJob*> customJobs;
    // ChangeBroadcaster processBroadcaster;
    ChangeBroadcaster loadBroadcaster;
    ChangeBroadcaster processBroadcaster;

    ApplicationCommandManager commandManager;  
    // MenuBar
    std::unique_ptr<MenuBarComponent> menuBar;
    // MenuBarPosition menuBarPosition = MenuBarPosition::window;

    //==============================================================================
    void showAudioResource (URL resource)
    {
        if (! loadURLIntoTransport (resource))
        {
            // Failed to load the audio file!
            jassertfalse;
            return;
        }

        zoomSlider.setValue (0, dontSendNotification);
        thumbnail->setURL (resource);
    }

    void addNewAudioFile (URL resource) 
    {
        currentAudioFileTarget = resource;
        
        currentAudioFile = URL(File(
            File::getSpecialLocation(File::SpecialLocationType::userDocumentsDirectory).getFullPathName() + "/HARP/"
            + currentAudioFileTarget.getLocalFile().getFileNameWithoutExtension() 
            + "_harp" + currentAudioFileTarget.getLocalFile().getFileExtension()
        ));

        currentAudioFile.getLocalFile().getParentDirectory().createDirectory();
        if (!currentAudioFileTarget.getLocalFile().copyFileTo(currentAudioFile.getLocalFile())) {
            DBG("MainComponent::addNewAudioFile: failed to copy file to " << currentAudioFile.getLocalFile().getFullPathName());
            // show an error to the user, we cannot proceed!
            AlertWindow("Error", "Failed to make a copy of the input file for processing!! are you out of space?", AlertWindow::WarningIcon);
        }
        DBG("MainComponent::addNewAudioFile: copied file to " << currentAudioFileTarget.getLocalFile().getFullPathName());

        playStopButton.setEnabled(true);
        showAudioResource(currentAudioFile);
    }

    bool loadURLIntoTransport (const URL& audioURL)
    {
        // unload the previous file source and delete it..
        transportSource.stop();
        transportSource.setSource (nullptr);
        currentAudioFileSource.reset();

        const auto source = makeInputSource (audioURL);

        if (source == nullptr)
            return false;

        auto stream = rawToUniquePtr (source->createInputStream());

        if (stream == nullptr)
            return false;

        auto reader = rawToUniquePtr (formatManager.createReaderFor (std::move (stream)));

        if (reader == nullptr)
            return false;

        currentAudioFileSource = std::make_unique<AudioFormatReaderSource> (reader.release(), true);

        // ..and plug it into our transport source
        transportSource.setSource (currentAudioFileSource.get(),
                                   32768,                   // tells it to buffer this many samples ahead
                                   &thread,                 // this is the background thread to use for reading-ahead
                                   currentAudioFileSource->getAudioFormatReader()->sampleRate);     // allows for sample rate correction

        return true;
    }

    void play() {
        if (!transportSource.isPlaying()) {
            // transportSource.setPosition (0);
            transportSource.start();
            playStopButton.setMode(stopButtonInfo.label);
            startTimerHz(10);
        }
    }

    void stop()    {
        if (transportSource.isPlaying()) {
            transportSource.stop();
            transportSource.setPosition (0);
            playStopButton.setMode(playButtonInfo.label);
            stopTimer();
        }
    }

    // void updateFollowTransportState()
    // {
    //     thumbnail->setFollowsTransport (followTransportButton.getToggleState());
    // }

    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (source == thumbnail.get()) {
            // if (transportSource.isPlaying()) {
            //     playStopButton.setMode(stopButtonInfo.label);
            // }
            // else {
            //     addNewAudioFile (URL (thumbnail->getLastDroppedFile()));
            // }
            if (thumbnail->getLastActionType() == ThumbnailComp::ActionType::FileDropped) {
                stop();
                addNewAudioFile (URL (thumbnail->getLastDroppedFile()));
            } else if (thumbnail->getLastActionType() == ThumbnailComp::ActionType::TransportStarted) {
                play();

            }
        }
        else if (source == &loadBroadcaster) {
            DBG("Setting up model card, CtrlComponent, resizing.");
            setModelCard(model->card());
            ctrlComponent.setModel(model);
            ctrlComponent.populateGui();
            repaint();

            // now, we can enable the buttons
            if (model->ready()) {
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
        else if (source == &processBroadcaster) {
            // refresh the display for the new updated file
            showAudioResource(currentAudioFile);

            // now, we can enable the process button
            processCancelButton.setMode(processButtonInfo.label);
            processCancelButton.setEnabled(true);
            saveEnabled = true;
            isProcessing = false;
            repaint();
        }
        else if (source == mModelStatusTimer.get()) {
            // update the status label
            DBG("HARPProcessorEditor::changeListenerCallback: updating status label");
            // statusLabel.setText(model->getStatus(), dontSendNotification);
            setStatus(model->getStatus());
        }
        else {
            DBG("HARPProcessorEditor::changeListenerCallback: unhandled change broadcaster");
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
