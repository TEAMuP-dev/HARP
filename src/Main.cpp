#include "MainComponent.h"

//==============================================================================
class GuiAppApplication : public juce::JUCEApplication
{
public:
    //==============================================================================
    GuiAppApplication() {}

    // We inject these as compile definitions from the CMakeLists.txt
    // If you've enabled the juce header with `juce_generate_juce_header(<thisTarget>)`
    // you could `#include <JuceHeader.h>` and use `ProjectInfo::projectName` etc. instead.
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return true; }

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

        // if (inputMediaFile.existsAsFile())
        // {
        //     URL inputMediaURL = URL(inputMediaFile);
        //     if (auto* mainComp = dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
        //     {
        //         mainComp->loadMediaDisplay(inputMediaURL.getLocalFile());
        //     }
        // }
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
        // save the command line arguments to a debug file in my home directory
        if (debugFilesOn())
        {
            File debugFile(
                juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName()
                + "/debug2.txt");
            debugFile.appendText(commandLine + "\n", true, true);
            debugFile.appendText(getCommandLineParameters() + "\n", true, true);
            debugFile.appendText(
                juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName()
                    + "\n",
                true,
                true);
        }

        resetWindow(commandLine);
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
            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.
            JUCEApplication::getInstance()->systemRequestedQuit();
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

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION(GuiAppApplication)
