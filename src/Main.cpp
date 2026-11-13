/**
 * @file Main.cpp
 * @brief Initial JUCE launch code and window management.
 * @author cwitkowitz, xribene, hugofloresgarcia
 */

#include "MainComponent.h"

#include "utils/Logging.h"
#include "utils/Settings.h"

#if JUCE_LINUX
#include <dlfcn.h>
#endif

using namespace juce;

class GuiAppApplication : public JUCEApplication, public FocusChangeListener
{
public:
    GuiAppApplication()
    {
        PropertiesFile::Options options;

        options.applicationName = getApplicationName();
        options.osxLibrarySubFolder = "Application Support/";
        options.filenameSuffix = ".settings";
        /*
          Settings should be located at:
            - macOS: ~/Library/Application Support/HARP/HARP.settings
            - Windows: %APPDATA%\HARP\HARP.settings
            - Linux: ~/.config/HARP/HARP.settings
        */
#if JUCE_LINUX
        // The folder is taken relative to the home directory, where it would otherwise sit
        options.folderName = ".config/" + getApplicationName();
#else
        options.folderName = getApplicationName();
#endif
        options.commonToAllUsers = false;

        moveLegacySettings(options);

        applicationProperties.setStorageParameters(options);

        // Initialize Settings singleton with our application properties
        Settings::initialize(&applicationProperties);
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

    /**
     * Called when the app is invoked.
     */
    void initialise(const String& commandLine) override
    {
        /* Opened before anything is logged. A launch from a DAW cannot be stepped through in a
           debugger, so the log is the only record of how the app was invoked. */
        HARPLogger::getInstance()->initializeLogger();

        DBG_AND_LOG("GuiAppApplication::initialise: Invoked with command line \"" << commandLine
                                                                                  << "\".");

        appJustLaunched = true;

        clearLastFocusedWindow();
        Desktop::getInstance().addFocusChangeListener(this);

        windowCounter++;

        mainWindow = std::make_unique<HARPWindow>(getApplicationName());

        bool showWelcome = Settings::getBoolValue("view.showWelcomePopup", true);

        if (showWelcome)
        {
            if (auto* mainComp =
                    dynamic_cast<MainComponent*>(getMainWindowPtr()->getContentComponent()))
            {
                mainComp->openTutorial();
            }
        }

        launchFiles = getMediaFiles(commandLine);

        importIntoMainWindow(launchFiles);

        // Assumes app can't be re-invoked manually this soon after the initial launch
        Timer::callAfterDelay(launchWindowMs, [this]() { appJustLaunched = false; });
    }

    /**
     * The files named by a command line, leaving out any argument that is not one.
     */
    static Array<File> getMediaFiles(const String& commandLine)
    {
        StringArray args;

        // Split command line arguments at spaces
        args.addTokens(commandLine.trim(), " ", "\"");

        Array<File> files;

        for (const auto& arg : args)
        {
            // A relative path is taken from the directory in which the app was started
            const File file = File::getCurrentWorkingDirectory().getChildFile(arg.unquoted());

            if (file.existsAsFile())
            {
                files.addIfNotAlreadyThere(file);
            }
        }

        return files;
    }

    void importIntoMainWindow(const Array<File>& files)
    {
        if (auto* mainComp = dynamic_cast<MainComponent*>(mainWindow->getContentComponent()))
        {
            for (const auto& file : files)
            {
                mainComp->importNewFile(file, true);
            }
        }
    }

    /**
     * Called when the app is invoked and other instances are running.
     */
    void anotherInstanceStarted(const String& commandLine) override
    {
        const Array<File> files = getMediaFiles(commandLine);

        /*
          With moreThanOneInstanceAllowed() returning false, a single launch can still arrive
          here, depending on its source and on the arguments given.

          For example, when REAPER launches HARP with file path argument(s), initialise() is
          called once with no arguments and anotherInstanceStarted() is subsequently called
          with the arguments. However, when HARP is launched from the command line with
          arguments, initialise() is called with the argument(s), and anotherInstanceStarted()
          is subsequently called with the same argument(s).

          An invocation that arrives while the app is launching is therefore taken as part of
          the launch. Whichever of its files the launch has not imported yet go to the window
          it opened, and the rest are the second delivery of files it already has.

          See also https://github.com/juce-framework/JUCE/issues/607
        */
        if (appJustLaunched)
        {
            Array<File> newFiles;

            for (const auto& file : files)
            {
                if (! launchFiles.contains(file))
                {
                    newFiles.add(file);
                }
            }

            DBG_AND_LOG("GuiAppApplication::anotherInstanceStarted: Launch invoked again with "
                        << "command line \"" << commandLine << "\", which names " << newFiles.size()
                        << " file(s) not imported yet.");

            launchFiles.addArray(newFiles);

            importIntoMainWindow(newFiles);

            return;
        }

        DBG_AND_LOG("GuiAppApplication::anotherInstanceStarted: Another instance started with "
                    << "command line \"" << commandLine << "\".");

        for (const auto& file : files)
        {
            // Choose/handle protocol for additional instances
            tryOpenChoiceWindow(file);
        }
    }

    void shutdown() override
    {
        DBG_AND_LOG("GuiAppApplication::shutdown: Closing " << (1 + additionalWindows.size())
                                                            << " window(s).");

        Desktop::getInstance().removeFocusChangeListener(this);

        clearLastFocusedWindow();

        /* Every window is deleted here, while the settings to which they save their position
           still exist. Left to the destructor, the additional windows would outlive them. */
        additionalWindows.clear();
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
            MainComponent* mainComp = new MainComponent();
            setContentOwned(mainComp, true);
            setResizable(true, true);

            setConstrainer(&constrainer);
            mainComp->updateWindowConstraints();

            // Try to restore saved position and size
            restoreWindowState();

            setVisible(true);
        }

        /* Saved here rather than only when the close button is pressed, since quitting the app
           (e.g., from the menu on macOS) deletes its windows without closing them. */
        ~HARPWindow() override { saveWindowState(); }

        /**
         * Hands the constrainer's minimum size to the window manager.
         *
         * JUCE publishes size limits from the constrainer in
         * XWindowSystem::updateConstraints, but only reaches the branch that reads it
         * when the peer carries ComponentPeer::windowIsResizable. ResizableWindow only
         * sets that flag where Desktop::supportsBorderlessNonClientResize() is true,
         * which on Linux it never is, so the other branch pins the limits to the
         * current size instead. Either way XWindowSystem::setBounds then assigns
         * WM_NORMAL_HINTS outright as USSize | USPosition, dropping whatever was just
         * published, so the window advertises no limits at all and the window manager
         * allows any size. Re-applying the minimum here, after each resize, is what
         * the window manager actually ends up seeing.
         *
         * Snapping back afterwards is not a workable substitute: the window manager
         * may refuse, leaving the window and its contents at different sizes.
         *
         * Xlib is reached through dlopen rather than its headers, whose Window, Time
         * and KeyPress macros collide with JUCE's types. Does nothing on platforms
         * where JUCE already applies the limits itself.
         */
        void publishMinimumSizeToWindowManager()
        {
#if JUCE_LINUX
            auto* peer = getPeer();

            if (peer == nullptr)
            {
                return;
            }

            const auto windowHandle = (unsigned long) (pointer_sized_int) peer->getNativeHandle();

            if (windowHandle == 0)
            {
                return;
            }

            /* Mirrors Xlib's XSizeHints, whose layout is fixed by the X11 ABI */
            struct SizeHints
            {
                long flags;
                int x, y, width, height;
                int minWidth, minHeight;
                int maxWidth, maxHeight;
                int widthInc, heightInc;
                struct
                {
                    int x, y;
                } minAspect, maxAspect;
                int baseWidth, baseHeight;
                int winGravity;
            };

            static constexpr long minSizeFlag = 1L << 4; // PMinSize

            /* Opened once and held for the lifetime of the process. This runs on
               every resize, and a connection set up and torn down per call would
               mean a full X handshake many times a second while a resize is being
               dragged. The connection is deliberately separate from the one JUCE
               holds: Xlib's locking is per-display, so nothing here can race with
               JUCE's own X calls and no ScopedXLock equivalent is needed. */
            struct XLib
            {
                void* display = nullptr;
                SizeHints* (*allocSizeHints)() = nullptr;
                int (*getWMNormalHints)(void*, unsigned long, SizeHints*, long*) = nullptr;
                void (*setWMNormalHints)(void*, unsigned long, SizeHints*) = nullptr;
                int (*freeMemory)(void*) = nullptr;
                int (*flush)(void*) = nullptr;

                XLib()
                {
                    void* handle = dlopen("libX11.so.6", RTLD_LAZY);

                    if (handle == nullptr)
                    {
                        return;
                    }

                    auto* openDisplay = (void* (*) (const char*) ) dlsym(handle, "XOpenDisplay");

                    allocSizeHints = (decltype(allocSizeHints)) dlsym(handle, "XAllocSizeHints");
                    getWMNormalHints =
                        (decltype(getWMNormalHints)) dlsym(handle, "XGetWMNormalHints");
                    setWMNormalHints =
                        (decltype(setWMNormalHints)) dlsym(handle, "XSetWMNormalHints");
                    freeMemory = (decltype(freeMemory)) dlsym(handle, "XFree");
                    flush = (decltype(flush)) dlsym(handle, "XFlush");

                    if (openDisplay != nullptr && allocSizeHints != nullptr
                        && getWMNormalHints != nullptr && setWMNormalHints != nullptr
                        && freeMemory != nullptr && flush != nullptr)
                    {
                        display = openDisplay(nullptr);
                    }
                }

                bool isUsable() const { return display != nullptr; }
            };

            static const XLib xlib;

            if (! xlib.isUsable())
            {
                return;
            }

            if (SizeHints* hints = xlib.allocSizeHints())
            {
                long supplied = 0;

                // Preserve whatever the window already advertises
                xlib.getWMNormalHints(xlib.display, windowHandle, hints, &supplied);

                hints->flags |= minSizeFlag;
                hints->minWidth = constrainer.getMinimumWidth();
                hints->minHeight = constrainer.getMinimumHeight();

                xlib.setWMNormalHints(xlib.display, windowHandle, hints);
                xlib.freeMemory(hints);
            }

            xlib.flush(xlib.display);
#endif
        }

        void resized() override
        {
            DocumentWindow::resized();

            /* JUCE rewrites WM_NORMAL_HINTS while setting the window bounds, and
               replaces the structure rather than merging into it, so the minimum
               has to be applied once that has happened. */
            Component::SafePointer<HARPWindow> safeThis(this);

            MessageManager::callAsync(
                [safeThis]
                {
                    if (safeThis != nullptr)
                    {
                        safeThis->publishMinimumSizeToWindowManager();
                    }
                });
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
            auto* app = dynamic_cast<GuiAppApplication*>(JUCEApplication::getInstance());

            if (this == app->getLastFocusedWindowPtr())
            {
                // Clear last focused window pointer
                app->clearLastFocusedWindow();
            }

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
                    // No other windows, so terminate the application
                    JUCEApplication::getInstance()->systemRequestedQuit();
                }
            }
            else
            {
                app->removeAdditionalWindow(this);
            }
        }

    private:
        String getPrefix() { return "window." + windowIdentifier + "."; }

        /* The state is the one ResizableWindow keeps, which holds the position and size of the
           window and whether it fills the display. On Linux it also holds the size of the frame
           the window manager adds, which is unknown until the window is showing. A window
           placed by its bounds alone opens lower by the height of its title bar on each
           launch. */
        void saveWindowState()
        {
            // Don't save minimized position
            if (! isMinimised())
            {
                Settings::setValue(getPrefix() + "state", getWindowStateAsString(), true);
            }
        }

        void restoreWindowState()
        {
            const String prefix = getPrefix();

            String state = Settings::getString(prefix + "state");

            // Up to 4.0.0 the bounds were saved, each under a key of its own
            const StringArray legacyKeys { "x", "y", "width", "height" };

            if (state.isEmpty() && Settings::containsKey(prefix + "width"))
            {
                StringArray bounds;

                for (const auto& key : legacyKeys)
                {
                    bounds.add(String(Settings::getIntValue(prefix + key)));
                }

                state = bounds.joinIntoString(" ");
            }

            for (const auto& key : legacyKeys)
            {
                Settings::removeValue(prefix + key);
            }

            // A state that places the window outside every display is moved onto the nearest
            if (state.isEmpty() || ! restoreWindowStateFromString(state))
            {
                // No saved position, so center on screen
                centreWithSize(getWidth(), getHeight());
            }
        }

        String windowIdentifier;

        ComponentBoundsConstrainer constrainer;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HARPWindow)
    };

    HARPWindow* getMainWindowPtr() const { return mainWindow.get(); }
    HARPWindow* getLastFocusedWindowPtr() const { return lastFocusedWindow; }

    bool hasAdditionalWindows() const { return ! additionalWindows.isEmpty(); }

    void promoteAdditionalWindowToMain()
    {
        if (! additionalWindows.isEmpty())
        {
            // Promote first additional window in line as new main
            mainWindow = std::move(additionalWindows.getReference(0));
            // Remove from additional windows
            additionalWindows.remove(0);
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

                break;
            }
        }
    }

