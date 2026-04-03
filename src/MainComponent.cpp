#include "MainComponent.h"

#include "windows/WelcomeWindow.h"

JUCE_IMPLEMENT_SINGLETON(HARPLogger)

MainComponent::MainComponent()
{
    HARPLogger::getInstance()->initializeLogger();

    initializeMenuBar();

    mainModelTab.addChangeListener(this);

    addAndMakeVisible(mainModelTab);
    addAndMakeVisible(statusAreaWidget);
    addAndMakeVisible(mediaClipboardWidget);

    showStatusArea = Settings::getBoolValue("view.showStatusArea", true);
    showMediaClipboard = Settings::getBoolValue("view.showMediaClipboard", false);

    requiredWindowWidth = minimumWindowWidth;
    requiredWindowHeight = minimumWindowHeight;
    setSize(requiredWindowWidth, requiredWindowHeight);

    sharedTokens->initializeAPIKeys();

    statusMessage->setMessage("Welcome to HARP!");
}

MainComponent::~MainComponent()
{
    deinitializeMenuBar();
    mainModelTab.removeChangeListener(this);
}

void MainComponent::paint(Graphics& g)
{
    g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
}

void MainComponent::paintOverChildren(Graphics& g)
{
    if (isTutorialActive)
    {
        auto area = getLocalBounds();
        g.setColour(Colours::black.withAlpha(0.6f));

        if (tutorialHighlightRect.isEmpty() && tutorialExtraHighlights.empty())
        {
            // Full dim if no highlight
            g.fillAll();
        }
        else
        {
            // Dim with cutout
            Path backgroundPath;
            backgroundPath.addRectangle(area.toFloat());

            Path highlightPath;
            if (! tutorialHighlightRect.isEmpty())
                highlightPath.addRoundedRectangle(tutorialHighlightRect.toFloat(), 5.0f);

            // Add extra highlights to the cutout path
            for (auto& rect : tutorialExtraHighlights)
            {
                highlightPath.addRoundedRectangle(rect.toFloat(), 5.0f);
            }

            backgroundPath.setUsingNonZeroWinding(false);
            backgroundPath.addPath(highlightPath);

            g.fillPath(backgroundPath);

            g.setColour(Colours::white);
            g.drawRoundedRectangle(tutorialHighlightRect.toFloat(), 5.0f, 2.0f);

            for (auto& rect : tutorialExtraHighlights)
            {
                g.drawRoundedRectangle(rect.toFloat(), 5.0f, 2.0f);
            }
        }
    }
}

void MainComponent::resized()
{
    Rectangle<int> fullArea = getLocalBounds();

#if not JUCE_MAC
    menuBar->setBounds(
        fullArea.removeFromTop(LookAndFeel::getDefaultLookAndFeel().getDefaultMenuBarHeight()));
#endif

    FlexBox fullWindow;
    fullWindow.flexDirection = FlexBox::Direction::row;

    FlexBox mainPanel;
    mainPanel.flexDirection = FlexBox::Direction::column;

    mainPanel.items.add(FlexItem(mainModelTab).withFlex(1.0));

    if (showStatusArea)
    {
        mainPanel.items.add(FlexItem(statusAreaWidget).withHeight(statusAreaHeight));
    }
    else
    {
        statusAreaWidget.setBounds(0, 0, 0, 0);
    }

    fullWindow.items.add(FlexItem(mainPanel).withFlex(1.0));

    if (showMediaClipboard)
    {
        fullWindow.items.add(FlexItem(mediaClipboardWidget).withFlex(mediaClipboardFlex));
    }
    else
    {
        mediaClipboardWidget.setBounds(0, 0, 0, 0);
    }

    fullWindow.performLayout(fullArea);

    if (welcomeWindow != nullptr)
    {
        welcomeWindow->refreshHighlightForCurrentStep();
    }
}

