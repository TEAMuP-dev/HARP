/**
 * @brief Adds tab container to HARP for MultiTabs
 * @author JEYuhas
 */
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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
        createNewTab(); // start with one
    }

    void createNewTab()
    {
        int index = getNumTabs() + 1;

        auto* tab = new ModelTab();
        tab->addChangeListener(this);

        addTab("Model " + String(index),
               Colours::lightgrey,
               tab,
               true);

        setCurrentTabIndex(index - 1);
    }

    ModelTab* getCurrentModelTab() const
    {
        return dynamic_cast<ModelTab*>(getCurrentContentComponent());
    }

private:
    void changeListenerCallback(ChangeBroadcaster* source) override
    {
        if (dynamic_cast<ModelTab*>(source))
        {
            sendChangeMessage(); // bubble up to MainComponent
        }
    }
};
