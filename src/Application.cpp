#include "MainComponent.h"

enum CommandIDs
{
    // invalid = 0x0000,

    // File
    open = 0x0001,
    save = 0x0002,
    saveAs = 0x0003,
    settings = 0x0004,

    // Edit
    undo = 0x1000,
    redo = 0x1001,

    // View
    viewStatusArea = 0x2000,
    viewMediaClipboard = 0x2001,

    // Help
    welcome = 0x3000,
    about = 0x3001
};

StringArray MainComponent::getMenuBarNames()
{
    StringArray menuBarNames;

    menuBarNames.add("File");
    //menuBarNames.add("Edit");
    menuBarNames.add("View");
    menuBarNames.add("Help");

    return menuBarNames;
}

/**
 * Place certain commands in the application menu ("HARP" tab) for MacOS.
 */
std::unique_ptr<PopupMenu> MainComponent::getMacExtraMenu()
{
    auto menu = std::make_unique<PopupMenu>();

    menu->addCommandItem(&commandManager, CommandIDs::about);
    menu->addCommandItem(&commandManager, CommandIDs::settings);

    return menu;
}

PopupMenu MainComponent::getMenuForIndex([[maybe_unused]] int menuIndex, const String& menuName)
{
    PopupMenu menu;

    if (menuName == "File")
    {
        menu.addCommandItem(&commandManager, CommandIDs::open);
        //menu.addCommandItem(&commandManager, CommandIDs::save);
        menu.addCommandItem(&commandManager, CommandIDs::saveAs);

        menu.addSeparator();

        menu.addCommandItem(&commandManager, CommandIDs::settings);
    }
    else if (menuName == "Edit")
    {
        //menu.addCommandItem(&commandManager, CommandIDs::undo);
        //menu.addCommandItem(&commandManager, CommandIDs::redo);
    }
    else if (menuName == "View")
    {
        menu.addCommandItem(&commandManager, CommandIDs::viewStatusArea);
        menu.addCommandItem(&commandManager, CommandIDs::viewMediaClipboard);
    }
    else if (menuName == "Help")
    {
        menu.addCommandItem(&commandManager, CommandIDs::about);
        menu.addCommandItem(&commandManager, CommandIDs::welcome);
    }
    else
    {
        DBG_AND_LOG("MainComponent::getMenuForIndex: Unknown menu name: " << menuName << ".");
    }

    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int topLevelMenuIndex)
{
    DBG_AND_LOG("MainComponent::menuItemSelected: Selected ID " << menuItemID << " and index "
                                                                << topLevelMenuIndex << ".");
}

void MainComponent::initializeMenuBar()
{
    menuBar.reset(new MenuBarComponent(this));
    addAndMakeVisible(menuBar.get());

    setApplicationCommandManagerToWatch(&commandManager);

    commandManager.registerAllCommandsForTarget(this);
    commandManager.setFirstCommandTarget(this);

    addKeyListener(commandManager.getKeyMappings());

#if JUCE_MAC
    // TODO - is this actually used?
    macExtraMenu = getMacExtraMenu();

    MenuBarModel::setMacMainMenu(this, macExtraMenu.get());
#endif

    menuBar->setVisible(true);
    menuItemsChanged();
}

void MainComponent::deinitializeMenuBar()
{
#if JUCE_MAC
    MenuBarModel::setMacMainMenu(nullptr);
#endif
}

/**
 * Fills commands array with commands this component/target supports.
 */
void MainComponent::getAllCommands(Array<CommandID>& commands)
{
    const CommandID fileIDs[] = {
        CommandIDs::open, CommandIDs::save, CommandIDs::saveAs, CommandIDs::settings
    };

    commands.addArray(fileIDs, numElementsInArray(fileIDs));

    const CommandID editIDs[] = { CommandIDs::undo, CommandIDs::redo };

    commands.addArray(editIDs, numElementsInArray(editIDs));

    const CommandID viewIDs[] = { CommandIDs::viewStatusArea, CommandIDs::viewMediaClipboard };

    commands.addArray(viewIDs, numElementsInArray(viewIDs));

    const CommandID helpIDs[] = { CommandIDs::about, CommandIDs::welcome };

    commands.addArray(helpIDs, numElementsInArray(helpIDs));
}