void MainComponent::updateWindowConstraints()
{
    if (auto* window = findParentComponentOfClass<DocumentWindow>())
    {
        // Compute percentage of total window width given to main panel
        const float mainPanelRatio = showMediaClipboard ? (1.0f / mediaClipboardScale) : 1.0f;

        // Determine minimum width needed to display controls plus padding
        const int requiredMainPanelWidth =
            jmax(minimumMainPanelWidth,
                 mainModelTab.getMinimumRequiredControlWidth() + minimumMainPanelHorPadding);
        // Determine current width of main panel
        const int mainPanelWidth = jmax(requiredMainPanelWidth, mainModelTab.getWidth());
        // Determine minimum height needed to display all model contents plus status widget
        const int requiredMainPanelHeight =
            mainModelTab.getMinimumRequiredHeightForWidth(mainPanelWidth)
            + (showStatusArea ? statusAreaHeight : 0);

        // Determine effective minimum width of entire window
        const int newRequiredWindowWidth = jmax(
            minimumWindowWidth, (int) std::ceil((float) requiredMainPanelWidth / mainPanelRatio));
        // Determine effective minimum height of entire window
        const int newRequiredWindowHeight =
            jmax(minimumWindowHeight, requiredMainPanelHeight + minimumMainPanelVertPadding);

        auto* currentDisplay =
            Desktop::getInstance().getDisplays().getDisplayForRect(window->getScreenBounds());

        const auto userArea =
            currentDisplay != nullptr
                ? currentDisplay->userArea
                : Rectangle<int>(0, 0, newRequiredWindowWidth, newRequiredWindowHeight);

        ComponentBoundsConstrainer* constrainer = window->getConstrainer();

        constrainer->setMinimumSize(jmin(newRequiredWindowWidth, userArea.getWidth()),
                                    jmin(newRequiredWindowHeight, userArea.getHeight()));
        constrainer->setMaximumSize(userArea.getWidth(), userArea.getHeight());
        constrainer->setMinimumOnscreenAmounts(40, 40, 40, 40);

        const bool constraintsDecreased = newRequiredWindowWidth < requiredWindowWidth
                                          || newRequiredWindowHeight < requiredWindowHeight;

        requiredWindowWidth = newRequiredWindowWidth;
        requiredWindowHeight = newRequiredWindowHeight;

        auto bounds = window->getBounds();

        window->setBoundsConstrained(bounds);

        if (constraintsDecreased)
        {
            // Small hack allowing immediate shrinking when constraints decrease
            window->setBounds(bounds.withWidth(bounds.getWidth() + 1));
            window->setBounds(bounds);
        }
    }
}

/* --File-- */

/**
 * Entry point for importing new files into HARP.
 */
void MainComponent::importNewFile(File mediaFile, bool fromDAW)
{
    mediaClipboardWidget.addTrackFromFilePath(URL(mediaFile), fromDAW);

    if (! showMediaClipboard)
    {
        viewMediaClipboardCallback();
    }
}

void MainComponent::openSettingsWindow()
{
    DialogWindow::LaunchOptions options;
    options.dialogTitle = "Settings";
    options.dialogBackgroundColour = Colours::darkgrey;
    options.content.setOwned(new SettingsWindow());

    options.useNativeTitleBar = true;
    options.resizable = true;
    options.escapeKeyTriggersCloseButton = true;

    options.launchAsync();
}

/* --View-- */

void MainComponent::viewStatusAreaCallback()
{
    // Toggle status Area visibility state
    showStatusArea = ! showStatusArea;

    // Find top-level window for resizing
    if (auto* window = findParentComponentOfClass<DocumentWindow>())
    {
        // Determine which display contains HARP
        auto* currentDisplay =
            Desktop::getInstance().getDisplays().getDisplayForRect(window->getScreenBounds());

        // Get current bounds of top-level window
        Rectangle<int> windowBounds = window->getBounds();

        // Default display height to height of current window
        int currentDisplayHeight = windowBounds.getHeight();

        if (currentDisplay != nullptr)
        {
            if (window->isFullScreen())
            {
                currentDisplayHeight = currentDisplay->totalArea.getHeight();
            }
            else
            {
                currentDisplayHeight = currentDisplay->userArea.getHeight();
            }
        }

        if (showStatusArea)
        {
            // Scale bounds to extend window by height of status area
            windowBounds.setHeight(
                jmin(currentDisplayHeight, windowBounds.getHeight() + statusAreaHeight));
        }
        else
        {
            if (! window->isFullScreen())
            {
                // Scale bounds to reduce window to main height
                windowBounds.setHeight(windowBounds.getHeight() - statusAreaHeight);
            }
        }

        // Set extended or reduced bounds
        window->setBounds(windowBounds);
    }

    // Add view preference to persistent settings
    Settings::setValue("view.showStatusArea", showStatusArea ? "1" : "0", true);

    // Send status message to add check to file menu
    commandManager.commandStatusChanged();

    updateWindowConstraints();
}

