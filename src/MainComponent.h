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
// #include <juce_events/timers/juce_Timer.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "WebModel.h"
#include "CtrlComponent.h"
#include "TitledTextBox.h"
#include "ThreadPoolJob.h"
#include "MediaDisplayComponent.h"


using namespace juce;


// this only calls the callback ONCE
class TimedCallback : public Timer
{
public:
    TimedCallback(std::function<void()> callback, int interval) : mCallback(callback), mInterval(interval) {
        startTimer(mInterval);
    }

    ~TimedCallback() {
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
                           private ChangeListener
                        
                        
{
public:
    explicit MainComponent(const URL& initialFilePath = URL()): jobsFinished(0), totalJobs(0),
        jobProcessorThread(customJobs, jobsFinished, totalJobs, processBroadcaster)
    {
        // TODO - need to check extension of file and load appropriate DisplayComponent
        mediaDisplay = std::make_unique<MediaDisplayComponent>();
        mediaDisplay->addChangeListener(this);
        mediaDisplay->setupDisplay();
        addChildComponent(mediaDisplay.get());

        // Load the initial file
        if (initialFilePath.isLocalFile())
        {
            mediaDisplay->setTargetFilePath(initialFilePath);
            mediaDisplay->loadMediaFile(initialFilePath);
            mediaDisplay->generateTempFile();
            resized();
        }

        thread.startThread (Thread::Priority::normal);

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

        // initialize load and process buttons
        processButton.setButtonText("process");
        model->ready() ? processButton.setEnabled(true)
                        : processButton.setEnabled(false);
        addAndMakeVisible(processButton);
        processButton.onClick = [this] { // Process callback
            DBG("HARPProcessorEditor::buttonClicked button listener activated");

            // set the button text to "processing {model.card().name}"
            processButton.setButtonText("processing " + String(model->card().name) + "...");
            processButton.setEnabled(false);

            // enable the cancel button
            cancelButton.setEnabled(true);
            mediaDisplay->enableSaving(true);

            // TODO: get the current audio file and process it
            // if we don't have one, let the user know
            // TODO: need to only be able to do this if we don't have any other jobs in the threadpool right?
            if (model == nullptr){
                DBG("unhandled exception: model is null. we should probably open an error window here.");
                AlertWindow::showMessageBoxAsync(
                    AlertWindow::WarningIcon,
                    "Loading Error", 
                    "Model is not loaded. Please load a model first."
                );
                return;
            }


            // Check if the model's type (Audio or MIDI) matches the input file's type
            // If not, show an error message and ask the user to either use another model
            // or another appropriate file
            if (isAudio && isAudioModel) {
                DBG("Processing audio file");
            } else if (!isAudio && !isAudioModel) {
                DBG("Processing MIDI file");
            } else {
                DBG("Model and file type mismatch");
                AlertWindow::showMessageBoxAsync(
                    AlertWindow::WarningIcon,
                    "Processing Error",
                    "Model and file type mismatch. Please use an appropriate model or file."
                );
                // processBroadcaster.sendChangeMessage();
                resetProcessingButtons();
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
                    model->process(mediaDisplay->getTempFilePath().getLocalFile());
                    DBG("Processing finished");
                    // load the audio file again
                    processBroadcaster.sendChangeMessage();
                    
                }
            ));

            // Now the customJobs are ready to be added to be run in the threadPool
            jobProcessorThread.signalTask();
        };

        processBroadcaster.addChangeListener(this);

        cancelButton.setButtonText("cancel");
        cancelButton.setEnabled(false);
        addAndMakeVisible(cancelButton);
        cancelButton.onClick = [this] { // Cancel callback
            DBG("HARPProcessorEditor::buttonClicked cancel button listener activated");
            model->cancel();
        };


        loadModelButton.setButtonText("load");
        addAndMakeVisible(loadModelButton);
        loadModelButton.onClick = [this]{
            DBG("HARPProcessorEditor::buttonClicked load model button listener activated");

            // collect input parameters for the model.
            std::map<std::string, std::any> params = {
            {"url", modelPathComboBox.getText().toStdString()},
            };
            resetUI();
            // loading happens asynchronously.
            // the document controller trigger a change listener callback, which will update the UI
            threadPool.addJob([this, params] {
                DBG("executeLoad!!");
                try {
                    // timeout after 10 seconds
                    // TODO: this callback needs to be cleaned up in the destructor in case we quit
                    std::atomic<bool> success = false;
                    TimedCallback timedCallback([this, &success] {
                        if (success)
                            return;
                        DBG("HARPProcessorEditor::buttonClicked timedCallback listener activated");
                        AlertWindow::showMessageBoxAsync(
                            AlertWindow::WarningIcon,
                            "Loading Error",
                            "An error occurred while loading the WebModel: TIMED OUT! Please check that the space is awake."
                        );
                        model.reset(new WebWave2Wave());
                        loadBroadcaster.sendChangeMessage();
                        mediaDisplay->enableSaving(false);
                    }, 10000);

                    model->load(params);
                    success = true;
                    DBG("executeLoad done!!");
                    loadBroadcaster.sendChangeMessage();
                    // since we're on a helper thread, 
                    // it's ok to sleep for 10s 
                    // to let the timeout callback do its thing
                    Thread::sleep(10000);
                } catch (const std::runtime_error& e) {
                    AlertWindow::showMessageBoxAsync(
                        AlertWindow::WarningIcon,
                        "Loading Error",
                        String("An error occurred while loading the WebModel: ") + e.what()
                    );
                    model.reset(new WebWave2Wave());
                    loadBroadcaster.sendChangeMessage();
                    mediaDisplay->enableSaving(false);
                }
            });

            // disable the load button until the model is loaded
            loadModelButton.setEnabled(false);
            loadModelButton.setButtonText("loading...");

            // TODO: enable the cancel button
            // cancelButton.setEnabled(true);
            // if the cancel button is pressed, forget the job and reset the model and UI
            // cancelButton.onClick = [this] {
            //     DBG("HARPProcessorEditor::buttonClicked cancel button listener activated");
            //     threadPool.removeAllJobs(true, 1000);
            //     model->cancel();
            //     resetUI();
            // };

            // disable the process button until the model is loaded
            processButton.setEnabled(false);

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
            String spaceUrl = url;
            if (spaceUrl.contains("localhost") || spaceUrl.contains("huggingface.co") || spaceUrl.contains("http")) {
            DBG("HARPProcessorEditor::buttonClicked: spaceUrl is already a valid url");
            }
            else {
            DBG("HARPProcessorEditor::buttonClicked: spaceUrl is not a valid url");
            spaceUrl = "https://huggingface.co/spaces/" + spaceUrl;
            }
            spaceUrlButton.setButtonText("open " + url + " in browser");
            spaceUrlButton.setURL(URL(spaceUrl));
            addAndMakeVisible(spaceUrlButton);
        };

        loadBroadcaster.addChangeListener(this);

        std::string currentStatus = model->getStatus();
        if (currentStatus == "Status.LOADED" || currentStatus == "Status.FINISHED") {
            processButton.setEnabled(true);
            processButton.setButtonText("process");
        } else if (currentStatus == "Status.PROCESSING" || currentStatus == "Status.STARTING" || currentStatus == "Status.SENDING") {
            cancelButton.setEnabled(true);
            processButton.setButtonText("processing " + String(model->card().name) + "...");
        }
        

        // status label
        statusLabel.setText(currentStatus, dontSendNotification);
        addAndMakeVisible(statusLabel);

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
        "hugggof/MusicGen",
        "cwitkowitz/timbre-trap",
        };


