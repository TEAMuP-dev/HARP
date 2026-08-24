#include "MainComponent.h"

#include "windows/WelcomeWindow.h"

JUCE_IMPLEMENT_SINGLETON(HARPLogger)

MainComponent::MainComponent()
{
    HARPLogger::getInstance()->initializeLogger();

    initializeMenuBar();

    mainModelTab.addChangeListener(this);

    mainPanelViewport.setViewedComponent(&mainModelTab, false);
    mainPanelViewport.setScrollBarsShown(true, false);
    mainPanelViewport.setScrollOnDragMode(Viewport::ScrollOnDragMode::never);
    mainPanelViewport.onScrolled = [this] { refreshTutorialHighlight(); };
    addAndMakeVisible(mainPanelViewport);
    addAndMakeVisible(statusAreaWidget);
    addAndMakeVisible(mediaClipboardWidget);
    addAndMakeVisible(dragOverlay);

    showStatusArea = Settings::getBoolValue("view.showStatusArea", true);
    showMediaClipboard = Settings::getBoolValue("view.showMediaClipboard", false);
    dragOverlay.setVisible(false);
    dragOverlay.toFront(false);

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

    mainPanel.items.add(FlexItem(mainPanelViewport).withFlex(1.0));

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

    /* Give the tab the width the viewport can show and whatever height it needs,
       so that a window too small for the content scrolls instead of clipping it */
    auto layOutTab = [this]
    {
        const int visibleWidth = mainPanelViewport.getMaximumVisibleWidth();

        if (visibleWidth <= 0)
        {
            return 0;
        }

        const int requiredHeight = mainModelTab.getMinimumRequiredHeightForWidth(visibleWidth);

        mainModelTab.setSize(visibleWidth,
                             jmax(requiredHeight, mainPanelViewport.getMaximumVisibleHeight()));

        return visibleWidth;
    };

    /* Laying out once can make the scrollbar appear, which narrows the visible
       area and would leave it overlapping the content. Lay out again whenever the
       available width changed as a result. */
    const int firstWidth = layOutTab();

    if (firstWidth > 0 && mainPanelViewport.getMaximumVisibleWidth() != firstWidth)
    {
        layOutTab();
    }

    /* Deferred: the highlight is measured from component bounds, which are only
       final once this layout pass and the tab's own have completed. */
    refreshTutorialHighlight();

    dragOverlay.setBounds(getLocalBounds());
}

void MainComponent::refreshTutorialHighlight()
{
    if (welcomeWindow == nullptr)
    {
        return;
    }

    Component::SafePointer<MainComponent> safeThis(this);

    MessageManager::callAsync(
        [safeThis]
        {
            if (safeThis != nullptr && safeThis->welcomeWindow != nullptr)
            {
                safeThis->welcomeWindow->refreshHighlightForCurrentStep();
            }
        });
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
        /* The panel scrolls vertically, so the window does not have to be tall
           enough for every control; it only has to stay usably large. Width is
           still content-driven, since there is no horizontal scrolling. */
        const int requiredMainPanelHeight =
            minimumWindowHeight - minimumMainPanelVertPadding + (showStatusArea ? statusAreaHeight : 0);

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

    /* Whatever prompted this - a model loading, a panel being toggled - changed how
       much there is to show, and so how much the viewport has to scroll. The window
       itself may not have changed size, in which case nothing else would recompute
       the scrollable area and the scrollbar would not appear until the next resize. */
    resized();
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
    // The settings dialog is a free-standing desktop window that can outlive this
    // component, so guard the callback with a SafePointer
    Component::SafePointer<MainComponent> safeThis(this);
    options.content.setOwned(new SettingsWindow(
        [safeThis]
        {
            if (safeThis != nullptr)
                safeThis->restoreViewDefaults();
        }));

    options.useNativeTitleBar = true;
    options.resizable = true;
    options.escapeKeyTriggersCloseButton = true;

    options.launchAsync();
}

void MainComponent::restoreViewDefaults()
{
    // Defaults must match the fallbacks used when reading the settings
    // in the constructor: status area shown, media clipboard hidden
    if (! showStatusArea)
        viewStatusAreaCallback();

    if (showMediaClipboard)
        viewMediaClipboardCallback();

    // showWelcomePopup default (true) is already restored by clearing settings;
    // it will show on the next launch automatically.
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

/**
 * Converts a rectangle from the model tab's coordinates into this component's,
 * clipped to the part of the tab the viewport is showing.
 *
 * The tab can be taller than its viewport, so a component that is scrolled out of
 * view could otherwise produce a highlight lying over the status area beneath it.
 */
Rectangle<int> MainComponent::getVisibleTabArea(Rectangle<int> tabBounds)
{
    return getLocalArea(&mainModelTab, tabBounds)
        .getIntersection(mainPanelViewport.getBounds());
}

Rectangle<int> MainComponent::getModelSelectBounds()
{
    auto bounds = mainModelTab.getModelSelectBounds();
    return getVisibleTabArea(bounds);
}

Rectangle<int> MainComponent::getControlsBounds()
{
    auto bounds = mainModelTab.getControlsBounds();
    return getVisibleTabArea(bounds);
}

Rectangle<int> MainComponent::getInputTrackBounds()
{
    auto bounds = mainModelTab.getInputTrackBounds();
    return getVisibleTabArea(bounds);
}

Rectangle<int> MainComponent::getInputFolderBounds()
{
    auto bounds = mainModelTab.getInputFolderBounds();
    return getVisibleTabArea(bounds);
}

Rectangle<int> MainComponent::getInputPlayBounds()
{
    auto bounds = mainModelTab.getInputPlayBounds();
    return getVisibleTabArea(bounds);
}

Rectangle<int> MainComponent::getProcessButtonBounds()
{
    auto bounds = mainModelTab.getProcessButtonBounds();
    return getVisibleTabArea(bounds);
}

Rectangle<int> MainComponent::getTracksBounds()
{
    auto bounds = mainModelTab.getTracksBounds();
    return getVisibleTabArea(bounds);
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
