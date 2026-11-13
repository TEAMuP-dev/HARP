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

#include "../../gui/Palette.h"

#include "../../utils/Interface.h"
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
    // Begins a line of the description that is shown as a heading, without the mark
    static constexpr const char* headingMark = "# ";

    String title;
    String description;
    std::vector<TutorialTargets::Target> highlights = {};
    std::vector<TutorialHost::Panel> panelsToShow = {};
};

class TutorialWindow : public DocumentWindow, private ChangeListener
{
public:
    // Loaded on the user's behalf when they continue past model selection without one
    static constexpr const char* fallbackModelPath = "teamup-tech/Demucs";

    explicit TutorialWindow(TutorialHost& tutorialHost)
        : DocumentWindow(
              "Welcome to HARP",
              getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground),
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

        // Steps aside for a popup, which opens where this window floats, until it is dismissed
        overlay.onPopupShowingChanged = [this](bool isShowing) { setVisible(! isShowing); };

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
        g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
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
                  + "# 1. Add input\n"
                    "Drag an audio or MIDI file onto an input track, or click its folder icon to choose one. Its play icon previews it.\n\n"
                    "# 2. Process\n"
                    "Click Process to run the model on your input. While it runs, the same button cancels it.\n\n"
                    "# 3. Get the output\n"
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
              "Select a file to play it in the preview pane beneath the list, or to save or remove it. Double-click its name to rename it, and drag it to reorder the list or onto a track.\n\n"
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
            // A load is already running, and the step advances by itself once the model
            // arrives (see changeListenerCallback)
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
        Settings::setValue(
            "view.showWelcomePopup", ! content->dontShowAgainToggle.getToggleState(), true);

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
            // Headed as the About window is, with the step in place of the version
            titleLabel.setFont(ModelStyle::font(24.0f, true));
            addAndMakeVisible(titleLabel);

            pageIndicator.setFont(ModelStyle::font(textHeight));
            pageIndicator.setColour(Label::textColourId, ModelStyle::secondaryText);
            addAndMakeVisible(pageIndicator);

            descriptionEditor.setMultiLine(true);
            descriptionEditor.setReadOnly(true);
            descriptionEditor.setScrollbarsShown(true);
            descriptionEditor.setCaretVisible(false);
            // Looks like a label, and starts at the same edge as the labels around it
            for (const auto colourID : { TextEditor::backgroundColourId,
                                         TextEditor::outlineColourId,
                                         TextEditor::focusedOutlineColourId })
            {
                descriptionEditor.setColour(colourID, Colours::transparentBlack);
            }
            descriptionEditor.setBorder({});
            descriptionEditor.setIndents(0, 0);
            descriptionEditor.setLineSpacing(lineSpacing);
            addAndMakeVisible(descriptionEditor);

            learnMoreLink.setFont(ModelStyle::font(textHeight), false, Justification::centredLeft);
            learnMoreLink.setColour(HyperlinkButton::textColourId, ModelStyle::accent);
            addAndMakeVisible(learnMoreLink);

            copyrightLabel.setText(APP_COPYRIGHT, dontSendNotification);
            copyrightLabel.setFont(ModelStyle::font(12.0f));
            copyrightLabel.setColour(Label::textColourId, ModelStyle::secondaryText);
            addAndMakeVisible(copyrightLabel);

            for (auto* label : { &titleLabel, &pageIndicator, &copyrightLabel })
            {
                label->setJustificationType(Justification::centredLeft);
                label->setBorderSize({ 0, 0, 0, 0 });
            }

            nextButton.onClick = [this] { owner.nextStep(); };
            addAndMakeVisible(nextButton);

            prevButton.onClick = [this] { owner.previousStep(); };
            addChildComponent(prevButton);

            skipButton.onClick = [this] { owner.finish(); };
            addAndMakeVisible(skipButton);

            dontShowAgainToggle.setToggleState(
                ! Settings::getBoolValue("view.showWelcomePopup", true), dontSendNotification);
            addAndMakeVisible(dontShowAgainToggle);

            showDetailsButton.onClick = [this] { owner.toggleControlDetails(); };
            addChildComponent(showDetailsButton);

            /* The widths are measured once. The button that lists the controls is wide enough
               for either of its two texts. */
            skipButtonWidth = skipButton.getBestWidthForHeight(buttonHeight);

            for (const auto* text : { hideDetailsText, showDetailsText })
            {
                showDetailsButton.setButtonText(text);
                showDetailsButtonWidth = jmax(
                    showDetailsButtonWidth, showDetailsButton.getBestWidthForHeight(buttonHeight));
            }

            dontShowAgainToggle.setSize(0, buttonHeight);
            dontShowAgainToggle.changeWidthToFitText();
            toggleWidth = dontShowAgainToggle.getWidth();