        modelPathComboBox.setTextWhenNothingSelected("choose a model"); 
        for(size_t i = 0; i < modelPaths.size(); ++i) {
            modelPathComboBox.addItem(modelPaths[i], i+1);
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
                customPathWindow->addButton("OK", 1, KeyPress(KeyPress::returnKey));
                customPathWindow->addButton("Cancel", 0, KeyPress(KeyPress::escapeKey));
                
                // Show the window and handle the result asynchronously
                customPathWindow->enterModalState(true, new CustomPathAlertCallback([this, customPathWindow](int result) {
                    if (result == 1) { // OK was clicked
                        // Retrieve the entered path
                        String customPath = customPathWindow->getTextEditor("customPath")->getText();
                        // Use the custom path as needed
                        DBG("Custom path entered: " + customPath);
                        int new_id = modelPathComboBox.getNumItems()+1;
                        modelPathComboBox.addItem(customPath, new_id);
                        modelPathComboBox.setSelectedId(new_id);

                    } else { // Cancel was clicked or the window was closed
                        DBG("Custom path entry was canceled.");
                        modelPathComboBox.setSelectedId(0);
                    }
                    delete customPathWindow;
                }), true);
            }
        };


        addAndMakeVisible(modelPathComboBox);

        // glossary label
        glossaryLabel.setText("To view an index of available HARP-compatible models, please see our ", NotificationType::dontSendNotification);
        glossaryLabel.setJustificationType(Justification::centredRight);
        addAndMakeVisible(glossaryLabel);

        // glossary link
        glossaryButton.setButtonText("Model Glossary");
        glossaryButton.setURL(URL("https://github.com/audacitorch/HARP#available-models"));
        //glossaryButton.setJustificationType(Justification::centredLeft);
        addAndMakeVisible(glossaryButton);

        // model controls
        ctrlComponent.setModel(model);
        addAndMakeVisible(ctrlComponent);
        ctrlComponent.populateGui();

        addAndMakeVisible(nameLabel);
        addAndMakeVisible(authorLabel);
        addAndMakeVisible(descriptionLabel);
        addAndMakeVisible(tagsLabel);
        addAndMakeVisible(audioOrMidiLabel);

        // model card component
        // Get the modelCard from the EditorView
        auto &card = model->card();
        setModelCard(card);

        jobProcessorThread.startThread();

        
        
        // ARA requires that plugin editors are resizable to support tight integration
        // into the host UI
        setOpaque (true);
        setSize(800, 800);
        resized();
    }


    ~MainComponent() override
    {
       #if (JUCE_ANDROID || JUCE_IOS)
        chooseFileButton.removeListener (this);
       #else

       #endif

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
    }

    void paint (Graphics& g) override
    {
        g.fillAll (getUIColourIfAvailable (LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
    }

    void resized() override
    {
        auto area = getLocalBounds();
        auto margin = 10;  // Adjusted margin value for top and bottom spacing

        auto docViewHeight = 100;

        // auto mainArea = area.removeFromTop(area.getHeight() - docViewHeight);
        // auto documentViewArea = area;  // what remains is the 15% area for documentView
        auto mainArea = area;
        // pianoRoll.setBounds(mainArea.removeFromBottom (140));
        // return;
        // Row 1: Model Path TextBox and Load Model Button
        auto row1 = mainArea.removeFromTop(40);  // adjust height as needed
        modelPathComboBox.setBounds(row1.removeFromLeft(row1.getWidth() * 0.8f).reduced(margin));
        //modelPathTextBox.setBounds(row1.removeFromLeft(row1.getWidth() * 0.8f).reduced(margin));
        loadModelButton.setBounds(row1.reduced(margin));

        // Row 2: Glossary Label and Hyperlink
        auto row2 = mainArea.removeFromTop(30);  // adjust height as needed
        glossaryLabel.setBounds(row2.removeFromLeft(row2.getWidth() * 0.8f).reduced(margin));
        glossaryButton.setBounds(row2.reduced(margin));
        glossaryLabel.setFont(Font(11.0f));
        glossaryButton.setFont(Font(11.0f), false, Justification::centredLeft);

        // Row 3: Name and Author Labels
        auto row3a = mainArea.removeFromTop(40);  // adjust height as needed
        nameLabel.setBounds(row3a.removeFromLeft(row3a.getWidth() / 2).reduced(margin));
        nameLabel.setFont(Font(20.0f, Font::bold));
        // auto row425 = mainArea.removeFromTop(20);  // adjust height as needed
        // audioOrMidiLabel.setBounds(row3a.removeFromLeft(row3a.getWidth() / 5).reduced(margin));
        

        // nameLabel.setColour(Label::textColourId, mHARPLookAndFeel.textHeaderColor);
        auto row3c = mainArea.removeFromTop(30);
        audioOrMidiLabel.setBounds(row3c.reduced(margin));
        audioOrMidiLabel.setFont(Font(10.0f, Font::bold));
        audioOrMidiLabel.setColour(Label::textColourId, Colours::bisque);

        auto row3b = mainArea.removeFromTop(30);
        authorLabel.setBounds(row3b.reduced(margin));
        authorLabel.setFont(Font(12.0f));

        // Row 4: Description Label
        auto row4 = mainArea.removeFromTop(40);  // adjust height as needed
        descriptionLabel.setBounds(row4.reduced(margin));
        // TODO: put the space url below the description

        // // Row 4.25: audioOrMidiLabel
        // auto row425 = mainArea.removeFromTop(20);  // adjust height as needed
        // audioOrMidiLabel.setBounds(row425.reduced(margin));

        // Row 4.5: Space URL Hyperlink
        auto row45 = mainArea.removeFromTop(30);  // adjust height as needed
        spaceUrlButton.setBounds(row45.reduced(margin));
        spaceUrlButton.setFont(Font(11.0f), false, Justification::centredLeft);

        // Row 5: CtrlComponent (flexible height)
        auto row5 = mainArea.removeFromTop(150);  // the remaining area is for row 4
        ctrlComponent.setBounds(row5.reduced(margin));

        // Row 6: Process Button (taken out in advance to preserve its height)
        auto row6Height = 25;  // adjust height as needed
        auto row6 = mainArea.removeFromTop(row6Height);

        // Assign bounds to processButton
        processButton.setBounds(row6.withSizeKeepingCentre(100, 20));  // centering the button in the row

        // place the cancel button to the right of the process button (justified right)
        cancelButton.setBounds(processButton.getBounds().translated(110, 0));

        // place the status label to the left of the process button (justified left)
        statusLabel.setBounds(processButton.getBounds().translated(-200, 0));

        auto controls = mainArea.removeFromBottom (90);

        auto controlRightBounds = controls.removeFromRight (controls.getWidth() / 3);

        mainArea.removeFromBottom (6);

        auto thumbnailArea = mainArea.removeFromBottom (mainArea.getHeight() * 0.85f);
        mediaDisplay->setBounds(thumbnailArea.reduced(margin));
        // mainArea.removeFromBottom (6);
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
        // It is assumed we only support wav2wav or midi2midi models for now
        if (card.midi_in == "1" && card.midi_out == "1") {
            audioOrMidiLabel.setText("MIDI", dontSendNotification); // TODO: setting the text doesn't work for some reason
            isAudioModel = false;
        } else if (card.midi_in == "0" && card.midi_in == "0"){
            audioOrMidiLabel.setText("Audio", dontSendNotification);
            isAudioModel = true;
        } else {
            audioOrMidiLabel.setText("", dontSendNotification);
        }
        
    }


private:
    // HARP UI 
    std::unique_ptr<ModelStatusTimer> mModelStatusTimer {nullptr};
    ComboBox modelPathComboBox;
    TextButton loadModelButton;
    TextButton saveChangesButton {"save changes"};
    Label glossaryLabel;
    HyperlinkButton glossaryButton;
    TextButton processButton;
    TextButton cancelButton;
    Label statusLabel;

    CtrlComponent ctrlComponent;

    // model card
    Label nameLabel, authorLabel, descriptionLabel, tagsLabel;
    // For now it is assumed that both input and output types 
    // of the model are the same (audio or midi)
    Label audioOrMidiLabel;
    HyperlinkButton spaceUrlButton;

    // the model itself
    std::shared_ptr<WebWave2Wave> model {new WebWave2Wave()};

    TimeSliceThread thread  { "audio file preview" };

    std::unique_ptr<MediaDisplayComponent> mediaDisplay;

    // These flags are very simplistic as they assume
    // that we only have audio2audio or midi2midi models
    // We can expand these in the future to support more types
    // Flag to indicate audio vs midi (for input file)
    bool isAudio;
    // Flag to indicate audio vs midi (for type of model)
    bool isAudioModel;

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

    //==============================================================================

   #if (JUCE_ANDROID || JUCE_IOS)
    void buttonClicked (Button* btn) override
    {
        if (btn == &chooseFileButton && fileChooser.get() == nullptr)
        {
            if (! RuntimePermissions::isGranted (RuntimePermissions::readExternalStorage))
            {
                SafePointer<MainComponent> safeThis (this);
                RuntimePermissions::request (RuntimePermissions::readExternalStorage,
                                             [safeThis] (bool granted) mutable
                                             {
                                                 if (safeThis != nullptr && granted)
                                                     safeThis->buttonClicked (&safeThis->chooseFileButton);
                                             });
                return;
            }

            if (FileChooser::isPlatformDialogAvailable())
            {
                fileChooser = std::make_unique<FileChooser> ("Select an audio file...", File(), "*.wav;*.flac;*.aif");

                fileChooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                                          [this] (const FileChooser& fc) mutable
                                          {
                                              if (fc.getURLResults().size() > 0)
                                              {
                                                  auto u = fc.getURLResult();

                                                  addNewAudioFile (std::move (u));
                                              }

                                              fileChooser = nullptr;
                                          }, nullptr);
            }
            else
            {
                NativeMessageBox::showAsync (MessageBoxOptions()
                                               .withIconType (MessageBoxIconType::WarningIcon)
                                               .withTitle ("Enable Code Signing")
                                               .withMessage ("You need to enable code-signing for your iOS project and enable \"iCloud Documents\" "
                                                             "permissions to be able to open audio files on your iDevice. See: "
                                                             "https://forum.juce.com/t/native-ios-android-file-choosers"),
                                             nullptr);
            }
        }
    }
   #else
   #endif

    void resetProcessingButtons() {
        processButton.setButtonText("process");
        processButton.setEnabled(true);
        cancelButton.setEnabled(false);
        repaint();
    }

    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        if (source == &loadBroadcaster) {
            DBG("Setting up model card, CtrlComponent, resizing.");
            setModelCard(model->card());
            ctrlComponent.setModel(model);
            ctrlComponent.populateGui();
            repaint();

            // now, we can enable the buttons
            processButton.setEnabled(true);
            loadModelButton.setEnabled(true);
            loadModelButton.setButtonText("load");

            // Set the focus to the process button
            // so that the user can press SPACE to trigger the playback
            processButton.grabKeyboardFocus();
        }
        else if (source == &processBroadcaster) {
            // refresh the display for the new updated file
            URL tempFilePath = mediaDisplay->getTempFilePath();
            mediaDisplay->loadMediaFile(tempFilePath);

            // now, we can enable the process button
            resetProcessingButtons();
        }
        else if (source == mModelStatusTimer.get()) {
            // update the status label
            DBG("HARPProcessorEditor::changeListenerCallback: updating status label");
            statusLabel.setText(model->getStatus(), dontSendNotification);
        }
        else {
            DBG("HARPProcessorEditor::changeListenerCallback: unhandled change broadcaster");
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
