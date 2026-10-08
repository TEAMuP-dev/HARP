/**
 * @file MainComponent.h
 * @brief Top-level component containing all HARP GUI elements and state.
 * @author cwitkowitz, saumya-pailwan, xribene, hugofloresgarcia
 */

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ModelTab.h"
#include "ModelTabContainer.h"

#include "clients/Client.h"

#include "widgets/MediaClipboardWidget.h"
#include "widgets/StatusAreaWidget.h"

#include "windows/tutorial/TutorialWindow.h"

#include "windows/AboutWindow.h"
#include "windows/settings/SettingsWindow.h"

#include "utils/Interface.h"
#include "utils/Logging.h"
#include "utils/Settings.h"

using namespace juce;

class MainComponent : public Component,
                      public MenuBarModel,
                      public ApplicationCommandTarget,
                      public DragAndDropContainer,
                      private ChangeListener,
                      private TutorialHost
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
    void openTutorial();

    /* Component */

    void paint(Graphics& g) override;
    void resized() override;

    void updateWindowConstraints();

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
    void changeListenerCallback(ChangeBroadcaster* source) override;

    /* Tutorial */

    Component& getTutorialArea() override { return *this; }
    ModelTabContainer& getModelTabs() override { return modelTabs; }
    void showPanel(Panel panel) override;

    /* Interface */

    const int statusAreaHeight = 100;
    int clipboardWidth = 250;

    // Minimum size to ensure all controls remain visible and functional:
    // - Tutorial window is 500x420, needs padding
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

    // Home tab plus one tab per opened model, each of which scrolls on its own
    ModelTabContainer modelTabs;

    StatusAreaWidget statusAreaWidget;
    DragOverlayComponent dragOverlay;
    MediaClipboardWidget mediaClipboardWidget { &dragOverlay };

    std::unique_ptr<TutorialWindow> tutorialWindow;

    SharedResourcePointer<SharedAPIKeys> sharedTokens;
    SharedResourcePointer<StatusMessage> statusMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