void MainComponent::viewMediaClipboardCallback()
{
    // Toggle media clipboard visibility state
    showMediaClipboard = ! showMediaClipboard;

    // Find top-level window for resizing
    if (auto* window = findParentComponentOfClass<DocumentWindow>())
    {
        // Determine which display contains HARP
        auto* currentDisplay =
            Desktop::getInstance().getDisplays().getDisplayForRect(window->getScreenBounds());

        // Get current bounds of top-level window
        Rectangle<int> windowBounds = window->getBounds();

        // Default display width to width of current window
        int currentDisplayWidth = windowBounds.getWidth();

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

        if (showMediaClipboard)
        {
            // Scale bounds to extend window by 40% of main width
            windowBounds.setWidth(
                jmin(currentDisplayWidth,
                     static_cast<int>(mediaClipboardScale * windowBounds.getWidth())));
        }
        else
        {
            if (! window->isFullScreen())
            {
                // Scale bounds to reduce window to main width
                windowBounds.setWidth(
                    static_cast<int>(windowBounds.getWidth() / mediaClipboardScale));
            }
        }

        // Set extended or reduced bounds
        window->setBounds(windowBounds);
    }

    // Add view preference to persistent settings
    Settings::setValue("view.showMediaClipboard", showMediaClipboard ? "1" : "0", true);

    // Send status message to add check to file menu
    commandManager.commandStatusChanged();

    updateWindowConstraints();
}

/* --Help-- */

void MainComponent::openAboutWindow()
{
    auto aboutComponent = std::make_unique<AboutWindow>();

    DialogWindow::LaunchOptions options;
    options.dialogTitle = "About " + String(APP_NAME);
    options.dialogBackgroundColour = Colours::grey;
    options.content.setOwned(aboutComponent.release());

    options.useNativeTitleBar = true;
    options.resizable = false;
    options.escapeKeyTriggersCloseButton = true;

    options.launchAsync();
}

void MainComponent::openWelcomeWindow(bool ensureDefaultModelLoaded)
{
    if (ensureDefaultModelLoaded)
        ensureTutorialModelLoaded();

    if (welcomeWindow != nullptr)
    {
        welcomeWindow->toFront(true);
        return;
    }

    Component::SafePointer<MainComponent> safeThis(this);
    MessageManager::callAsync(
        [safeThis]()
        {
            if (safeThis == nullptr)
                return;

            safeThis->welcomeWindow.reset(new WelcomeWindow(safeThis.getComponent()));
            safeThis->welcomeWindow->onClose = [safeThis]()
            {
                if (safeThis != nullptr)
                    safeThis->welcomeWindow.reset();
            };

            safeThis->welcomeWindow->setVisible(true);
            safeThis->welcomeWindow->positionOnMainComponentDisplay();
            safeThis->welcomeWindow->toFront(true);
        });
}

/* --Tutorial-- */

void MainComponent::setTutorialActive(bool active)
{
    isTutorialActive = active;
    repaint();
}

void MainComponent::setTutorialHighlight(Rectangle<int> bounds)
{
    tutorialHighlightRect = bounds;
    repaint();
}

void MainComponent::setTutorialExtraHighlights(std::vector<Rectangle<int>> bounds)
{
    tutorialExtraHighlights = bounds;
    repaint();
}

void MainComponent::ensureTutorialModelLoaded()
{
    if (! mainModelTab.isModelLoaded())
        mainModelTab.loadDefaultModel();
}

void MainComponent::resetTutorialAutoLoadedModel()
{
    if (! mainModelTab.isModelLoaded())
        return;

    if (mainModelTab.getLoadedPath() == TutorialConstants::fallbackModelPath)
    {
        mainModelTab.resetState();
    }
}

Rectangle<int> MainComponent::getModelSelectBounds()
{
    auto bounds = mainModelTab.getModelSelectBounds();
    return getLocalArea(&mainModelTab, bounds);
}

Rectangle<int> MainComponent::getControlsBounds()
{
    auto bounds = mainModelTab.getControlsBounds();
    return getLocalArea(&mainModelTab, bounds);
}

Rectangle<int> MainComponent::getInputTrackBounds()
{
    auto bounds = mainModelTab.getInputTrackBounds();
    return getLocalArea(&mainModelTab, bounds);
}

Rectangle<int> MainComponent::getInputFolderBounds()
{
    auto bounds = mainModelTab.getInputFolderBounds();
    return getLocalArea(&mainModelTab, bounds);
}

