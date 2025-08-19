#include "MainComponent.h"
#include "AppSettings.h"

using namespace juce;

class GuiAppApplication : public JUCEApplication
{
public:
    GuiAppApplication()
    {
        PropertiesFile::Options options;

        options.applicationName = getApplicationName();
        options.osxLibrarySubFolder = "Application Support/";
        options.folderName = getApplicationName();
        options.filenameSuffix = ".settings";
        /*
          Settings should be located at:
            - macOS: ~/Library/Application Support/HARP/HARP.settings
            - Windows: %APPDATA%\HARP\HARP.settings (no need to manually set)
            - Linux: ~/.config/HARP/HARP.settings (no need to manually set)
        */
        options.commonToAllUsers = false;
        applicationProperties.setStorageParameters(options);

        // Initialize AppSettings singleton with our application properties
        AppSettings::initialize(&applicationProperties);
    }

    /*
      Multiple invocations are automatically handled on macOS / Windows.
    */
    bool moreThanOneInstanceAllowed() override { return false; }

    /*
      We inject the following as compile definitions from CMakeLists.txt. If you've
      enabled the juce header with `juce_generate_juce_header(<thisTarget>)` you
      could `#include <JuceHeader.h>` and use `ProjectInfo::projectName`, etc., instead.
    */
    const String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }

    /// Called when app is invoked
    void initialise(const String& commandLine) override
    {
        writeDebugLog("GuiAppApplication::initialise: Invoked with command line \"" + commandLine
                      + "\".");
        writeDebugLog("GuiAppApplication::getCommandLineParameters(): \""
                      + getCommandLineParameters() + "\".");

        appJustLaunched = true;
        originalCommandLine = commandLine;

        String windowTitle = getApplicationName();

        if (windowCounter > 1)
        {
            windowTitle += " (" + String(windowCounter) + ")";
        }

        windowCounter++;

        mainWindow.reset(new HARPWindow(windowTitle));

        StringArray args;

        // Split command line arguments at spaces
        args.addTokens(commandLine.trim(), " ", "\"");

        importInitialFiles(args);

        // Assumes app can't be re-invoked manually within 500ms of initial launch
        Timer::callAfterDelay(500, [this]() { appJustLaunched = false; });
    }

    void importInitialFiles(StringArray files)
    {
        for (auto f : files)
        {
            File inputMediaFile(f.unquoted());

            // Check if argument is valid file
            if (inputMediaFile.existsAsFile())
            {
                if (auto* mainComp =
                        dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
                {
                    // Import file into initialized window
                    mainComp->importNewFile(inputMediaFile, true);
                }
            }
        }
    }

    /// Called when app is invoked and other instances are running
    void anotherInstanceStarted(const String& commandLine) override
    {
        StringArray args;

        // Split command line arguments at spaces
        args.addTokens(commandLine.trim(), " ", "\"");

        /*
          With moreThanOneInstanceAllowed() returning false, HARP is actually launched more
          than once depending on the number of arguments given and the source of the launch.

          For example, when REAPER launches HARP with file path argument(s), initialise() is
          called once with no arguments and anotherInstanceStarted() is subsequently called
          with the arguments. However, when HARP is launched from the command line with
          arguments, initialise() is called with the argument(s), and anotherInstanceStarted()
          is subsequently called with the same argument(s).

          The following conditional is an ugly solution to an ugly problem...

          See also https://github.com/juce-framework/JUCE/issues/607
        */
        if (appJustLaunched)
        {
            if (! commandLine.isEmpty() && originalCommandLine.isEmpty())
            {
                writeDebugLog(
                    "GuiAppApplication::anotherInstanceStarted: Handling subsequent invocation with command line \""
                    + commandLine + "\".");

                importInitialFiles(args);

                return;
            }

            writeDebugLog(
                "GuiAppApplication::anotherInstanceStarted: Ignoring spurious invocation with command line \""
                + commandLine + "\".");

            return;
        }

        writeDebugLog(
            "GuiAppApplication::anotherInstanceStarted: Another instance started with command line \""
            + commandLine + "\".");

        for (auto arg : args)
        {
            File inputMediaFile(arg.unquoted());

            // Check if argument is valid file
            if (inputMediaFile.existsAsFile())
            {
                // Choose/handle protocol for additional instances
                tryOpenChoiceWindow(inputMediaFile);
            }
        }
    }

    /// Application shutdown code
    void shutdown() override
    {
        // Delete our window
        mainWindow = nullptr;
    }

    /**
     * Called when the app is being asked to quit. This request can be ignored and
     * the app will continue to run, or quit() can be called to close the app.
     */
    void systemRequestedQuit() override { quit(); }

    /**
     * Implements desktop window that contains instance of MainComponent.
     */
    class HARPWindow : public DocumentWindow
    {
    public:
        explicit HARPWindow(String name)
            : DocumentWindow(name,
                             Desktop::getInstance().getDefaultLookAndFeel().findColour(
                                 ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons),
              windowIdentifier(name.replaceCharacters(" :", "__"))
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(true, true);

            // Try to restore saved position and size
            restoreWindowPosition();

            setVisible(true);
        }

        /**
         * Defines the behavior when the user closes the window. Although multiple
         * windows are supported, they will all belong to the same application instance
         * and closing the main window will lead to all other windows closing.
         * 
         * If the main window is closed, we promote the first additional window
         * (if any exist) to become the new main window.
         */
        void closeButtonPressed() override
        {
            // Save window position and size before closing
            saveWindowPosition();

            auto* app = dynamic_cast<GuiAppApplication*>(JUCEApplication::getInstance());

            if (this == app->getMainWindowPtr())
            {
                // Check if other windows open
                if (app->hasAdditionalWindows())
                {
                    app->promoteAdditionalWindowToMain();

                    // Main window will be deleted automatically when it goes out of scope
                }
                else
                {
                    // No other windows - terminate application
                    JUCEApplication::getInstance()->systemRequestedQuit();
                }
            }
            else
            {
                app->removeAdditionalWindow(this);
            }
        }

        /*void activeWindowStatusChanged() override
        {
            if (isActiveWindow())
            {
                if (auto* mainComp = dynamic_cast<MainComponent*>(getContentComponent()))
                {
                    // Check if loaded file(s) are still up to date when window refocused
                    mainComp->focusCallback();
                }
            }

            DocumentWindow::activeWindowStatusChanged();
        }*/

    private:
        String getPrefix() { return "window." + windowIdentifier + "."; }

        void saveWindowPosition()
        {
            // Don't save minimized position
            if (! isMinimised())
            {
                auto bounds = getBounds();

                String prefix = getPrefix();

                // Create property names using window identifier to avoid conflicts
                AppSettings::setValue(prefix + "x", bounds.getX());
                AppSettings::setValue(prefix + "y", bounds.getY());
                AppSettings::setValue(prefix + "width", bounds.getWidth());
                AppSettings::setValue(prefix + "height", bounds.getHeight());

                AppSettings::saveIfNeeded();
            }
        }

        void restoreWindowPosition()
        {
            String prefix = getPrefix();

            // Check if we have saved position data
            if (AppSettings::containsKey(prefix + "x") && AppSettings::containsKey(prefix + "y")
                && AppSettings::containsKey(prefix + "width")
                && AppSettings::containsKey(prefix + "height"))
            {
                // Get stored position and size
                int x = AppSettings::getIntValue(prefix + "x");
                int y = AppSettings::getIntValue(prefix + "y");
                int width = AppSettings::getIntValue(prefix + "width");
                int height = AppSettings::getIntValue(prefix + "height");

                // Validate size
                width = jmax(100, width);
                height = jmax(100, height);

                // Create rectangle with the stored bounds
                Rectangle<int> bounds(x, y, width, height);

                auto displays = Desktop::getInstance().getDisplays();

                // Check if position exists on an available displays
                if (displays.getDisplayForRect(bounds) != nullptr)
                {
                    // Position is valid
                    setBounds(bounds);
                }
                else
                {
                    // No display found for saved position - center on main display
                    centreWithSize(width, height);
                }
            }
            else
            {
                // No saved position - center on screen
                centreWithSize(getWidth(), getHeight());
            }
        }

        String windowIdentifier;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HARPWindow)
    };

    HARPWindow* getMainWindowPtr() const { return mainWindow.get(); }

    bool hasAdditionalWindows() const { return ! additionalWindows.isEmpty(); }

    void promoteAdditionalWindowToMain()
    {
        if (! additionalWindows.isEmpty())
        {
            // Promote first additional window in line as new main
            mainWindow = std::move(additionalWindows.getReference(0));
            // Remove from additional windows
            additionalWindows.remove(0);

            windowCounter--;
        }
    }

    void removeAdditionalWindow(HARPWindow* windowToRemove)
    {
        for (int i = 0; i < additionalWindows.size(); ++i)
        {
            if (additionalWindows.getReference(i).get() == windowToRemove)
            {
                // Remove referenced window
                additionalWindows.remove(i);

                windowCounter--;

                break;
            }
        }
    }

