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
    StatusAreaWidget statusAreaWidget;
    MediaClipboardWidget mediaClipboardWidget;

    bool isTutorialActive = false;
    Rectangle<int> tutorialHighlightRect;
    std::vector<Rectangle<int>> tutorialExtraHighlights;
    std::unique_ptr<WelcomeWindow> welcomeWindow;

    SharedResourcePointer<SharedAPIKeys> sharedTokens;
    SharedResourcePointer<StatusMessage> statusMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
