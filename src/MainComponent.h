/**
 * @file MainComponent.h
 * @brief Top-level component containing all HARP GUI elements and state.
 * @author hugofloresgarcia, xribene, cwitkowitz
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ModelTab.h"

#include "clients/Client.h"

#include "widgets/MediaClipboardWidget.h"
#include "widgets/StatusAreaWidget.h"

#include "windows/AboutWindow.h"
#include "windows/settings/SettingsWindow.h"

#include "utils/Interface.h"
#include "utils/Logging.h"
#include "utils/Settings.h"
#include "utils/Tutorial.h"

using namespace juce;

// Forward declaration (include in .cpp)
class WelcomeWindow;

class MainComponent : public Component,
                      public MenuBarModel,
                      public ApplicationCommandTarget,
                      private ChangeListener

{
public:
    MainComponent();
    ~MainComponent() override;

    /* File Menu */

    StringArray getMenuBarNames() override;

    std::unique_ptr<PopupMenu> getMacExtraMenu();

    PopupMenu getMenuForIndex([[maybe_unused]] int menuIndex, const String& menuName) override;

    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    /* Application */

    ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }

    void getAllCommands(Array<CommandID>& commands) override;

    void getCommandInfo(CommandID commandID, ApplicationCommandInfo& result) override;

    bool perform(const InvocationInfo& info) override;

    /* Callbacks */

    // File
    void importNewFile(File mediaFile, bool fromDAW = false);
    void openSettingsWindow();

    // View
    void viewStatusAreaCallback();
    void viewMediaClipboardCallback();
    void restoreViewDefaults();

    // Help
    void openAboutWindow();
    void openWelcomeWindow(bool ensureDefaultModelLoaded = false);

    /* Tutorial */

    ModelTab* getModelTab() { return &mainModelTab; }

    void setTutorialActive(bool active);
    void setTutorialHighlight(Rectangle<int> bounds);
    void setTutorialExtraHighlights(std::vector<Rectangle<int>> bounds);
    void ensureTutorialModelLoaded();
    void resetTutorialAutoLoadedModel();

    // Bounds accessors for tutorial steps (public for WelcomeWindow)
    Rectangle<int> getModelSelectBounds();
    Rectangle<int> getControlsBounds();
    Rectangle<int> getInputTrackBounds();
    Rectangle<int> getInputFolderBounds();
    Rectangle<int> getInputPlayBounds();
    Rectangle<int> getProcessButtonBounds();
    Rectangle<int> getTracksBounds();
    Rectangle<int> getClipboardBounds();
    Rectangle<int> getClipboardTrackAreaBounds();
    Rectangle<int> getClipboardControlsBounds();
    Rectangle<int> getClipboardNameBoxBounds();
    Rectangle<int> getClipboardButtonsBounds();
    Rectangle<int> getClipboardAddButtonBounds();
    Rectangle<int> getClipboardRemoveButtonBounds();
    Rectangle<int> getClipboardPlayButtonBounds();
    Rectangle<int> getClipboardSendToDAWButtonBounds();
    Rectangle<int> getInfoBarBounds();

    /* Component */

    void paint(Graphics& g) override;
    void paintOverChildren(Graphics& g) override;
    void resized() override;

    void updateWindowConstraints();
    void refreshTutorialHighlight();
    Rectangle<int> getVisibleTabArea(Rectangle<int> tabBounds);

private:
    /* File Menu */

    void initializeMenuBar();
    void deinitializeMenuBar();

    std::unique_ptr<MenuBarComponent> menuBar;
    std::unique_ptr<PopupMenu> macExtraMenu;

    /* Application */

    ApplicationCommandManager commandManager;

    /* Callbacks */

    // Miscellaneous
    //void focusCallback();
    void changeListenerCallback(ChangeBroadcaster* source);

    /* Interface */

    const int statusAreaHeight = 100;
    const float mediaClipboardFlex = 0.4f;
    const float mediaClipboardScale = 1.4f;

    // Minimum size to ensure all controls remain visible and functional:
    // - WelcomeWindow popup is 480x500, needs padding
    // - Dropdown labels need adequate width
    // - Control Area needs space for sliders/toggles/textboxes
    const int minimumWindowWidth = 700;
    const int minimumWindowHeight = 500;
    const int minimumMainPanelWidth = 320;
    const int minimumMainPanelHorPadding = 32;
    const int minimumMainPanelVertPadding = 32;

    int requiredWindowWidth;
    int requiredWindowHeight;

    bool showStatusArea;
    bool showMediaClipboard;

    ModelTab mainModelTab;

    /**
     * Viewport that leaves the mouse wheel to the tracks.
     *
     * Tracks use the wheel to zoom their contents, so the panel must not treat a
     * wheel event over one as a request to scroll. Viewport::useMouseWheelMoveIfNeeded
     * is not virtual, so the decision is made here instead.
     */
    struct MainPanelViewport : public Viewport
    {
        void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override
        {
            /* Test originalComponent, not eventComponent: a wheel event that goes
               unhandled is passed up the hierarchy with getEventRelativeTo, which
               rewrites eventComponent to each parent in turn, so by the time it
               arrives here eventComponent is this viewport. originalComponent still
               names the component the wheel was actually over. */
            if (isWithinTrack(e.originalComponent))
            {
                return;
            }

            Viewport::mouseWheelMove(e, wheel);
        }

        /* Scrolling moves the tab under the tutorial overlay, which draws its
           highlight in window coordinates and would otherwise keep pointing at
           where a component used to be. */
        void visibleAreaChanged(const Rectangle<int>&) override
        {
            if (onScrolled != nullptr)
            {
                onScrolled();
            }
        }

        std::function<void()> onScrolled;

        /* True when the wheel landed on a track that will act on it. A track with
           no media loaded does not, so the panel should still scroll over it. */
        static bool isWithinTrack(Component* c)
        {
            for (auto* candidate = c; candidate != nullptr;
                 candidate = candidate->getParentComponent())
            {
                if (auto* track = dynamic_cast<MediaDisplayComponent*>(candidate))
                {
                    return track->usesMouseWheel();
                }
            }

            return false;
        }
    };

    /* The tab is scrolled rather than squeezed when the window cannot be made
       large enough to show it, which happens once the required size exceeds the
       display. Declared after the tab so that it is torn down first. */
    MainPanelViewport mainPanelViewport;

    StatusAreaWidget statusAreaWidget;
    DragOverlayComponent dragOverlay;
    MediaClipboardWidget mediaClipboardWidget { &dragOverlay };

    bool isTutorialActive = false;
    Rectangle<int> tutorialHighlightRect;
    std::vector<Rectangle<int>> tutorialExtraHighlights;
    std::unique_ptr<WelcomeWindow> welcomeWindow;

    SharedResourcePointer<SharedAPIKeys> sharedTokens;
    SharedResourcePointer<StatusMessage> statusMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