private:
    /*
      There's no way to hit breakpoints in Main.cpp when invoking from the DAW,
      so we use print statements and write to a dedicated log file located at:
        - macOS: ~/Library/Logs/HARP/launch.log
        - Windows: C:\Users\<username>\AppData\Roaming\HARP\launch.log
        - Linux: ~/.config/HARP/launch.log
    */
    void writeDebugLog(const String& message)
    {
        // Write message to standard error stream
        DBG(message);

        // Get application platform-dependent logs directory
        File logsDir = FileLogger::getSystemLogFileFolder().getChildFile(getApplicationName());
        // Ensure log directory exists
        logsDir.createDirectory();
        // Create log file if it doesn't exist
        File debugFile = logsDir.getChildFile("launch.log");
        // Write message to file
        debugFile.appendText(message + "\n", true, true);
    }

    void tryOpenChoiceWindow(File inputMediaFile)
    {
        if (blockNewModalWindows)
        {
            // Wait until current modal window is closed to continue
            Timer::callAfterDelay(
                100, [this, inputMediaFile]() { tryOpenChoiceWindow(inputMediaFile); });
            return;
        }

        openChoiceWindow(inputMediaFile);
    }

    void openChoiceWindow(File inputMediaFile)
    {
        // We deal with UI stuff so it's safer to work on the message thread
        MessageManager::callAsync(
            [this, inputMediaFile]()
            {
                // Check if existing user choice in settings
                bool hasPreference = AppSettings::containsKey("newInstancePreference");
                int preferenceValue = AppSettings::getIntValue("newInstancePreference", -1);

                if (hasPreference && preferenceValue >= 0 && preferenceValue <= 1)
                {
                    handleFileOpenChoice(preferenceValue, inputMediaFile);
                }
                else
                {
                    blockNewModalWindows = true;

                    // Show dialog with "Remember my choice" option
                    auto options = MessageBoxOptions()
                                       .withTitle("Open File")
                                       .withMessage("How would you like to open \""
                                                    + inputMediaFile.getFileName() + "\"?")
                                       .withIconType(MessageBoxIconType::QuestionIcon)
                                       .withButton("New Window")
                                       .withButton("Current Window")
                                       .withButton("Cancel");

                    /*
                      Create custom AlertWindow to add checkbox. AlertWindow::showAsync()
                      is avoided because it's unclear how to add a checkbox to it and
                      MessageBoxOptions doesn't have withCustomComponent() method.
                    */
                    std::unique_ptr<AlertWindow> alertWindow = std::make_unique<AlertWindow>(
                        options.getTitle(), options.getMessage(), options.getIconType());

                    alertWindow->addButton(options.getButtonText(0), 1); // New window
                    alertWindow->addButton(options.getButtonText(1), 2); // Current window
                    alertWindow->addButton(options.getButtonText(2), 0); // Cancel

                    // Checkbox is a raw pointer because it couldn't be passed to lambda as a unique_ptr
                    auto rememberCheckbox = std::make_unique<ToggleButton>("Remember my choice");
                    rememberCheckbox->setSize(200, 24);
                    rememberCheckbox->setName("");

                    // AlertWindow doesn't take ownership of custom components so checkbox needs to be deleted manually
                    alertWindow->addCustomComponent(rememberCheckbox.get());

                    // Open window asynchronously
                    alertWindow->enterModalState(
                        true,
                        ModalCallbackFunction::create(
                            [this,
                             alertWindow = alertWindow.release(),
                             inputMediaFile,
                             rememberCheckboxPtr = std::move(rememberCheckbox)](int result)
                            {
                                blockNewModalWindows = false;

                                if (result != 0) // Cancel
                                {
                                    int choice = result - 1;

                                    bool rememberChoice = rememberCheckboxPtr->getToggleState();

                                    // Save preference if requested but d on't save Cancel choice
                                    if (rememberChoice && choice <= 1)
                                    {
                                        AppSettings::setValue("newInstancePreference", choice);
                                        AppSettings::saveIfNeeded();
                                    }

                                    handleFileOpenChoice(choice, inputMediaFile);
                                }
                            }),
                        true);
                }
            });
    }

    void handleFileOpenChoice(int choice, const File& inputMediaFile)
    {
        switch (choice)
        {
            case 0: // New Window
            {
                String windowTitle = getApplicationName();

                windowCounter++;

                if (windowCounter > 1)
                {
                    windowTitle += " (" + String(windowCounter - 1) + ")";
                }

                std::unique_ptr<HARPWindow> newWindow = std::make_unique<HARPWindow>(windowTitle);

                if (auto* mainComp = dynamic_cast<MainComponent*>(newWindow->getContentComponent()))
                {
                    // Import file into new window
                    mainComp->importNewFile(inputMediaFile, true);
                }

                // Store window and make it visible
                additionalWindows.add(std::move(newWindow));

                break;
            }
            case 1: // Current window
            {
                if (auto* mainComp =
                        dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
                {
                    // Import file into current window
                    mainComp->importNewFile(inputMediaFile, true);
                }
                break;
            }
        }
    }

    std::unique_ptr<HARPWindow> mainWindow;
    Array<std::unique_ptr<HARPWindow>> additionalWindows;

    ApplicationProperties applicationProperties;

    int windowCounter = 0;

    String originalCommandLine;

    std::atomic<bool> appJustLaunched { false };
    std::atomic<bool> blockNewModalWindows { false };
};

// Generates main() routine that launches app
START_JUCE_APPLICATION(GuiAppApplication)