            setSize(windowWidth, windowHeight);
        }

        void paint(Graphics& g) override
        {
            g.fillAll(
                getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
        }

        void showStep(const TutorialStep& step, int index, int numSteps, bool offersDetails)
        {
            titleLabel.setText(step.title, dontSendNotification);
            pageIndicator.setText("Step " + String(index + 1) + " of " + String(numSteps),
                                  dontSendNotification);

            showDescription(step.description);

            const bool isLastStep = index == numSteps - 1;

            prevButton.setVisible(index > 0);
            skipButton.setVisible(! isLastStep);
            nextButton.setButtonText(isLastStep ? "Finish" : "Next");

            learnMoreLink.setVisible(index == 0);
            copyrightLabel.setVisible(index == 0);

            showDetailsButton.setVisible(offersDetails);
            showDetailsButton.setButtonText(showingDetails ? hideDetailsText : showDetailsText);

            resized();
        }

        void resized() override
        {
            FlexBox column;
            column.flexDirection = FlexBox::Direction::column;

            auto addRow = [&column](Component& component, int height, int marginAbove = 0)
            {
                column.items.add(FlexItem(component)
                                     .withHeight((float) height)
                                     .withMargin({ (float) marginAbove, 0, 0, 0 }));
            };

            addRow(titleLabel, titleHeight);
            addRow(pageIndicator, rowHeight);

            /* The text takes whatever height the rows around it leave, and scrolls where a step
               needs more. What a step adds beneath it therefore keeps its place, as the button
               that lists the controls has to while the list comes and goes. */
            column.items.add(FlexItem(descriptionEditor)
                                 .withFlex(1)
                                 .withMargin({ (float) sectionGap, 0, 0, 0 }));

            if (learnMoreLink.isVisible())
            {
                addRow(learnMoreLink, rowHeight, rowGap);
            }

            if (showDetailsButton.isVisible())
            {
                column.items.add(FlexItem(showDetailsButton)
                                     .withWidth((float) showDetailsButtonWidth)
                                     .withHeight((float) buttonHeight)
                                     .withAlignSelf(FlexItem::AlignSelf::flexStart)
                                     .withMargin({ (float) rowGap, 0, 0, 0 }));
            }

            if (copyrightLabel.isVisible())
            {
                addRow(copyrightLabel, rowHeight);
            }

            /* Leaving the tutorial is on the left and moving through it on the right, so that
               the buttons used most keep their place from step to step */
            FlexBox footer;

            auto addToFooter = [&footer](Component& component, int width, int marginLeft = 0)
            {
                footer.items.add(FlexItem(component)
                                     .withWidth((float) width)
                                     .withMargin({ 0, 0, 0, (float) marginLeft }));
            };

            if (skipButton.isVisible())
            {
                addToFooter(skipButton, skipButtonWidth);
            }

            addToFooter(dontShowAgainToggle, toggleWidth, skipButton.isVisible() ? buttonGap : 0);

            footer.items.add(FlexItem().withFlex(1));

            if (prevButton.isVisible())
            {
                addToFooter(prevButton, stepButtonWidth);
            }

            addToFooter(nextButton, stepButtonWidth, buttonGap);

            column.items.add(FlexItem(footer)
                                 .withHeight((float) buttonHeight)
                                 .withMargin({ (float) sectionGap, 0, 0, 0 }));

            column.performLayout(getLocalBounds().reduced(pagePadding));
        }

        Label titleLabel;
        Label pageIndicator;
        TextEditor descriptionEditor;

        TextButton nextButton { "Next" };
        TextButton prevButton { "Back" };
        TextButton skipButton { "Skip Tutorial" };
        ToggleButton dontShowAgainToggle { "Don't show again" };

        HyperlinkButton learnMoreLink { "Learn more on the HARP website",
                                        URL("https://harp.tools/intro.html") };
        Label copyrightLabel;

        TextButton showDetailsButton { showDetailsText };

        int currentStep = 0;
        bool showingDetails = false;

    private:
        // Shows the text of a step, with each line that begins with the heading mark in bold
        void showDescription(const String& description)
        {
            descriptionEditor.clear();

            StringArray lines;
            lines.addLines(description);

            for (int i = 0; i < lines.size(); ++i)
            {
                const bool isHeading = lines[i].startsWith(TutorialStep::headingMark);

                // The font of the editor applies to whatever is added next
                descriptionEditor.setFont(ModelStyle::font(textHeight, isHeading));
                descriptionEditor.moveCaretToEnd();
                descriptionEditor.insertTextAtCaret(
                    (isHeading
                         ? lines[i].fromFirstOccurrenceOf(TutorialStep::headingMark, false, false)
                         : lines[i])
                    + (i < lines.size() - 1 ? "\n" : ""));
            }

            descriptionEditor.setCaretPosition(0);
        }

        static constexpr const char* showDetailsText = "List control descriptions";
        static constexpr const char* hideDetailsText = "Hide control descriptions";

        static constexpr int windowWidth = 480;
        static constexpr int windowHeight = 380;
        // Space around the contents, and the sizes of the rows, as in the Settings and About windows
        static constexpr int pagePadding = 14;
        static constexpr int titleHeight = 30;
        static constexpr int rowHeight = 22;
        static constexpr int rowGap = 4;
        static constexpr int sectionGap = 12;
        static constexpr int buttonHeight = 28;
        static constexpr int buttonGap = 8;
        // Wide enough for "Back", "Next", and "Finish" alike
        static constexpr int stepButtonWidth = 84;

        static constexpr float textHeight = 14.0f;
        static constexpr float lineSpacing = 1.15f;

        int skipButtonWidth = 0;
        int showDetailsButtonWidth = 0;
        int toggleWidth = 0;

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
