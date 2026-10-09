#include "MainComponent.h"

JUCE_IMPLEMENT_SINGLETON(HARPLogger)

MainComponent::MainComponent()
{
    HARPLogger::getInstance()->initializeLogger();

    initializeMenuBar();

    modelTabs.addChangeListener(this);

    addAndMakeVisible(modelTabs);
    addAndMakeVisible(statusAreaWidget);
    addAndMakeVisible(mediaClipboardWidget);
    mediaClipboardWidget.onResize = [this](int newWidth)
    {
        // Leave the main panel at least the width its controls need
        const int maximumClipboardWidth = getWidth() - getRequiredMainPanelWidth();

        clipboardWidth = jmax(mediaClipboardWidget.getMinimumWidth(),
                              jmin(newWidth, maximumClipboardWidth));
        resized();
    };
    addAndMakeVisible(dragOverlay);

    showStatusArea = Settings::getBoolValue("view.showStatusArea", true);
    showMediaClipboard = Settings::getBoolValue("view.showMediaClipboard", false);
    mediaClipboardWidget.setPreviewPaneVisible(Settings::getBoolValue("view.showPreviewPane", true));
    mediaClipboardWidget.onPreviewPaneVisibilityChanged = [this]
    {
        Settings::setValue("view.showPreviewPane", mediaClipboardWidget.isPreviewPaneVisible() ? "1" : "0", true);
        commandManager.commandStatusChanged();
    };
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
    modelTabs.removeChangeListener(this);
}

void MainComponent::paint(Graphics& g)
{
    g.fillAll(getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
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

    mainPanel.items.add(FlexItem(modelTabs).withFlex(1.0));

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
        fullWindow.items.add(FlexItem(mediaClipboardWidget).withWidth(getVisibleClipboardWidth()));
    }
    else
    {
        mediaClipboardWidget.setBounds(0, 0, 0, 0);
    }

    fullWindow.performLayout(fullArea);

    dragOverlay.setBounds(getLocalBounds());
}

int MainComponent::getRequiredMainPanelWidth()
{
    // The Home tab has no controls, so only the general minimums apply while it is showing
    auto* tab = modelTabs.getCurrentModelTab();
    const int requiredControlWidth = tab != nullptr ? tab->getMinimumRequiredControlWidth() : 0;

    // Determine minimum width needed to display controls plus padding
    return jmax(minimumMainPanelWidth, requiredControlWidth + minimumMainPanelHorPadding);
}

int MainComponent::getVisibleClipboardWidth()
{
    /* clipboardWidth is the width the user chose. In a narrow window the clipboard
       gives up space, down to its own minimum, so the main panel keeps the width it
       needs; it gets that space back when the window widens again. */
    const int availableWidth = getWidth() - getRequiredMainPanelWidth();

    return jmax(mediaClipboardWidget.getMinimumWidth(), jmin(clipboardWidth, availableWidth));
}

void MainComponent::updateWindowConstraints()
{
    if (auto* window = findParentComponentOfClass<DocumentWindow>())
    {
        // The window can shrink until both panels are down to their minimum widths
        const int requiredMainPanelWidth = getRequiredMainPanelWidth();
        const int requiredClipboardWidth =
            showMediaClipboard ? mediaClipboardWidget.getMinimumWidth() : 0;

        /* Each tab scrolls vertically, so the window does not have to be tall enough
           for every control; it only has to stay usably large. Width is still
           content-driven, since there is no horizontal scrolling. */
        const int requiredMainPanelHeight = minimumWindowHeight - minimumMainPanelVertPadding
                                            + (showStatusArea ? statusAreaHeight : 0);

        // Determine effective minimum width of entire window
        const int newRequiredWindowWidth = jmax(
            minimumWindowWidth, requiredMainPanelWidth + requiredClipboardWidth);
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
       much there is to show, and so how much the current tab has to scroll. The
       window itself may not have changed size, in which case nothing else would
       recompute the scrollable area and the scrollbar would not appear until the
       next resize. */
    modelTabs.layOutCurrentPage();
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
    // in the constructor: status area shown, media clipboard hidden, preview pane shown
    if (! showStatusArea)
        viewStatusAreaCallback();

    if (showMediaClipboard)
        viewMediaClipboardCallback();

    if (! mediaClipboardWidget.isPreviewPaneVisible())
        viewPreviewPaneCallback();

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

void MainComponent::viewPreviewPaneCallback()
{
    // Saving the preference and updating the menu both happen in
    // onPreviewPaneVisibilityChanged, which also covers the pane's own close button
    mediaClipboardWidget.setPreviewPaneVisible(! mediaClipboardWidget.isPreviewPaneVisible());
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
            clipboardWidth = mediaClipboardWidget.getDefaultWidth();
            windowBounds.setWidth(
                jmin(currentDisplayWidth, windowBounds.getWidth() + clipboardWidth));
        }
        else
        {
            if (! window->isFullScreen())
            {
                windowBounds.setWidth(
                    jmax(minimumWindowWidth, windowBounds.getWidth() - getVisibleClipboardWidth()));
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

void MainComponent::openTutorial()
{
    if (tutorialWindow != nullptr)
    {
        tutorialWindow->toFront(true);
        return;
    }

    Component::SafePointer<MainComponent> safeThis(this);
    MessageManager::callAsync(
        [safeThis]()
        {
            if (safeThis == nullptr)
                return;

            TutorialHost& host = *safeThis.getComponent();
            safeThis->tutorialWindow = std::make_unique<TutorialWindow>(host);
            safeThis->tutorialWindow->onClose = [safeThis]()
            {
                if (safeThis != nullptr)
                    safeThis->tutorialWindow.reset();
            };

            safeThis->tutorialWindow->setVisible(true);
            safeThis->tutorialWindow->positionOnHostDisplay();
            safeThis->tutorialWindow->toFront(true);
        });
}

void MainComponent::showPanel(Panel panel)
{
    switch (panel)
    {
        case Panel::statusArea:
            if (! showStatusArea)
                viewStatusAreaCallback();

            break;

        case Panel::mediaClipboard:
            if (! showMediaClipboard)
                viewMediaClipboardCallback();

            break;
    }
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
    if (source == &modelTabs)
    {
        updateWindowConstraints();
    }
}
