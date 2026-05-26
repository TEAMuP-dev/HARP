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
        createHomeTab();
    }

    ModelTab* createNewTab(const String& modelPath = {})
    {
        int index = getNumTabs();

        auto* tab = new ModelTab();
        tab->addChangeListener(this);

        addTab("Model " + String(index),
               Colours::lightgrey,
               tab,
               true);

        setCurrentTabIndex(getNumTabs() - 1);

        if (modelPath.isNotEmpty())
            tab->loadModelPath(modelPath);

        return tab;
    }

    ModelTab* getCurrentModelTab() const
    {
        return dynamic_cast<ModelTab*>(getCurrentContentComponent());
    }

private:
    void createHomeTab()
    {
        auto* homeTab = new HomeTab();
        homeTab->onModelLoadRequested = [this, homeTab](String modelPath)
        {
            createNewTab(modelPath);
            homeTab->resetSelection();
        };

        addTab("Home",
               Colours::lightgrey,
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
};
