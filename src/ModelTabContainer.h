/**
 * @brief Adds tab container to HARP for MultiTabs
 * @author JEYuhas
 */
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "HomeTab.h"
#include "Model.h"
#include "ModelTab.h"

#include "widgets/ControlAreaWidget.h"
#include "widgets/ModelInfoWidget.h"
#include "widgets/ModelSelectionWidget.h"
#include "widgets/TrackAreaWidget.h"

#include "utils/Errors.h"
#include "utils/Interface.h"
#include "utils/Logging.h"
#include "utils/ModelRegistry.h"
#include "utils/Tutorial.h"

#include "media/MediaDisplayComponent.h"

using namespace juce;

class ModelTabsLookAndFeel : public LookAndFeel_V4
{
public:
    void drawTabbedButtonBarBackground(TabbedButtonBar& bar, Graphics& g) override
    {
        g.fillAll(tabBarColour);
        g.setColour(separatorColour);
        g.fillRect(0, bar.getHeight() - 1, bar.getWidth(), 1);
    }

    void drawTabAreaBehindFrontButton(TabbedButtonBar&, Graphics& g, int w, int h) override
    {
        g.setColour(separatorColour);
        g.fillRect(0, h - 1, w, 1);
    }

    void drawTabButton(TabBarButton& button,
                       Graphics& g,
                       bool isMouseOver,
                       bool isMouseDown) override
    {
        const auto isActive = button.isFrontTab();
        auto area = button.getActiveArea();

        const auto fill = isActive
                              ? activeTabColour
                              : inactiveTabColour.brighter(isMouseOver || isMouseDown ? 0.08f : 0.0f);

        g.setColour(fill);
        g.fillRect(area);

        if (button.getIndex() > 0)
        {
            g.setColour(separatorColour);
            g.fillRect(area.getX(), area.getY() + 2, 1, area.getHeight() - 4);
        }

        auto textArea = button.getTextArea().reduced(tabTextInset, 0);

        g.setColour(isActive ? activeTextColour
                     : inactiveTextColour);

        g.drawText(button.getButtonText(),
                textArea,
                Justification::centred,
                true);
    }

    int getTabButtonBestWidth(TabBarButton& button, int tabDepth) override
    {
        return button.getButtonText() == "Home"
           ? homeTabWidth
           : fixedTabWidth;
    }

    void drawTabButtonText(TabBarButton&,
                           Graphics&,
                           bool /*isMouseOver*/,
                           bool /*isMouseDown*/) override
    {
    }

private:
    const Colour tabBarColour { Colour(0xff1f1f1f) };
    const Colour inactiveTabColour { Colour(0xff242424) };
    const Colour activeTabColour { Colour(0xff343434) };
    const Colour separatorColour { Colour(0xff4a4a4a) };
    const Colour activeTextColour { Colours::white };
    const Colour inactiveTextColour { Colour(0xffaeb0b4) };
    static constexpr int fixedTabWidth = 140;
    static constexpr int homeTabWidth = 64;
    static constexpr int tabTextInset = 10;
};

/**
 * Scrolling page that holds one model tab.
 *
 * The tab is given the width the page can show and whatever height it needs, so that a
 * window too small for the content scrolls instead of clipping it. The tab itself is
 * owned by ModelTabContainer, since its lifetime is tied to the requests it has in flight.
 */
class ModelTabPage : public Viewport
{
public:
    explicit ModelTabPage(ModelTab& tabToShow) : modelTab(tabToShow)
    {
        setViewedComponent(&modelTab, false);
        setScrollBarsShown(true, false);
        setScrollOnDragMode(Viewport::ScrollOnDragMode::never);
    }

    ModelTab& getModelTab() const { return modelTab; }

    void resized() override
    {
        Viewport::resized();

        layOutTab();
    }

    void layOutTab()
    {
        /* Laying out once can make the scrollbar appear, which narrows the visible
           area and would leave it overlapping the content. Lay out again whenever the
           available width changed as a result. */
        const int firstWidth = layOutTabForVisibleWidth();

        if (firstWidth > 0 && getMaximumVisibleWidth() != firstWidth)
        {
            layOutTabForVisibleWidth();
        }
    }