void MainComponent::getCommandInfo(CommandID commandID, ApplicationCommandInfo& result)
{
    switch (commandID)
    {
        /*
          Note that the third argument of setInfo() serves only as a tag for
          categorization and does not need to match the corresponding file menu option.
        */

        /* --File-- */
        case CommandIDs::open:
            result.setInfo("Open", "Add a track to the media clipboard", "File", 0);
            result.addDefaultKeypress('o', ModifierKeys::commandModifier);

            break;

        case CommandIDs::save:
            result.setInfo("Save", "Saves currently selected media clipboard track", "File", 0);
            result.addDefaultKeypress('s', ModifierKeys::commandModifier);

            break;

        case CommandIDs::saveAs:
            result.setInfo("Save As", "Saves currently selected track with a new name", "File", 0);
            // TODO = is logical (|) correct here?
            result.addDefaultKeypress('s',
                                      ModifierKeys::shiftModifier | ModifierKeys::commandModifier);

            break;

        case CommandIDs::settings:
            result.setInfo("Settings", "Open settings window", "File", 0);

            break;

        /* --Edit-- */
        case CommandIDs::undo:
            result.setInfo("Undo", "Reverses most recent action", "Edit", 0);
            result.addDefaultKeypress('z', ModifierKeys::commandModifier);

            break;

        case CommandIDs::redo:
            result.setInfo("Redo", "Repeats next future action", "Edit", 0);
            // TODO = is logical (|) correct here?
            result.addDefaultKeypress('z',
                                      ModifierKeys::shiftModifier | ModifierKeys::commandModifier);

            break;

        /* --View-- */
        case CommandIDs::viewStatusArea:
            result.setInfo("Status Area", "Toggles display of status area", "View", 0);
            result.setTicked(showStatusArea);

            break;

        case CommandIDs::viewMediaClipboard:
            result.setInfo("Media Clipboard", "Toggles display of media clipboard", "View", 0);
            result.setTicked(showMediaClipboard);

            break;

        /* --Help-- */
        case CommandIDs::welcome:
            result.setInfo(
                "Welcome Page", "Provides helpful information for first-time users", "Help", 0);

            break;

        case CommandIDs::about:
            result.setInfo(
                "About HARP", "Provides helpful information and links for application", "Help", 0);

            break;
    }
}

bool MainComponent::perform(const InvocationInfo& info)
{
    switch (info.commandID)
    {
        /* --File-- */
        case CommandIDs::open:
            DBG_AND_LOG("MainComponent::perform: \"open\" command invoked.");
            mediaClipboardWidget.addFileCallback();

            break;

        case CommandIDs::save:
            DBG_AND_LOG("MainComponent::perform: \"save\" command invoked.");
            // TODO - saveCallback();

            break;

        case CommandIDs::saveAs:
            DBG_AND_LOG("MainComponent::perform: \"saveAs\" command invoked.");
            mediaClipboardWidget.saveFileCallback();

            break;

        case CommandIDs::settings:
            DBG_AND_LOG("MainComponent::perform: \"settings\" command invoked.");
            openSettingsWindow();

            break;

        /* --Edit-- */
        case CommandIDs::undo:
            DBG_AND_LOG("MainComponent::perform: \"undo\" command invoked.");
            // TODO - undoCallback();

            break;

        case CommandIDs::redo:
            DBG_AND_LOG("MainComponent::perform: \"redo\" command invoked.");
            // TODO - redoCallback();

            break;

        /* --View-- */
        case CommandIDs::viewMediaClipboard:
            DBG_AND_LOG("MainComponent::perform: \"viewMediaClipboard\" command invoked.");
            viewMediaClipboardCallback();

            break;

        case CommandIDs::viewStatusArea:
            DBG_AND_LOG("MainComponent::perform: \"viewStatusArea\" command invoked.");
            viewStatusAreaCallback();

            break;

        /* --Help-- */
        case CommandIDs::welcome:
            DBG_AND_LOG("MainComponent::perform: \"welcome\" command invoked.");
            // TODO - openWelcomeWindow();

            break;

        case CommandIDs::about:
            DBG_AND_LOG("MainComponent::perform: \"about\" command invoked.");
            openAboutWindow();

            break;

        default:
            return false;
    }

    return true;
}
