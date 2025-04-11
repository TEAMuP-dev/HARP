#include "MainComponent.h"

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
    }

    // We inject these as compile definitions from the CMakeLists.txt
    // If you've enabled the juce header with `juce_generate_juce_header(<thisTarget>)`
    // you could `#include <JuceHeader.h>` and use `ProjectInfo::projectName` etc. instead.
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    bool debugFilesOn() { return false; }

    //==============================================================================
    void initialise(const juce::String& commandLine) override
    {
        // save the command line arguments to a debug file in my home directory
        if (debugFilesOn())
        {
            File debugFile(
                juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName()
                + "/debug.txt");
            debugFile.appendText(commandLine + "\n", true, true);
            debugFile.appendText(getCommandLineParameters() + "\n", true, true);
            debugFile.appendText(
                juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName()
                    + "\n",
                true,
                true);
        }

        mainWindow.reset(new MainWindow(getApplicationName()));
        resetWindow(commandLine);
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
        // First check if it's a valid file
        File inputMediaFile(commandLine.unquoted().trim());

        if (inputMediaFile.existsAsFile())
        {
            // We deal with UI stuff so it's safe to work on the message thread
            MessageManager::callAsync(
                [this, inputMediaFile]()
                {
                    auto* userSettings = applicationProperties.getUserSettings();

                    // Check if the user made a choice before
                    bool hasPreference = userSettings->containsKey("newInstancePreference");
                    int preferenceValue = userSettings->getIntValue("newInstancePreference", -1);

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
                                 userSettings,
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
                                            userSettings->setValue("newInstancePreference", choice);
                                            userSettings->saveIfNeeded();
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
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);

#if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
#else
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
#endif

            setVisible(true);
        }

        void closeButtonPressed() override
        {
            // If this is the main window
            if (this
                == dynamic_cast<GuiAppApplication*>(JUCEApplication::getInstance())
                       ->getMainWindowPtr())
            {
                JUCEApplication::getInstance()->systemRequestedQuit();
            }
            else
            {
                // Just remove this window from the additionalWindows array
                dynamic_cast<GuiAppApplication*>(JUCEApplication::getInstance())
                    ->removeAdditionalWindow(this);
            }
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

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
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    MainWindow* getMainWindowPtr() const { return mainWindow.get(); }

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
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(GuiAppApplication)
