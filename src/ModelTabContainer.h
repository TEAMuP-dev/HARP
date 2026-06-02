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
    }

    ModelTab* createNewTab(const String& modelPath = {}, const String& modelName = {})
    {
        int index = getNumTabs();

        auto* tab = new ModelTab();
        tab->addChangeListener(this);

        auto tabName = modelName;

        if (tabName.isEmpty() && modelPath.isNotEmpty())
            tabName = ModelRegistry::getEntryForPath(modelPath).displayName;

        if (tabName.isEmpty())
            tabName = "Model " + String(index);

        addTab(tabName,
               tabBackgroundColour,
               tab,
               true);

        addCloseButtonToModelTab(tab);

        setCurrentTabIndex(getNumTabs() - 1);

        if (modelPath.isNotEmpty())
            tab->loadModelPath(modelPath);

        return tab;
    }

    ModelTab* getCurrentModelTab() const
    {
        return dynamic_cast<ModelTab*>(getCurrentContentComponent());
    }

    ModelTab* getFirstModelTab() const
    {
        for (int i = 0; i < getNumTabs(); ++i)
        {
            if (auto* tab = dynamic_cast<ModelTab*>(getTabContentComponent(i)))
                return tab;
        }

        return nullptr;
    }

private:
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
            if (getTabContentComponent(i) == tabToClose)
            {
                const auto currentIndex = getCurrentTabIndex();
                const auto targetIndex = currentIndex == i ? jmax(0, i - 1)
                                                           : (currentIndex > i ? currentIndex - 1
                                                                               : currentIndex);

                removeTab(i);

                if (getNumTabs() > 0)
                    setCurrentTabIndex(jlimit(0, getNumTabs() - 1, targetIndex));

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
            createNewTab(modelPath, modelName);
            homeTab->resetSelection();
        };

        addTab("Home",
               tabBackgroundColour,
               homeTab,
               false);

        setCurrentTabIndex(0);
    }

    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        if (dynamic_cast<ModelTab*>(source))
        {
            sendChangeMessage(); // bubble up to MainComponent
        }
    }

    const Colour tabBackgroundColour {
        getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground)
    };

    ModelTabsLookAndFeel tabsLookAndFeel;
};
