/**
 * @file TutorialWindow.h
 * @brief Window walking a new user through HARP, step by step.
 * @author cwitkowitz, saumya-pailwan
 */

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../../ModelTabContainer.h"

#include "../../utils/Settings.h"

#include "TutorialOverlay.h"
#include "TutorialTargets.h"

using namespace juce;

/**
 * What the tutorial needs from the rest of the application.
 */
struct TutorialHost
{
    virtual ~TutorialHost() = default;

    // The area the tutorial dims, within which it finds what it points at
    virtual Component& getTutorialArea() = 0;

    virtual ModelTabContainer& getModelTabs() = 0;

    // Parts of the window the user can hide, which a step that points at one shows again
    enum class Panel
    {
        statusArea,
        mediaClipboard
    };

    virtual void showPanel(Panel panel) = 0;
};

struct TutorialStep
{
    String title;
    String description;
    std::vector<TutorialTargets::Target> highlights = {};
    std::vector<TutorialHost::Panel> panelsToShow = {};
};

class TutorialWindow : public DocumentWindow, private ChangeListener
{
public:
    // Loaded on the user's behalf when they continue past model selection without one
    static constexpr const char* fallbackModelPath = "teamup-tech/demucs-source-separation";

    explicit TutorialWindow(TutorialHost& tutorialHost)
        : DocumentWindow("Welcome to HARP",
                         Desktop::getInstance().getDefaultLookAndFeel().findColour(
                             ResizableWindow::backgroundColourId),
                         DocumentWindow::closeButton),
          host(tutorialHost),
          overlay(host.getTutorialArea())
    {
        setUsingNativeTitleBar(true);
        content = new TutorialContent(*this);
        setContentOwned(content, true);

        setAlwaysOnTop(true);
        setResizable(false, false);

        setConstrainer(&constrainer);
        constrainer.setMinimumSize(content->getWidth(), content->getHeight());
        constrainer.setMaximumSize(content->getWidth(), content->getHeight());
        constrainer.setMinimumOnscreenAmounts(40, 40, 40, 40);

        positionOnHostDisplay();

        host.getModelTabs().addChangeListener(this);

        rebuildSteps();
        updateStep();
    }

    ~TutorialWindow() override { host.getModelTabs().removeChangeListener(this); }

    void closeButtonPressed() override
    {
        if (onClose)
            onClose();
    }

    std::function<void()> onClose;

    void paint(Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(ResizableWindow::backgroundColourId));
    }

    void positionOnHostDisplay()
    {
        const int width = content->getWidth();
        const int height = content->getHeight();
        setSize(width, height);

        Rectangle<int> targetBounds;

        if (auto* topLevel = host.getTutorialArea().getTopLevelComponent())
            targetBounds = topLevel->getScreenBounds();

        if (targetBounds.isEmpty())
        {
            centreWithSize(width, height);
            return;
        }

        setTopLeftPosition(targetBounds.getCentreX() - width / 2,
                           targetBounds.getCentreY() - height / 2);
    }

