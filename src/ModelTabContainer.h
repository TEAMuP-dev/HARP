/**
 * @file ModelTabContainer.h
 * @brief Tab bar holding the Home tab and one tab per opened model.
 * @author cwitkowitz, VedMistry42, 2cylu2, JEYuhas
 */

#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "HomeTab.h"
#include "ModelTab.h"

#include "media/MediaDisplayComponent.h"

#include "utils/Interface.h"
#include "utils/ModelCatalog.h"

#include "windows/tutorial/TutorialTargets.h"

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

        const auto fill =
            isActive ? activeTabColour
                     : inactiveTabColour.brighter(isMouseOver || isMouseDown ? 0.08f : 0.0f);

        g.setColour(fill);
        g.fillRect(area);

        if (button.getIndex() > 0)
        {
            g.setColour(separatorColour);
            g.fillRect(area.getX(), area.getY() + 2, 1, area.getHeight() - 4);
        }

        auto textArea = button.getTextArea().reduced(tabTextInset, 0);

        g.setColour(isActive ? activeTextColour : inactiveTextColour);

        g.drawText(button.getButtonText(), textArea, Justification::centred, true);
    }

    int getTabButtonBestWidth(TabBarButton& button, int /*tabDepth*/) override
    {
        // The Home tab is always first
        return button.getIndex() == 0 ? homeTabWidth : fixedTabWidth;
    }

    /* The default gives a tab's close button the full height of the tab, which stretches it,
       so it is kept square, centred, and clear of the tab's edge instead */
    Rectangle<int> getTabButtonExtraComponentBounds(const TabBarButton& button,
                                                    Rectangle<int>& textArea,
                                                    Component& extraComponent) override
    {
        textArea.removeFromRight(extraComponentMargin);

        const int side = extraComponent.getWidth();

        return LookAndFeel_V4::getTabButtonExtraComponentBounds(button, textArea, extraComponent)
            .withSizeKeepingCentre(side, side);
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
    static constexpr int extraComponentMargin = 6;
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

class ModelTabContainer : public TabbedComponent, private ChangeListener, public ChangeBroadcaster
{
public:
    ModelTabContainer() : TabbedComponent(TabbedButtonBar::TabsAtTop)
    {
        getTabbedButtonBar().setLookAndFeel(&tabsLookAndFeel);

        setColour(TabbedComponent::backgroundColourId, tabBackgroundColour);
        getTabbedButtonBar().setColour(TabbedButtonBar::tabTextColourId, Colours::white);
        getTabbedButtonBar().setColour(TabbedButtonBar::frontTextColourId, Colours::white);
        getTabbedButtonBar().setColour(TabbedButtonBar::tabOutlineColourId,
                                       tabBackgroundColour.darker(0.35f));
        getTabbedButtonBar().setColour(TabbedButtonBar::frontOutlineColourId,
                                       tabBackgroundColour.darker(0.35f));

        homeTab.onModelOpenRequested = [this](const String& modelPath, const String& modelName)
        { openModelTab(modelPath, modelName); };

        addTab("Home", tabBackgroundColour, &homeTab, false);
        setCurrentTabIndex(0);

        setComponentID(TutorialTargets::modelTabs);
        getTabbedButtonBar().setComponentID(TutorialTargets::tabBar);
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

    /**
     * Opens a model in a new tab and shows it. The tab fills in once the model has loaded,
     * and closes by itself if loading fails, once the error has been dismissed.
     */
    ModelTab* openModelTab(const String& modelPath, String tabName = {})
    {
        if (tabName.isEmpty())
        {
            const auto* entry = catalog->findEntry(modelPath);
            tabName =
                entry != nullptr ? entry->name : modelPath.fromLastOccurrenceOf("/", false, false);
        }

        auto* tab = new ModelTab();

        // Owned from creation, so that a tab is never left unowned while a
        // request is in flight (see closeTab and the destructor)
        modelTabs.add(tab);
        tab->addChangeListener(this);

        auto* page = pages.add(new ModelTabPage(*tab));

        // The page is owned by pages and the tab by modelTabs, so it is added with
        // deleteComponentWhenNotNeeded = false: closing it must not force JUCE
        // to destroy the tab synchronously in removeTab(); see closeTab() for
        // why destruction may need to be deferred.
        addTab(tabName, tabBackgroundColour, page, false);
        addCloseButton(*tab, getNumTabs() - 1);

        setCurrentTabIndex(getNumTabs() - 1);

        tab->loadModel(modelPath);

        return tab;
    }

    // Closes a model tab as if its close button had been clicked. Does nothing
    // if the tab is no longer in the tab bar.
    void closeTab(ModelTab* tabToClose)
    {
        const int index = findTabIndex(tabToClose);

        if (index < 0)
            return;

        const auto currentIndex = getCurrentTabIndex();
        const auto targetIndex = currentIndex == index
                                     ? jmax(0, index - 1)
                                     : (currentIndex > index ? currentIndex - 1 : currentIndex);

        // Remove the tab from the UI immediately so it looks closed to the user.
        // Because the page was added with deleteComponentWhenNotNeeded = false,
        // removeTab() does not destroy it; it is owned via pages.
        removeTab(index);

        // Deleting the page only detaches the tab from it
        pages.removeObject(findPage(tabToClose));

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

private:
    // Close button shown on each model tab, which explains itself in the instructions box
    struct CloseTabButton : public Button
    {
        CloseTabButton() : Button("Close") {}

        void paintButton(Graphics& g, bool isHighlighted, bool isDown) override
        {
            // Square whatever this is given, so that the circle and cross are never stretched
            const float side = (float) jmin(getWidth(), getHeight());
            const auto bounds =
                getLocalBounds().toFloat().withSizeKeepingCentre(side, side).reduced(1.0f);

            if (isHighlighted || isDown)
            {
                g.setColour(Colours::white.withAlpha(isDown ? 0.2f : 0.12f));
                g.fillEllipse(bounds);
            }

            // A cross drawn from two strokes, which sits centred where a glyph might not
            const auto cross = bounds.reduced(bounds.getWidth() * 0.32f);

            g.setColour(isHighlighted ? Colours::white : Colours::lightgrey);
            g.drawLine({ cross.getTopLeft(), cross.getBottomRight() }, 1.5f);
            g.drawLine({ cross.getBottomLeft(), cross.getTopRight() }, 1.5f);
        }

        void mouseEnter(const MouseEvent& e) override
        {
            Button::mouseEnter(e);
            instructionsMessage->setMessage("Click to close this model tab.");
        }

        void mouseExit(const MouseEvent& e) override
        {
            Button::mouseExit(e);
            instructionsMessage->clearMessage();
        }

        SharedResourcePointer<InstructionsMessage> instructionsMessage;
    };

    void addCloseButton(ModelTab& tab, int tabIndex)
    {
        auto* closeButton = new CloseTabButton();
        closeButton->setSize(18, 18);
        closeButton->onClick = [this, safeTab = SafePointer<ModelTab>(&tab)]
        {
            // Deferred, since closing deletes the tab button that owns this one
            MessageManager::callAsync(
                [safeThis = SafePointer<ModelTabContainer>(this), safeTab]
                {
                    if (safeThis != nullptr && safeTab != nullptr)
                        safeThis->closeTab(safeTab.getComponent());
                });
        };

        // The tab button takes ownership of the close button
        if (auto* tabButton = getTabbedButtonBar().getTabButton(tabIndex))
            tabButton->setExtraComponent(closeButton, TabBarButton::afterText);
        else
            delete closeButton;
    }

    ModelTabPage* findPage(const ModelTab* tab) const
    {
        for (auto* page : pages)
        {
            if (&page->getModelTab() == tab)
                return page;
        }

        return nullptr;
    }

    int findTabIndex(const ModelTab* tab) const
    {
        for (int i = 1; i < getNumTabs(); ++i)
        {
            auto* page = dynamic_cast<ModelTabPage*>(getTabContentComponent(i));

            if (page != nullptr && &page->getModelTab() == tab)
                return i;
        }

        return -1;
    }

    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        auto* tab = dynamic_cast<ModelTab*>(source);

        if (tab == nullptr)
            return;

        // A tab left without a model once its load finished failed to load one, and
        // has nothing to show now that its error has been dismissed
        if (! tab->isModelLoaded() && ! tab->isLoading())
        {
            closeTab(tab);
            return;
        }

        // What the tab has to show changed, and so how far its page has to scroll
        if (auto* page = findPage(tab))
            page->layOutTab();

        sendChangeMessage(); // bubble up to MainComponent
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

    const Colour tabBackgroundColour { getUIColourIfAvailable(
        LookAndFeel_V4::ColourScheme::UIColour::windowBackground) };

    ModelTabsLookAndFeel tabsLookAndFeel;

    HomeTab homeTab;

    OwnedArray<ModelTab> modelTabs;
    // Declared after modelTabs so that each page is destroyed before the tab it shows
    OwnedArray<ModelTabPage> pages;
    DeferredTabReaper tabReaper;

    SharedResourcePointer<ModelCatalog> catalog;
};
