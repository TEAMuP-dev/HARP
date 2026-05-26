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
#include "utils/Tutorial.h"

using namespace juce;

class ModelTabContainer : public TabbedComponent,
                          private ChangeListener,
                          public ChangeBroadcaster
{
public:
    ModelTabContainer()
        : TabbedComponent(TabbedButtonBar::TabsAtTop)
    {
        setColour(TabbedComponent::backgroundColourId, tabBackgroundColour);
        getTabbedButtonBar().setColour(TabbedButtonBar::tabTextColourId, Colours::white);
        getTabbedButtonBar().setColour(TabbedButtonBar::frontTextColourId, Colours::white);
        getTabbedButtonBar().setColour(TabbedButtonBar::tabOutlineColourId, tabBackgroundColour.darker(0.35f));
        getTabbedButtonBar().setColour(TabbedButtonBar::frontOutlineColourId, tabBackgroundColour.darker(0.35f));

        createHomeTab();
    }

    ModelTab* createNewTab(const String& modelPath = {})
    {
        int index = getNumTabs();

        auto* tab = new ModelTab();
        tab->addChangeListener(this);

        addTab("Model " + String(index),
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
        closeButton->setColour(TextButton::buttonColourId, tabBackgroundColour);
        closeButton->setColour(TextButton::buttonOnColourId, tabBackgroundColour.brighter(0.1f));
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
                removeTab(i);
                sendChangeMessage();
                return;
            }
        }
    }

    void createHomeTab()
    {
        auto* homeTab = new HomeTab();
        homeTab->onModelLoadRequested = [this, homeTab](String modelPath)
        {
            createNewTab(modelPath);
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
};