private:
    /* --Model state-- */

    // Called whenever a model tab is opened, closed, or selected, or finishes loading
    void changeListenerCallback(ChangeBroadcaster*) override
    {
        rebuildSteps();

        auto model = getCurrentModel();

        if (model == nullptr || ! model->isLoaded())
        {
            updateStep();
            return;
        }

        // Once a different model is showing, whether it just loaded or its tab was selected,
        // restart at "Quick Start", since the steps from there on describe that model
        if (content->currentStep > 0 && model != describedModel.lock())
            content->currentStep = quickStartStep;

        describedModel = model;

        updateStep();
    }

    // The tutorial describes the tab that is showing
    std::shared_ptr<Model> getCurrentModel() const
    {
        auto* tab = host.getModelTabs().getCurrentModelTab();
        return tab != nullptr ? tab->getModel() : nullptr;
    }

    bool isModelLoadedInCurrentTab() const
    {
        auto model = getCurrentModel();
        return model != nullptr && model->isLoaded();
    }

    bool isModelLoadingInCurrentTab() const
    {
        auto* tab = host.getModelTabs().getCurrentModelTab();
        return tab != nullptr && tab->isLoading();
    }

    void loadFallbackModel()
    {
        // A model tab that is showing either has a model or is loading one, and in both
        // cases the tutorial describes that one
        if (host.getModelTabs().getCurrentModelTab() == nullptr)
            openedTab = host.getModelTabs().openModelTab(fallbackModelPath);
    }

    /* --Steps-- */

    void rebuildSteps()
    {
        using namespace TutorialTargets;

        steps.clear();

        steps.push_back(
            { "Welcome to HARP",
              "HARP (hosted, asynchronous, remote processing) runs machine learning models on your audio and MIDI. The models are hosted online, e.g., as Hugging Face Spaces, so there is nothing to install.\n\n"
              "Use HARP as a standalone app, or as an external sample editor in your DAW.\n\n"
              "This short tutorial covers the main parts of the interface." });

        steps.push_back(
            { "Select a Model",
              "The Home tab lists the available models by category. Search them by name, task, or tag, or filter them by category, and hover over one to see its full description.\n\n"
              "Click a model to open it in a tab of its own. To add one that is not listed, such as a Gradio app of your own, click Custom Path.\n\n"
              "If you click Next without opening a model, HARP opens Demucs, which separates music into stems.",
              { { modelSearch, {}, tabBar }, { modelList } } });

        auto model = getCurrentModel();
        const bool isLoaded = model != nullptr && model->isLoaded();
        const String modelName = isLoaded ? String(model->getMetadata().name) : String();

        /* The model is pointed out rather than described here, since its card is already shown
           at the top of its tab, and a long description would push the steps out of view */
        steps.push_back(
            { "Quick Start",
              (isLoaded ? "This is " + modelName + ". Its details are at the top of its tab.\n\n"
                        : String())
                  + "1. Add input\n"
                    "Drag an audio or MIDI file onto an input track, or click its folder icon to choose one. Its play icon previews it.\n\n"
                    "2. Process\n"
                    "Click Process to run the model on your input. While it runs, the same button cancels it.\n\n"
                    "3. Get the output\n"
                    "The results appear in the output tracks, ready to play or save.",
              { { modelTabs },
                { trackFolderButton, inputTracks },
                { trackPlayButton, inputTracks },
                { processButton } } });

        // Only a model with controls has any to point out
        if (isLoaded && ! model->getControls().empty())
        {
            steps.push_back({ controlsStepTitle, describeControls(*model), { { controls } } });
        }

        steps.push_back(
            { "Tracks",
              "Input tracks hold what the model reads, and output tracks what it produces.\n\n"
              "Click or drag within a track to move its playback position, and scroll over it to zoom or pan.\n\n"
              "To use a track elsewhere, drag it onto another track, into the media clipboard, or into your DAW. Output tracks can also be saved, or copied for pasting elsewhere.",
              { { inputTracks }, { outputTracks } } });

        steps.push_back(
            { "Status and Instructions",
              "The bar at the bottom has two halves.\n\n"
              "The left half is a history of what each model has done, e.g., READY once it has loaded, or PROCESSING while it runs. If something fails, the reason appears here.\n\n"
              "The right half explains whatever is under the mouse. Hover over any part of HARP to learn what it does.",
              { { statusArea } },
              { TutorialHost::Panel::statusArea } });

        steps.push_back(
            { "Media Clipboard",
              "The media clipboard keeps files at hand across models and tabs. Drag tracks into it, or add files with its folder icon.\n\n"
              "Select a file to play, save, rename (in the text box), or remove it, or drag it onto a track.\n\n"
              "When your DAW opened HARP with a file, the send icon replaces that file with the selected one, which brings the result back into your project.",
              { { clipboard }, { clipboardControls } },
              { TutorialHost::Panel::mediaClipboard } });

        steps.push_back(
            { "All Set!",
              "Models on Hugging Face work without an account, but a Hugging Face token raises your allowance for models marked ZeroGPU, and gives access to private Spaces. Stability AI models require a Stability AI key.\n\n"
              "Add these under File > Settings > API Keys, which also links to where each service issues them.\n\n"
              "To see this tutorial again, choose Help > Welcome Tutorial." });

        content->currentStep = jlimit(0, (int) steps.size() - 1, content->currentStep);
    }

    String describeControls(Model& model) const
    {
        String text =
            "Most models have controls that adjust how they run, e.g., to trade quality for speed, or to choose a variant of the model. The defaults are a good place to start.\n\n"
            "Hover over a control to see what it does, or click below to list all of them.";

        if (! content->showingDetails)
            return text;

        text += "\n";

        for (const auto& info : model.getControls())
        {
            const auto description = String(info->info).trim();

            text += "\n- " + String(info->label) + ": "
                    + (description.isNotEmpty() ? description : "No description provided.");
        }

        return text;
    }

    /* --Navigation-- */

    void updateStep()
    {
        const auto& step = steps[(size_t) content->currentStep];

        for (const auto panel : step.panelsToShow)
            host.showPanel(panel);

        overlay.setTargets(step.highlights);

        content->showStep(
            step, content->currentStep, (int) steps.size(), step.title == controlsStepTitle);
    }

    void nextStep()
    {
        if (content->currentStep == selectModelStep && ! isModelLoadedInCurrentTab())
        {
            // A load is already running; the step advances by itself once the model arrives
            // (see changeListenerCallback)
            if (isModelLoadingInCurrentTab())
                return;

            if (! attemptedFallbackLoad)
            {
                attemptedFallbackLoad = true;
                loadFallbackModel();
                return;
            }

            // The fallback load did not succeed. Carry on rather than leaving the user stuck
            // on this step with a button that does nothing.
        }

        if (content->currentStep < (int) steps.size() - 1)
        {
            content->currentStep++;
            updateStep();
        }
        else
        {
            finish();
        }
    }

    void previousStep()
    {
        if (content->currentStep > 0)
        {
            content->currentStep--;
            updateStep();
        }
    }

    void toggleControlDetails()
    {
        content->showingDetails = ! content->showingDetails;

        rebuildSteps();
        updateStep();
    }

    void finish()
    {
        Settings::setValue("view.showWelcomePopup",
                           content->dontShowAgainToggle.getToggleState() ? "0" : "1",
                           true);

        // Close the tab the tutorial opened on the user's behalf. Resetting it in place would
        // leave a blank tab behind, since models are chosen on the Home tab.
        if (auto* tab = openedTab.getComponent())
            host.getModelTabs().closeTab(tab);

        closeButtonPressed();
    }

    /* --Layout-- */

    class TutorialContent : public Component
    {
    public:
        explicit TutorialContent(TutorialWindow& ownerWindow) : owner(ownerWindow)
        {
            titleLabel.setFont(Font(FontOptions(24.0f, Font::bold)));
            titleLabel.setJustificationType(Justification::centred);
            addAndMakeVisible(titleLabel);

            descriptionEditor.setMultiLine(true);
            descriptionEditor.setReadOnly(true);
            descriptionEditor.setScrollbarsShown(true);
            descriptionEditor.setCaretVisible(false);
            // Look like a label
            descriptionEditor.setColour(TextEditor::backgroundColourId, Colours::transparentBlack);
            descriptionEditor.setColour(TextEditor::outlineColourId, Colours::transparentBlack);
            descriptionEditor.setFont(Font(FontOptions(16.0f)));
            addAndMakeVisible(descriptionEditor);

            addAndMakeVisible(learnMoreLink);

            copyrightLabel.setText("Copyright 2026 TEAMuP. All rights reserved.",
                                   dontSendNotification);
            copyrightLabel.setJustificationType(Justification::centred);
            copyrightLabel.setFont(Font(FontOptions(12.0f)));
            copyrightLabel.setColour(Label::textColourId, Colours::grey);
            addAndMakeVisible(copyrightLabel);

            nextButton.onClick = [this] { owner.nextStep(); };
            addAndMakeVisible(nextButton);

            prevButton.onClick = [this] { owner.previousStep(); };
            addChildComponent(prevButton);

            skipButton.onClick = [this] { owner.finish(); };
            addAndMakeVisible(skipButton);

            dontShowAgainToggle.setToggleState(
                ! Settings::getBoolValue("view.showWelcomePopup", true), dontSendNotification);
            addAndMakeVisible(dontShowAgainToggle);

            pageIndicator.setJustificationType(Justification::centredLeft);
            pageIndicator.setFont(Font(FontOptions(12.0f)));
            pageIndicator.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(pageIndicator);

            showDetailsButton.onClick = [this] { owner.toggleControlDetails(); };
            addChildComponent(showDetailsButton);

            setSize(500, 420);
        }

        void showStep(const TutorialStep& step, int index, int numSteps, bool offersDetails)
        {
            titleLabel.setText(step.title, dontSendNotification);
            descriptionEditor.setText(step.description);
            descriptionEditor.setCaretPosition(0);

            const bool isLastStep = index == numSteps - 1;

            prevButton.setVisible(index > 0);
            skipButton.setVisible(! isLastStep);
            nextButton.setButtonText(isLastStep ? "Finish" : "Next");

            learnMoreLink.setVisible(index == 0);
            copyrightLabel.setVisible(index == 0);

            pageIndicator.setText("Step " + String(index + 1) + " of " + String(numSteps),
                                  dontSendNotification);

            showDetailsButton.setVisible(offersDetails);
            showDetailsButton.setButtonText(showingDetails ? "Hide control descriptions"
                                                           : "List control descriptions");

            resized();
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(20);

            titleLabel.setBounds(area.removeFromTop(40));
            area.removeFromTop(10);

            auto footerArea = area.removeFromBottom(72);
            auto footer = footerArea.removeFromTop(30);
            auto toggleRow = footerArea.removeFromBottom(30);

            const int buttonHeight = 30;
            const int buttonPaddingX = 28;
            const int buttonWidth =
                jmax(skipButton.getBestWidthForHeight(buttonHeight) + buttonPaddingX,
                     nextButton.getBestWidthForHeight(buttonHeight) + buttonPaddingX);
            skipButton.setBounds(footer.removeFromLeft(buttonWidth));
            nextButton.setBounds(footer.removeFromRight(buttonWidth));
            footer.removeFromRight(10);
            prevButton.setBounds(footer.removeFromRight(buttonWidth));

            const int middleRightX =
                prevButton.isVisible() ? (prevButton.getX() - 10) : nextButton.getX();
            const int middleX = skipButton.getRight() + 12;
            copyrightLabel.setBounds(
                middleX, footer.getY(), jmax(0, middleRightX - middleX), footer.getHeight());

            constexpr int toggleHeight = 26;
            const int toggleTextWidth = GlyphArrangement::getStringWidthInt(
                Font(FontOptions(16.0f)), dontShowAgainToggle.getButtonText());
            const int toggleWidth = 24 + toggleTextWidth + 16;
            const int toggleY = toggleRow.getY() + 12;
            dontShowAgainToggle.setBounds(
                toggleRow.getRight() - toggleWidth, toggleY, toggleWidth, toggleHeight);

            const int stepWidth = jmax(skipButton.getWidth(),
                                       GlyphArrangement::getStringWidthInt(pageIndicator.getFont(),
                                                                           pageIndicator.getText())
                                           + 16);
            pageIndicator.setBounds(toggleRow.getX(), toggleY, stepWidth, toggleHeight);

            area.removeFromBottom(8);

            if (learnMoreLink.isVisible())
            {
                learnMoreLink.setBounds(area.removeFromBottom(36).withSizeKeepingCentre(180, 30));
                area.removeFromBottom(4);
            }
            else if (showDetailsButton.isVisible())
            {
                showDetailsButton.setBounds(area.removeFromBottom(30).reduced(20, 0));
                area.removeFromBottom(8);
            }

            descriptionEditor.setBounds(area);
        }

        Label titleLabel;
        TextEditor descriptionEditor;

        TextButton nextButton { "Next" };
        TextButton prevButton { "Back" };
        TextButton skipButton { "Skip Tutorial" };
        ToggleButton dontShowAgainToggle { "Don't show again" };

        Label pageIndicator;
        HyperlinkButton learnMoreLink { "Learn more",
                                        URL("https://harp3.netlify.app/content/intro.html") };
        Label copyrightLabel;

        TextButton showDetailsButton;

        int currentStep = 0;
        bool showingDetails = false;

    private:
        TutorialWindow& owner;
    };

    static constexpr int selectModelStep = 1;
    static constexpr int quickStartStep = 2;
    static inline const String controlsStepTitle { "Controls (Optional)" };

    TutorialHost& host;

    // Declared after host, which it needs to be constructed
    TutorialOverlay overlay;

    TutorialContent* content = nullptr;
    std::vector<TutorialStep> steps;

    // Whether the tutorial has already asked for the fallback model on the user's behalf, so
    // that a failed load does not trap them on that step
    bool attemptedFallbackLoad = false;

    // The tab the tutorial opened on the user's behalf, closed again at the end
    Component::SafePointer<ModelTab> openedTab;

    // The model the steps were last written for, so that only a different one restarts them
    std::weak_ptr<Model> describedModel;

    ComponentBoundsConstrainer constrainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TutorialWindow)
};