private:
    /* Moves the settings, and the files HARP keeps beside them, to where they are kept now.
       Up to 4.0.0 they were in a folder of their own in the home directory on Linux. */
    static void moveLegacySettings([[maybe_unused]] const PropertiesFile::Options& options)
    {
#if JUCE_LINUX
        const File legacyFolder =
            File::getSpecialLocation(File::userHomeDirectory).getChildFile(options.applicationName);
        const File settingsFolder = options.getDefaultFile().getParentDirectory();

        if (! legacyFolder.getChildFile(options.getDefaultFile().getFileName()).existsAsFile()
            || options.getDefaultFile().existsAsFile())
        {
            return;
        }

        settingsFolder.createDirectory();

        /* Only what HARP wrote there. A folder named after the app in the home directory can
           hold anything else, a clone of this repository for one. */
        for (const String& fileName : { options.getDefaultFile().getFileName(),
                                        String(ModelCatalog::listingFileName),
                                        String(ModelCatalog::cardsFileName) })
        {
            const File source = legacyFolder.getChildFile(fileName);
            const File target = settingsFolder.getChildFile(fileName);

            if (source.existsAsFile() && ! target.exists())
            {
                source.moveFileTo(target);
            }
        }

        // Only if nothing else was kept there
        if (legacyFolder.getNumberOfChildFiles(File::findFilesAndDirectories) == 0)
        {
            legacyFolder.deleteFile();
        }
#endif
    }

    void globalFocusChanged(Component* focusedComponent) override
    {
        if (focusedComponent == nullptr)
        {
            // Handle case where pointer was cleared prior to callback
            return;
        }

        // Obtain reference to top-level component containing focused child
        Component* topFocusedComponent = focusedComponent->getTopLevelComponent();

        if (HARPWindow* focusedWindow = dynamic_cast<HARPWindow*>(topFocusedComponent))
        {
            if (lastFocusedWindow != focusedWindow)
            {
                // Update pointer to most recently focused HARP window
                lastFocusedWindow = focusedWindow;

                DBG_AND_LOG(
                    "GuiAppApplication::globalFocusChanged: Last focused window updated to \""
                    << focusedWindow->getName() << "\".");
            }
        }
    }

    void clearLastFocusedWindow() { lastFocusedWindow = nullptr; }

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
                bool hasPreference = Settings::containsKey("newInstancePreference");
                int preferenceValue = Settings::getIntValue("newInstancePreference", -1);

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
                    auto alertWindow = std::make_unique<AlertWindow>(
                        options.getTitle(), options.getMessage(), options.getIconType());

                    alertWindow->addButton(options.getButtonText(0), 1); // New window
                    alertWindow->addButton(options.getButtonText(1), 2); // Current window
                    alertWindow->addButton(options.getButtonText(2), 0); // Cancel

                    // AlertWindow does not own its custom components, so the callback does
                    auto rememberCheckbox = std::make_unique<ToggleButton>("Remember my choice");
                    rememberCheckbox->setSize(200, 24);
                    rememberCheckbox->setName("");

                    alertWindow->addCustomComponent(rememberCheckbox.get());

                    // Deleted by the modal manager once it is dismissed
                    alertWindow.release()->enterModalState(
                        true,
                        ModalCallbackFunction::create(
                            [this,
                             inputMediaFile,
                             rememberCheckboxPtr = std::move(rememberCheckbox)](int result)
                            {
                                blockNewModalWindows = false;

                                if (result != 0) // Cancel
                                {
                                    int choice = result - 1;

                                    bool rememberChoice = rememberCheckboxPtr->getToggleState();

                                    // Save the preference if requested
                                    if (rememberChoice)
                                    {
                                        Settings::setValue("newInstancePreference", choice, true);
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
                HARPWindow* windowForFileImport;

                if (lastFocusedWindow != nullptr)
                {
                    windowForFileImport = lastFocusedWindow;
                }
                else
                {
                    DBG_AND_LOG("GuiAppApplication::handleFileOpenChoice: No window currently "
                                "focused. Importing file to main window.");

                    windowForFileImport = getMainWindowPtr();
                }

                if (auto* mainComp =
                        dynamic_cast<MainComponent*>(windowForFileImport->getContentComponent()))
                {
                    // Import file into most recently focused window
                    mainComp->importNewFile(inputMediaFile, true);
                }

                break;
            }
        }
    }

    std::unique_ptr<HARPWindow> mainWindow;
    Array<std::unique_ptr<HARPWindow>> additionalWindows;

    HARPWindow* lastFocusedWindow = nullptr;

    ApplicationProperties applicationProperties;

    int windowCounter = 0;

    // How long after starting an invocation is still taken as part of the launch
    static constexpr int launchWindowMs = 3000;

    // Files the launch has imported, which the same launch may deliver a second time
    Array<File> launchFiles;

    std::atomic<bool> appJustLaunched { false };
    std::atomic<bool> blockNewModalWindows { false };
};

// Generates main() routine that launches app
START_JUCE_APPLICATION(GuiAppApplication)