    /* Tracks use the wheel to zoom their contents, so the page must not treat a wheel
       event over one as a request to scroll. Viewport::useMouseWheelMoveIfNeeded is not
       virtual, so the decision is made here instead. */
    void mouseWheelMove(const MouseEvent& e, const MouseWheelDetails& wheel) override
    {
        /* Test originalComponent, not eventComponent: a wheel event that goes unhandled
           is passed up the hierarchy with getEventRelativeTo, which rewrites
           eventComponent to each parent in turn, so by the time it arrives here
           eventComponent is this viewport. originalComponent still names the component
           the wheel was actually over. */
        if (isWithinTrack(e.originalComponent))
        {
            return;
        }

        Viewport::mouseWheelMove(e, wheel);
    }

    /* Scrolling moves the tab under the tutorial overlay, which draws its highlight in
       window coordinates and would otherwise keep pointing at where a component used
       to be. */
    void visibleAreaChanged(const Rectangle<int>&) override
    {
        if (onScrolled != nullptr)
        {
            onScrolled();
        }
    }

    std::function<void()> onScrolled;

private:
    int layOutTabForVisibleWidth()
    {
        const int visibleWidth = getMaximumVisibleWidth();

        if (visibleWidth <= 0)
        {
            return 0;
        }

        const int requiredHeight = modelTab.getMinimumRequiredHeightForWidth(visibleWidth);

        modelTab.setSize(visibleWidth, jmax(requiredHeight, getMaximumVisibleHeight()));

        return visibleWidth;
    }

    /* True when the wheel landed on a track that will act on it. A track with no media
       loaded does not, so the page should still scroll over it. */
    static bool isWithinTrack(Component* c)
    {
        for (auto* candidate = c; candidate != nullptr; candidate = candidate->getParentComponent())
        {
            if (auto* track = dynamic_cast<MediaDisplayComponent*>(candidate))
            {
                return track->usesMouseWheel();
            }
        }

        return false;
    }

    ModelTab& modelTab;
};

