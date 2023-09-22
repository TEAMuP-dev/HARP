/**
 * @file
 *
 * @brief Implementation of the ARA Editor View.
 * @author xribene
 */

#pragma once
#include "juce_gui_basics/juce_gui_basics.h"

#include <ARA_Library/PlugIn/ARAPlug.h>
#include <ARA_Library/Utilities/ARAPitchInterpretation.h>
#include <ARA_Library/Utilities/ARATimelineConversion.h>
#include "../DeepLearning/WebModel.h" // needed for ModelCardListener
#include "../UI/CustomComponents.h"


using namespace juce;

/**
 * @class EditorRenderer
 * @brief TODO: Write brief class description.
 */
class EditorView : public juce::ARAEditorView
{
public:
    // constructor. Using the base class constructor
    using ARAEditorView::ARAEditorView;

    /**
     * @brief returns the active model
     * @return std::shared_ptr<WebWave2Wave> 
     */
    std::shared_ptr<WebWave2Wave> getModel() { return mModel; }

    void setModel(std::shared_ptr<WebWave2Wave> model) { mModel = model; }

private:
    std::shared_ptr<WebWave2Wave> mModel {nullptr}; ///< Model for audio processing.
};
