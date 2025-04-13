#include "MainComponent.h"
#include "AppSettings.h"

//==============================================================================
class GuiAppApplication : public juce::JUCEApplication
{
public:
    //==============================================================================
    GuiAppApplication()
    {
        PropertiesFile::Options options;
        options.applicationName = getApplicationName();
        options.filenameSuffix = ".settings";
        // For macos the settings path is ~/Library/Application Support/HARP/HARP.settings
        // For windows is %APPDATA%\HARP\HARP.settings (no need to manually set this)
        // For linux is ~/.config/HARP/HARP.settings (no need to manually set this)
        options.osxLibrarySubFolder = "Application Support";
        applicationProperties.setStorageParameters(options);
        
        // Initialize the AppSettings singleton with our application properties
        AppSettings::initialize(&applicationProperties);
    }

    // We inject these as compile definitions from the CMakeLists.txt
    // If you've enabled the juce header with `juce_generate_juce_header(<thisTarget>)`
    // you could `#include <JuceHeader.h>` and use `ProjectInfo::projectName` etc. instead.
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    // In MacOS it's always false, but in Windows and Linux it can be true
    // we keep it false for every OS to avoid confusion
    bool moreThanOneInstanceAllowed() override { return false; }

    // Set this to true only for debugging the current Main.cpp file
    // it doesn't affect debugging in HARP itself
    bool debugFilesOn() { return true; }

    // Local debugging just for this file because there is no way to debug the app
    // at this stage using the debugger when invoking it from the DAW
    void writeDebugLog(const juce::String& message)
    {
        if (!debugFilesOn())
            return;
        DBG(message);
        File debugFile(
            juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName()
            + "/debug.txt");
        debugFile.appendText(message + "\n", true, true);
    }

