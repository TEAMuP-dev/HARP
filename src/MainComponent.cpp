#include "MainComponent.h"

JUCE_IMPLEMENT_SINGLETON(HARPLogger)

MainComponent::MainComponent()
{
    HARPLogger::getInstance()->initializeLogger();

    initializeMenuBar();

    modelTabs.addChangeListener(this);

    /* Every model tab needs this, and tabs are opened as models are, so the container
       carries it and hands it to each tab it creates. */
    modelTabs.onOpenAPIKeySettings = [this](Provider provider) { openSettingsWindow(provider); };

    addAndMakeVisible(modelTabs);
    addAndMakeVisible(statusAreaWidget);
    addAndMakeVisible(mediaClipboardWidget);

    statusAreaWidget.setComponentID(TutorialTargets::statusArea);
    mediaClipboardWidget.setComponentID(TutorialTargets::clipboard);

    mediaClipboardWidget.onResize = [this](int newWidth)
    {
        // Leave the main panel at least the width its controls need
        const int maximumClipboardWidth = getWidth() - getRequiredMainPanelWidth();

        clipboardWidth =
            jmax(mediaClipboardWidget.getMinimumWidth(), jmin(newWidth, maximumClipboardWidth));
        resized();
    };
    addAndMakeVisible(dragOverlay);

    showStatusArea = Settings::getBoolValue("view.showStatusArea", true);
    showMediaClipboard = Settings::getBoolValue("view.showMediaClipboard", false);
    mediaClipboardWidget.setPreviewPaneVisible(
        Settings::getBoolValue("view.showPreviewPane", true));
    mediaClipboardWidget.onPreviewPaneVisibilityChanged = [this]
    {
        Settings::setValue(
            "view.showPreviewPane", mediaClipboardWidget.isPreviewPaneVisible(), true);
        commandManager.commandStatusChanged();
    };

    // Sizes the clipboard and its preview pane were last given
    clipboardWidth = jmax(
        mediaClipboardWidget.getMinimumWidth(),
        Settings::getIntValue("view.mediaClipboardWidth", mediaClipboardWidget.getDefaultWidth()));
    mediaClipboardWidget.setPreviewPaneHeight(
        Settings::getIntValue("view.previewPaneHeight", PreviewPaneWidget::defaultHeight));
    mediaClipboardWidget.setPreviewPaneMinimized(
        Settings::getBoolValue("view.previewPaneMinimized", false));
    // Attached after the sizes are restored, which would otherwise save them straight back
    mediaClipboardWidget.onLayoutChanged = [this] { saveClipboardLayout(); };

    dragOverlay.setVisible(false);
    dragOverlay.toFront(false);

    requiredWindowWidth = minimumWindowWidth;
    requiredWindowHeight = minimumWindowHeight;
    setSize(requiredWindowWidth, requiredWindowHeight);

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

#if ! JUCE_MAC
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

int MainComponent::measureRequiredMainPanelWidth()
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
       gives up space, down to its own minimum, so that the main panel keeps the width
       it needs. The clipboard gets that space back when the window widens again. */
    const int availableWidth = getWidth() - getRequiredMainPanelWidth();

    return jmax(mediaClipboardWidget.getMinimumWidth(), jmin(clipboardWidth, availableWidth));
}