Rectangle<int> MainComponent::getInputPlayBounds()
{
    auto bounds = mainModelTab.getInputPlayBounds();
    return getLocalArea(&mainModelTab, bounds);
}

Rectangle<int> MainComponent::getProcessButtonBounds()
{
    auto bounds = mainModelTab.getProcessButtonBounds();
    return getLocalArea(&mainModelTab, bounds);
}

Rectangle<int> MainComponent::getTracksBounds()
{
    auto bounds = mainModelTab.getTracksBounds();
    return getLocalArea(&mainModelTab, bounds);
}

Rectangle<int> MainComponent::getClipboardBounds()
{
    if (showMediaClipboard && mediaClipboardWidget.isVisible())
        return mediaClipboardWidget.getBounds();
    return {};
}

Rectangle<int> MainComponent::getClipboardTrackAreaBounds()
{
    if (showMediaClipboard && mediaClipboardWidget.isVisible())
    {
        auto bounds = mediaClipboardWidget.getClipboardTrackAreaBounds();
        return getLocalArea(&mediaClipboardWidget, bounds);
    }
    return {};
}

Rectangle<int> MainComponent::getClipboardControlsBounds()
{
    if (showMediaClipboard && mediaClipboardWidget.isVisible())
    {
        auto bounds = mediaClipboardWidget.getClipboardControlsBounds();
        return getLocalArea(&mediaClipboardWidget, bounds);
    }
    return {};
}

Rectangle<int> MainComponent::getClipboardNameBoxBounds()
{
    if (showMediaClipboard && mediaClipboardWidget.isVisible())
    {
        auto bounds = mediaClipboardWidget.getClipboardNameBoxBounds();
        return getLocalArea(&mediaClipboardWidget, bounds);
    }
    return {};
}

Rectangle<int> MainComponent::getClipboardButtonsBounds()
{
    if (showMediaClipboard && mediaClipboardWidget.isVisible())
    {
        auto bounds = mediaClipboardWidget.getClipboardButtonsBounds();
        return getLocalArea(&mediaClipboardWidget, bounds);
    }
    return {};
}

Rectangle<int> MainComponent::getClipboardAddButtonBounds()
{
    if (showMediaClipboard && mediaClipboardWidget.isVisible())
    {
        auto bounds = mediaClipboardWidget.getAddFileButtonBounds();
        return getLocalArea(&mediaClipboardWidget, bounds);
    }
    return {};
}

Rectangle<int> MainComponent::getClipboardRemoveButtonBounds()
{
    if (showMediaClipboard && mediaClipboardWidget.isVisible())
    {
        auto bounds = mediaClipboardWidget.getRemoveButtonBounds();
        return getLocalArea(&mediaClipboardWidget, bounds);
    }
    return {};
}

Rectangle<int> MainComponent::getClipboardPlayButtonBounds()
{
    if (showMediaClipboard && mediaClipboardWidget.isVisible())
    {
        auto bounds = mediaClipboardWidget.getPlayButtonBounds();
        return getLocalArea(&mediaClipboardWidget, bounds);
    }
    return {};
}

Rectangle<int> MainComponent::getClipboardSendToDAWButtonBounds()
{
    if (showMediaClipboard && mediaClipboardWidget.isVisible())
    {
        auto bounds = mediaClipboardWidget.getSendToDAWButtonBounds();
        return getLocalArea(&mediaClipboardWidget, bounds);
    }
    return {};
}

Rectangle<int> MainComponent::getInfoBarBounds()
{
    if (showStatusArea && statusAreaWidget.isVisible())
        return statusAreaWidget.getBounds();
    return {};
}

/* --Miscellaneous-- */

// TODO - The following is an old callback from V2. It may be helpful in the future.

/*
void MainComponent::focusCallback()
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
                            DBG_AND_LOG("Reloading file");
                            loadMediaDisplay(mediaDisplay->getTargetFilePath().getLocalFile());
                        }
                        else
                        { // No was clicked or the window was closed
                            DBG_AND_LOG("Not reloading file");
                            lastLoadTime =
                                Time::getCurrentTime(); //Reset time so we stop asking
                        }
                        delete reloadCheckWindow;
                    }),
                true);
        }
    }
}
*/

void MainComponent::changeListenerCallback(ChangeBroadcaster* source)
{
    if (source == &mainModelTab)
    {
        updateWindowConstraints();
    }
}