    //==============================================================================
    void initialise(const juce::String& commandLine) override
    {
        appJustLaunched = true;
        originalCommandLine = commandLine;
        writeDebugLog("commandLine: " + commandLine);
        writeDebugLog("getCommandLineParameters(): " + getCommandLineParameters());

        File inputMediaFile(commandLine.unquoted().trim());

        if (inputMediaFile.existsAsFile())
        {
            // Create main window with the file name in the title
            mainWindow.reset(
                new MainWindow(getApplicationName() + " - " + inputMediaFile.getFileName()));

            // Load the file directly
            if (auto* mainComp = dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
            {
                mainComp->loadMediaDisplay(inputMediaFile);
            }
        }
        else
        {
            // Only create a blank window if no file was specified
            mainWindow.reset(new MainWindow(getApplicationName()));
        }

        // An ugly solution for an ugy problem
        // Described here https://github.com/juce-framework/JUCE/issues/607
        // It's supposed to happen only in MacOS
        // I'm assuming that no one can re-invoke the app from the DAW within
        // 500ms of the first launch, so this should be safe
        Timer::callAfterDelay(500, [this]() { appJustLaunched = false; });
    }

    void shutdown() override
    {
        // Add your application's shutdown code here..
        mainWindow = nullptr; // (deletes our window)
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void resetWindow(const juce::String& commandLine)
    {
        File inputMediaFile(commandLine.unquoted().trim());

        if (inputMediaFile.existsAsFile())
        {
            URL inputMediaURL = URL(inputMediaFile);
            if (auto* mainComp = dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
            {
                mainComp->loadMediaDisplay(inputMediaURL.getLocalFile());
            }
        }
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        // This method is called when another instance of the app is started.
        // We can handle the command line arguments here to open files in the current instance.
        
        // What happens in reality, is that (at least on MacOS) the app is launched more than once
        // and it depends on the number of arguments passed to the app, and also who is launching it.
        // For example, when the app is launched by Reaper with a file as argument, initialize() is
        // called once with no arguments, and then anotherInstanceStarted() is called with the file as argument.
        // On the other hand when we call the app from the command line like this 
        // `pathToHARP/build/HARP_artefacts/Debug/HARP.app/Contents/MacOS/Harp pathToHARP/test.wav
        // initialize() is called with the file as argument, and then anotherInstanceStarted() is called with the same file as argument.
        // So this is why we need the appJustLaunched flag
        // Hopefully these MacOS hacks won't affect the Linux and Windows versions.
        if (appJustLaunched)
        {
            if (!commandLine.isEmpty() && originalCommandLine.isEmpty())
            {
                // DBG("Replacing original window (empty commandLine) with " + commandLine);
                writeDebugLog("Replacing original window (empty commandLine) with " + commandLine);
                resetWindow(commandLine);
                return;
            }
            writeDebugLog("Ignoring spurious anotherInstanceStarted during startup: " + commandLine);
            return;

        }

        DBG("Another instance started with command line: " + commandLine);

        // First check if it's a valid file
        File inputMediaFile(commandLine.unquoted().trim());

        if (debugFilesOn())
        {
            File debugFile(
                juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName()
                + "/debug.txt");
            debugFile.appendText(
                "Another instance started with command line: " + commandLine + "\n", true, true);
        }

        if (inputMediaFile.existsAsFile())
        {
            // We deal with UI stuff so it's safer to work on the message thread
            MessageManager::callAsync(
                [this, inputMediaFile]()
                {
                    // Check if the user made a choice before
                    bool hasPreference = AppSettings::containsKey("newInstancePreference");
                    int preferenceValue = AppSettings::getIntValue("newInstancePreference", -1);

                    if (hasPreference && preferenceValue >= 0 && preferenceValue <= 1)
                    {
                        handleFileOpenChoice(preferenceValue, inputMediaFile);
                    }
                    else
                    {
                        // show dialog with "Remember my choice" option
                        auto options = MessageBoxOptions()
                                           .withTitle("Open File")
                                           .withMessage("How would you like to open \""
                                                        + inputMediaFile.getFileName() + "\"?")
                                           .withIconType(MessageBoxIconType::QuestionIcon)
                                           .withButton("New Window")
                                           .withButton("Current Window")
                                           .withButton("Cancel");

                        // Create a custom AlertWindow to add the checkbox
                        // I don't use AlertWindow::showAsync() because I couldn't find a way
                        // to add a checkbox to it. MessageBoxOptions doesn't have a .withCustomComponent() method
                        std::unique_ptr<AlertWindow> alertWindow = std::make_unique<AlertWindow>(
                            options.getTitle(), options.getMessage(), options.getIconType());

                        alertWindow->addButton(options.getButtonText(0), 1); // New Window
                        alertWindow->addButton(options.getButtonText(1), 2); // Current Window
                        alertWindow->addButton(options.getButtonText(2), 0); // Cancel

                        // I had to make the checkbox a raw pointer because it couldn't be passed
                        // to the lambda as a unique_ptr.
                        // We also need to manually delete it because AlertWindow doesn't take
                        // ownership of the customComponents
                        // auto* rememberCheckbox = new ToggleButton("Remember my choice");
                        auto rememberCheckbox =
                            std::make_unique<ToggleButton>("Remember my choice");
                        rememberCheckbox->setSize(200, 24);

                        // As stated in the JUCE documentation, alertWindow doesn't take ownership of the customComponents
                        // So we need to delete it manually when the alertWindow is closed
                        alertWindow->addCustomComponent(rememberCheckbox.get());

                        // Show the window asynchronously
                        alertWindow->enterModalState(
                            true,
                            ModalCallbackFunction::create(
                                [this,
                                 alertWindow = alertWindow.release(),
                                 inputMediaFile,
                                 rememberCheckboxPtr = std::move(rememberCheckbox)](int result)
                                {
                                    if (result != 0) // 0 is Cancel in this case
                                    {
                                        int choice = result - 1;

                                        bool rememberChoice = rememberCheckboxPtr->getToggleState();

                                        // Save preference if requested
                                        // Don't save the Cancel choice
                                        if (rememberChoice && choice <= 1)
                                        {
                                            AppSettings::setValue("newInstancePreference", choice);
                                            AppSettings::saveIfNeeded();
                                        }

                                        // Handle the choice
                                        handleFileOpenChoice(choice, inputMediaFile);
                                    }
                                }),
                            true);
                    }
                });
        }
    }

    // method to handle the file opening choice
    void handleFileOpenChoice(int choice, const File& inputMediaFile)
    {
        switch (choice)
        {
            // New Window
            case 0:
            {
                std::unique_ptr<MainWindow> newWindow = std::make_unique<MainWindow>(
                    getApplicationName() + " - " + inputMediaFile.getFileName());

                // Configure the window before making it visible
                if (auto* mainComp = dynamic_cast<MainComponent*>(newWindow->getContentComponent()))
                {
                    mainComp->loadMediaDisplay(inputMediaFile);
                }
                // Store the window and make it visible
                additionalWindows.add(std::move(newWindow));
                break;
            }
            // Load in the current window
            case 1:
            {
                if (auto* mainComp =
                        dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
                {
                    mainComp->loadMediaDisplay(inputMediaFile);
                }
                break;
            }
        }
    }

    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our MainComponent class.
    */
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(
                                 ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons),
              windowIdentifier(name.replaceCharacters(" :", "__"))
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);

#if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
#else
            setResizable(true, true);
            
            // Try to restore saved position and size
            restoreWindowPosition();
#endif

            setVisible(true);
        }

        /*
            Defines the behavior of the window when the user tries to close it.
            Even though we have multiple windows, all of them actually belong to the 
            same application instance. This would mean that if the user chose to 
            close the main window (the first one that was created) then all the other
            would also close. 
            To avoid this, we check if the window is the main one and if it is, we check
            if there are other windows open. If there are, we promote one of them to be the main window.
        */
        void closeButtonPressed() override
        {
            // Save window position and size before closing
            saveWindowPosition();
            
            if (this
                == dynamic_cast<GuiAppApplication*>(JUCEApplication::getInstance())
                       ->getMainWindowPtr())
            {
                auto* app = dynamic_cast<GuiAppApplication*>(JUCEApplication::getInstance());

                // Check if there are other windows open
                if (app->hasAdditionalWindows())
                {
                    // Promote one of the additionalal windows to main window
                    app->promoteAdditionalWindowToMain();
                    // The main window will be deleted automatically when it goes out of scope
                }
                else
                {
                    // No other windows, terminate the application
                    JUCEApplication::getInstance()->systemRequestedQuit();
                }
            }
            else
            {
                // Just remove this window from the additionalWindows array
                dynamic_cast<GuiAppApplication*>(JUCEApplication::getInstance())
                    ->removeAdditionalWindow(this);
            }
        }

        void activeWindowStatusChanged() override
        {
            //Check if an open file is still up to date when window comes back into focus
            if (isActiveWindow())
            {
                if (auto* mainComp = dynamic_cast<MainComponent*>(getContentComponent()))
                {
                    mainComp->focusCallback();
                }
            }
            DocumentWindow::activeWindowStatusChanged();
        }

    private:
        juce::String windowIdentifier;
        
        void saveWindowPosition()
        {
            // Don't save if minimized
            if (isMinimised())
                return;
                
            auto bounds = getBounds();
            
            // Create property names using the window identifier to avoid conflicts
            juce::String prefix = "window." + windowIdentifier + ".";
            AppSettings::setValue(prefix + "x", bounds.getX());
            AppSettings::setValue(prefix + "y", bounds.getY());
            AppSettings::setValue(prefix + "width", bounds.getWidth());
            AppSettings::setValue(prefix + "height", bounds.getHeight());
            
            AppSettings::saveIfNeeded();
        }
        
        void restoreWindowPosition()
        {
            juce::String prefix = "window." + windowIdentifier + ".";
            
            // Check if we have saved position data
            if (AppSettings::containsKey(prefix + "x") && 
                AppSettings::containsKey(prefix + "y") &&
                AppSettings::containsKey(prefix + "width") &&
                AppSettings::containsKey(prefix + "height"))
            {
                // Get the stored position and size
                int x = AppSettings::getIntValue(prefix + "x");
                int y = AppSettings::getIntValue(prefix + "y");
                int width = AppSettings::getIntValue(prefix + "width");
                int height = AppSettings::getIntValue(prefix + "height");
                
                // Validate size
                width = juce::jmax(100, width);
                height = juce::jmax(100, height);
                
                // Create a rectangle with the stored bounds
                juce::Rectangle<int> bounds(x, y, width, height);
                
                // Check if the position is on any display
                auto displays = Desktop::getInstance().getDisplays();
                auto* display = displays.getDisplayForRect(bounds);
                
                if (display != nullptr)
                {
                    // Position is valid
                    setBounds(bounds);
                }
                else
                {
                    // No display found for the saved position, center on the main display
                    centreWithSize(width, height);
                }
            }
            else
            {
                // No saved position, center on screen
                centreWithSize(getWidth(), getHeight());
            }
        }
        
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    MainWindow* getMainWindowPtr() const { return mainWindow.get(); }

    bool hasAdditionalWindows() const { return ! additionalWindows.isEmpty(); }

    void promoteAdditionalWindowToMain()
    {
        // Get the first additional window to be the new main
        if (! additionalWindows.isEmpty())
        {
            mainWindow = std::move(additionalWindows.getReference(0));
            additionalWindows.remove(0);
        }
    }

    void removeAdditionalWindow(MainWindow* windowToRemove)
    {
        for (int i = 0; i < additionalWindows.size(); ++i)
        {
            if (additionalWindows.getReference(i).get() == windowToRemove)
            {
                additionalWindows.remove(i);
                break;
            }
        }
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
    juce::Array<std::unique_ptr<MainWindow>> additionalWindows;
    ApplicationProperties applicationProperties;
    bool appJustLaunched;
    juce::String originalCommandLine;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(GuiAppApplication)