void MainComponent::updateWindowConstraints()
{
    /* Measuring the control labels is slow, so it happens here, whenever the controls
       can have changed, rather than on every resize of the window or the clipboard. */
    requiredMainPanelWidth = measureRequiredMainPanelWidth();

    if (auto* window = findParentComponentOfClass<DocumentWindow>())
    {
        // The window can shrink until both panels are down to their minimum widths
        const int requiredClipboardWidth =
            showMediaClipboard ? mediaClipboardWidget.getMinimumWidth() : 0;

        /* Each tab scrolls vertically, so the window does not have to be tall enough
           for every control. It only has to stay usably large. Width is still
           content-driven, since there is no horizontal scrolling. */
        const int requiredMainPanelHeight = minimumWindowHeight - minimumMainPanelVertPadding
                                            + (showStatusArea ? statusAreaHeight : 0);

        // Determine effective minimum width of entire window
        const int newRequiredWindowWidth =
            jmax(minimumWindowWidth, requiredMainPanelWidth + requiredClipboardWidth);
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

    /* Whatever prompted this (e.g., a model loading or a panel being toggled) changed
       how much there is to show, and so how much the current tab has to scroll. The
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
    DBG_AND_LOG("MainComponent::importNewFile: Importing \""
                << mediaFile.getFullPathName() << "\""
                << (fromDAW ? ", which stays linked to the DAW that opened it." : "."));

    mediaClipboardWidget.addTrackFromFilePath(URL(mediaFile), fromDAW);

    if (! showMediaClipboard)
    {
        viewMediaClipboardCallback();
    }
}

void MainComponent::openSettingsWindow(std::optional<Provider> focusProvider)
{
    DialogWindow::LaunchOptions options;
    options.dialogTitle = "Settings";
    options.dialogBackgroundColour =
        getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground);
    // The settings dialog is a free-standing desktop window that can outlive this
    // component, so guard the callback with a SafePointer
    Component::SafePointer<MainComponent> safeThis(this);

    auto* settings = new SettingsWindow(
        [safeThis]
        {
            if (safeThis != nullptr)
                safeThis->restoreViewDefaults();
        });

    // Selected before the window is shown, so it opens already on the right tab
    if (focusProvider.has_value())
    {
        settings->showAPIKeysFor(*focusProvider);
    }

    options.content.setOwned(settings);

    options.useNativeTitleBar = true;
    options.resizable = true;
    options.escapeKeyTriggersCloseButton = true;

    options.launchAsync();
}

void MainComponent::restoreViewDefaults()
{
    // Defaults must match the fallbacks used when reading the settings
    // in the constructor: status area shown, media clipboard hidden, preview pane shown,
    // and the clipboard and preview pane at their default sizes
    if (! showStatusArea)
        viewStatusAreaCallback();

    if (showMediaClipboard)
        viewMediaClipboardCallback();

    if (! mediaClipboardWidget.isPreviewPaneVisible())
        viewPreviewPaneCallback();

    // Sizes as well, after hiding the clipboard, which shrinks the window by its current width
    clipboardWidth = mediaClipboardWidget.getDefaultWidth();
    mediaClipboardWidget.setPreviewPaneHeight(PreviewPaneWidget::defaultHeight);
    mediaClipboardWidget.setPreviewPaneMinimized(false);

    /* The window takes the geometry of a first launch, which HARPWindow gives it when no
       position is saved: as small as the default view allows, and centred. Its position
       is saved again when it closes, so leaving it where it was would undo the reset. */
    if (auto* window = findParentComponentOfClass<DocumentWindow>())
    {
        updateWindowConstraints();

        if (window->isFullScreen())
            window->setFullScreen(false);

        window->centreWithSize(requiredWindowWidth, requiredWindowHeight);
    }

    // showWelcomePopup default (true) is already restored by clearing settings;
    // it will show on the next launch automatically.
}

/* --View-- */

void MainComponent::saveClipboardLayout()
{
    Settings::setValue("view.mediaClipboardWidth", clipboardWidth);
    Settings::setValue("view.previewPaneHeight", mediaClipboardWidget.getPreviewPaneHeight());
    Settings::setValue(
        "view.previewPaneMinimized", mediaClipboardWidget.isPreviewPaneMinimized(), true);
}

void MainComponent::viewStatusAreaCallback()
{
    // Toggle status area visibility state
    showStatusArea = ! showStatusArea;

    resizeWindowForPanel(0, showStatusArea ? statusAreaHeight : -statusAreaHeight);

    // Add view preference to persistent settings
    Settings::setValue("view.showStatusArea", showStatusArea, true);

    // Send status message to add check to file menu
    commandManager.commandStatusChanged();
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

    // A hidden preview pane has no controls left to stop it with
    if (! showMediaClipboard)
    {
        mediaClipboardWidget.stopPlayback();
    }

    // Opened at the width it was last given, and closed by the width at which it is showing
    resizeWindowForPanel(showMediaClipboard ? clipboardWidth : -getVisibleClipboardWidth(), 0);

    // Add view preference to persistent settings
    Settings::setValue("view.showMediaClipboard", showMediaClipboard, true);

    // Send status message to add check to file menu
    commandManager.commandStatusChanged();
}

void MainComponent::resizeWindowForPanel(int deltaWidth, int deltaHeight)
{
    auto* window = findParentComponentOfClass<DocumentWindow>();

    const bool isShrinking = deltaWidth < 0 || deltaHeight < 0;

    /* The smallest size the window may take depends on which panels are showing, and a platform
       can hold every resize to it, this one included, as Windows does. A panel being hidden
       therefore lowers the limit before the window shrinks, and one being shown raises it only
       after the window has grown. In the other order the window keeps its size when a panel is
       hidden at the limit, and then grows by another panel each time it is shown again. */
    if (isShrinking)
    {
        updateWindowConstraints();
    }

    // A window that fills the display keeps doing so, and lays the panel out within it
    if (window != nullptr && ! window->isFullScreen())
    {
        Rectangle<int> windowBounds = window->getBounds();

        // Determine which display contains HARP, beyond which the window does not grow
        auto* currentDisplay =
            Desktop::getInstance().getDisplays().getDisplayForRect(window->getScreenBounds());

        const Rectangle<int> displayArea =
            currentDisplay != nullptr ? currentDisplay->userArea : windowBounds;

        windowBounds.setWidth(jmin(jmax(displayArea.getWidth(), windowBounds.getWidth()),
                                   windowBounds.getWidth() + deltaWidth));
        windowBounds.setHeight(jmin(jmax(displayArea.getHeight(), windowBounds.getHeight()),
                                    windowBounds.getHeight() + deltaHeight));

        window->setBoundsConstrained(windowBounds);
    }

    if (! isShrinking)
    {
        updateWindowConstraints();
    }

    // A window that kept its size, as one filling the display does, still has the panel to lay out
    resized();
}

/* --Help-- */

void MainComponent::openAboutWindow()
{
    auto aboutComponent = std::make_unique<AboutWindow>();

    DialogWindow::LaunchOptions options;
    options.dialogTitle = "About " + String(APP_NAME);
    options.dialogBackgroundColour =
        getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground);
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

void MainComponent::changeListenerCallback(ChangeBroadcaster* source)
{
    if (source == &modelTabs)
    {
        updateWindowConstraints();
    }
}
