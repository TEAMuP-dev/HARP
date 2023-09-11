/**
 * @file
 *
 * @brief Implementation of the ARA Editor View.
 * @author xribene
 */

#pragma once
#include <torch/script.h>
#include <torch/torch.h>
#include "juce_gui_basics/juce_gui_basics.h"

#include <ARA_Library/PlugIn/ARAPlug.h>
#include <ARA_Library/Utilities/ARAPitchInterpretation.h>
#include <ARA_Library/Utilities/ARATimelineConversion.h>
#include "../DeepLearning/TorchModel.h" // needed for ModelCardListener
#include "../UI/CustomComponents.h"


// #include "../UI/ModelCard.h"
using namespace juce;
using GenericDict = std::map<std::string, std::any>;
using ListOfDicts = std::vector<GenericDict>;
/**
 * @class EditorRenderer
 * @brief TODO: Write brief class description.
 */
class EditorView : public juce::ARAEditorView,
                    public ChangeListener
                    {
public:
    // EditorView(ARA::PlugIn::DocumentController *documentController);
    // ~EditorView() override;
    // constructor. Using the base class constructor
    using ARAEditorView::ARAEditorView;

    // setModelGuiAttributes
    void changeListenerCallback(ChangeBroadcaster *source) override;

    // Model Card getter
    ModelCard getModelCard() const;

    // Neural Model Attributes getter
    ListOfDicts getModelGuiAttributes() const;

private:
    // std::shared_ptr<ModelCard> modelCard;
    ModelCard modelCard;
    bool modelLoaded = false;
    ListOfDicts modelGuiAttributes;
};