class ModelTabContainer : public TabbedComponent,
                          private ChangeListener,
                          public ChangeBroadcaster
{
public:
    ModelTabContainer()
        : TabbedComponent(TabbedButtonBar::TabsAtTop)
    {
        getTabbedButtonBar().setLookAndFeel(&tabsLookAndFeel);

        setColour(TabbedComponent::backgroundColourId, tabBackgroundColour);
        getTabbedButtonBar().setColour(TabbedButtonBar::tabTextColourId, Colours::white);
        getTabbedButtonBar().setColour(TabbedButtonBar::frontTextColourId, Colours::white);
        getTabbedButtonBar().setColour(TabbedButtonBar::tabOutlineColourId, tabBackgroundColour.darker(0.35f));
        getTabbedButtonBar().setColour(TabbedButtonBar::frontOutlineColourId, tabBackgroundColour.darker(0.35f));

        createHomeTab();
    }

    ~ModelTabContainer() override
    {
        getTabbedButtonBar().setLookAndFeel(nullptr);

        // App shutdown: a tab that is still open (never closed) may have an
        // in-flight request. Letting modelTabs auto-destruct would delete such
        // a tab and destroy its ThreadPool while a worker is blocked in a
        // network syscall, force-killing it (see ModelTab::hasPendingRequests).
        // Waiting for the request instead would stall the quit for as long as
        // its timeout, so abandon it - which aborts the connection, so that the
        // OS networking task resolves now rather than minutes from now, once
        // too much of the app has gone away to deliver the result safely - and
        // release ownership without deleting, leaving the threads and memory
        // for the exiting process to reclaim. Idle tabs are left in the array
        // and destroyed normally.
        for (int i = modelTabs.size(); --i >= 0;)
        {
            if (ModelTab* tab = modelTabs.getUnchecked(i); tab->hasPendingRequests())
            {
                tab->abandon();
                modelTabs.remove(i, false);
            }
        }
    }

    ModelTab* createNewTab(const String& modelPath = {}, const String& modelName = {})
    {
        int index = getNumTabs();

        auto* tab = new ModelTab();

        // Owned from creation, so that a tab is never left unowned while a
        // request is in flight (see closeModelTab and the destructor)
        modelTabs.add(tab);

        auto tabName = modelName;

        if (tabName.isEmpty() && modelPath.isNotEmpty())
            tabName = ModelRegistry::getEntryForPath(modelPath).displayName;

        if (tabName.isEmpty())
            tabName = "Model " + String(index);

        addLoadedModelTab(tab, tabName);

        if (modelPath.isNotEmpty())
            tab->loadModelPath(modelPath);

        return tab;
    }

    ModelTabPage* getCurrentModelTabPage() const
    {
        return dynamic_cast<ModelTabPage*>(getCurrentContentComponent());
    }

    ModelTab* getCurrentModelTab() const
    {
        auto* page = getCurrentModelTabPage();
        return page != nullptr ? &page->getModelTab() : nullptr;
    }

    HomeTab* getHomeTabIfShowing() const
    {
        return dynamic_cast<HomeTab*>(getCurrentContentComponent());
    }

    void layOutCurrentPage()
    {
        if (auto* page = getCurrentModelTabPage())
            page->layOutTab();
    }

    void currentTabChanged(int newCurrentTabIndex, const String& newCurrentTabName) override
    {
        TabbedComponent::currentTabChanged(newCurrentTabIndex, newCurrentTabName);

        // Window constraints and the tutorial both follow whichever tab is showing
        sendChangeMessage();
    }

    // Called whenever the page showing a model tab scrolls
    std::function<void()> onPageScrolled;

    // Closes a model tab as if its close button had been clicked. Does nothing
    // if the tab is no longer in the tab bar.
    void closeTab(ModelTab* tab) { closeModelTab(tab); }

private:
    void addLoadedModelTab(ModelTab* tab, const String& tabName)
    {
        tab->addChangeListener(this);

        auto* page = pages.add(new ModelTabPage(*tab));
        page->onScrolled = [this]
        {
            if (onPageScrolled != nullptr)
                onPageScrolled();
        };

        // The page is owned by pages and the tab by modelTabs, so it is added with
        // deleteComponentWhenNotNeeded = false: closing it must not force JUCE
        // to destroy the tab synchronously in removeTab(); see closeModelTab() for
        // why destruction may need to be deferred.
        addTab(tabName,
               tabBackgroundColour,
               page,
               false);

        addCloseButtonToModelTab(tab);

        setCurrentTabIndex(getNumTabs() - 1);
    }

    void addCloseButtonToModelTab(ModelTab* tab)
    {
        auto* closeButton = new TextButton("x");
        closeButton->setTooltip("Close model tab");
        closeButton->setSize(18, 18);
        closeButton->setColour(TextButton::buttonColourId, Colours::transparentBlack);
        closeButton->setColour(TextButton::buttonOnColourId, Colours::transparentBlack);
        closeButton->setColour(TextButton::textColourOffId, Colours::white);
        closeButton->setColour(TextButton::textColourOnId, Colours::white);
        closeButton->onClick = [this, tab] { closeModelTab(tab); };

        if (auto* tabButton = getTabbedButtonBar().getTabButton(getNumTabs() - 1))
            tabButton->setExtraComponent(closeButton, TabBarButton::afterText);
    }

    void closeModelTab(ModelTab* tabToClose)
    {
        for (int i = 1; i < getNumTabs(); ++i)
        {
            auto* page = dynamic_cast<ModelTabPage*>(getTabContentComponent(i));

            if (page != nullptr && &page->getModelTab() == tabToClose)
            {
                const auto currentIndex = getCurrentTabIndex();
                const auto targetIndex = currentIndex == i ? jmax(0, i - 1)
                                                           : (currentIndex > i ? currentIndex - 1
                                                                               : currentIndex);

                // Remove the tab from the UI immediately so it looks closed to
                // the user. Because the tab was added with
                // deleteComponentWhenNotNeeded = false, removeTab() does not
                // destroy it; we own it via modelTabs.
                removeTab(i);

                // Deleting the page only detaches the tab from it
                pages.removeObject(page);

                if (getNumTabs() > 0)
                    setCurrentTabIndex(jlimit(0, getNumTabs() - 1, targetIndex));

                tabToClose->removeChangeListener(this);

                if (tabToClose->hasPendingRequests())
                {
                    // A network request is still in flight. Destroying the tab
                    // (and its ThreadPool) now would force-kill a worker thread
                    // blocked in a network syscall. Abandoning it aborts the
                    // connection so the worker returns within moments; hand
                    // ownership to the reaper, which deletes it once it does.
                    tabToClose->abandon();

                    modelTabs.removeObject(tabToClose, false);
                    tabReaper.add(tabToClose);
                }
                else
                {
                    modelTabs.removeObject(tabToClose, true);
                }

                sendChangeMessage();
                return;
            }
        }
    }

    void createHomeTab()
    {
        auto* homeTab = new HomeTab();
        homeTab->onModelLoadRequested = [this, homeTab](String modelPath, String modelName)
        {
            // Owned from creation as well: this tab is not in the UI yet, but it
            // has a load request in flight, so quitting the app now must find it
            // in modelTabs and abandon it rather than leave it running.
            auto* pendingTab = new ModelTab();
            modelTabs.add(pendingTab);

            pendingTab->onNextModelLoadComplete(
                [this, homeTab, modelName](ModelTab* tab, bool wasSuccessful)
                {
                    if (wasSuccessful)
                    {
                        addLoadedModelTab(tab, modelName);
                        sendChangeMessage();
                    }
                    else
                    {
                        // Deleted asynchronously because this runs from inside
                        // the tab's own load completion handler
                        modelTabs.removeObject(tab, false);
                        MessageManager::callAsync([tab] { delete tab; });
                    }

                    homeTab->resetSelection();
                });

            pendingTab->loadModelPath(modelPath);
        };

        addTab("Home",
               tabBackgroundColour,
               homeTab,
               false);

        setCurrentTabIndex(0);
    }

    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        if (auto* tab = dynamic_cast<ModelTab*>(source))
        {
            // What the tab has to show changed, and so how far its page has to scroll
            for (auto* page : pages)
            {
                if (&page->getModelTab() == tab)
                    page->layOutTab();
            }

            sendChangeMessage(); // bubble up to MainComponent
        }
    }

    // Owns model tabs whose UI has been closed while a request was still in
    // flight. Polls each tab until its thread pools are idle, then deletes it on
    // the message thread so that no worker thread is force-killed mid-request.
    class DeferredTabReaper : private Timer
    {
    public:
        ~DeferredTabReaper() override
        {
            stopTimer();

            // The reaper is only destroyed when the container is, i.e. at app
            // shutdown. Any tab still pending here has an in-flight request;
            // deleting it would destroy a ThreadPool whose worker is blocked in
            // a network syscall, force-killing it. Their connections were
            // already aborted when they were added, so release ownership
            // without deleting and let the exiting process reclaim the threads
            // and memory.
            pendingTabs.clear(false);
        }

        void add(ModelTab* tab)
        {
            pendingTabs.add(tab);

            if (! isTimerRunning())
                startTimer(pollIntervalMs);
        }

    private:
        void timerCallback() override
        {
            for (int i = pendingTabs.size(); --i >= 0;)
            {
                ModelTab* tab = pendingTabs.getUnchecked(i);

                if (! tab->hasPendingRequests())
                {
                    // Release ownership and delete via callAsync so that any
                    // completion callbacks the finished job already queued run
                    // before the tab is destroyed.
                    pendingTabs.removeObject(tab, false);
                    MessageManager::callAsync([tab] { delete tab; });
                }
            }

            if (pendingTabs.isEmpty())
                stopTimer();
        }

        static constexpr int pollIntervalMs = 250;

        OwnedArray<ModelTab> pendingTabs;
    };

    const Colour tabBackgroundColour {
        getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground)
    };

    ModelTabsLookAndFeel tabsLookAndFeel;

    OwnedArray<ModelTab> modelTabs;
    // Declared after modelTabs so that each page is destroyed before the tab it shows
    OwnedArray<ModelTabPage> pages;
    DeferredTabReaper tabReaper;
};
