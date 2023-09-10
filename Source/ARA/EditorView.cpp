#include "EditorView.h"

// EditorView::EditorView(ARA::PlugIn::DocumentController *documentController)
//     : ARAEditorView(documentController) {   
//     }
// EditorView::~EditorRenderer() {

// }
// This callback gets called once a model is loaded
// It is used to get the info from the model's attributes (modelCard and GUI info)
void EditorView::changeListenerCallback(ChangeBroadcaster *source) {

    // the broadcaster source is of type TorchModel (TorchWave2Wave)
    auto tm = dynamic_cast<TorchModel *>(source);
    // clear the modelGuiAttributes vector before filling it
    modelGuiAttributes.clear();
    for (const auto &attr : tm->m_model->named_attributes()) {

        if (attr.name == "model_card") {
            // populate the model card
            // auto pycard = m_model->attr("model_card").toObject();
            auto pycard = attr.value.toObject();

            modelCard.name = pycard->getAttr("name").toStringRef();
            modelCard.description = pycard->getAttr("description").toStringRef();
            modelCard.author = pycard->getAttr("author").toStringRef();
            modelCard.sampleRate = pycard->getAttr("sample_rate").toInt();
            // empty the modelCard.tags vector before filling it
            modelCard.tags.clear();
            for (const auto &tag : pycard->getAttr("tags").toListRef()) {
                modelCard.tags.push_back(tag.toStringRef());
            }
        }
        else if (attr.name == "ctrls") {
            // Make sure that the attribute is a dictionary, if not print a message
            if (!attr.value.isGenericDict()) {
                DBG("ctrls is not a dictionary");
                jassertfalse;
            }
            auto ctrls = attr.value.toGenericDict();
            for (auto it = ctrls.begin(); it != ctrls.end(); it++) {
                GenericDict currentCtrlDict;
                auto nameId = it->key().toStringRef();
                auto ctrl = it->value().toObjectRef();
                auto name = ctrl.getAttr("name").toStringRef();
                std::string ctrlType = ctrl.getAttr("ctrl_type").toStringRef();

                // Store the attributes in the C++ map
                currentCtrlDict["nameId"] = nameId;
                currentCtrlDict["name"] = name;
                currentCtrlDict["ctrl_type"] = ctrlType;

                auto widget = ctrl.getAttr("widget").toEnumHolder().get();
                auto widgetType = widget->name();
                auto widgetTypeVal = widget->value().toInt();

                currentCtrlDict["widget_type"] = widgetType;
                currentCtrlDict["widget_type_val"] = widgetTypeVal;

                if (ctrlType == "ContinuousCtrl") {
                    auto defaultValue = ctrl.getAttr("default").toDouble();
                    auto min = ctrl.getAttr("min").toDouble();
                    auto max = ctrl.getAttr("max").toDouble();
                    auto step = ctrl.getAttr("step").toDouble();

                    // Store the attributes specific to ContinuousCtrl
                    currentCtrlDict["default"] = defaultValue;
                    currentCtrlDict["min"] = min;
                    currentCtrlDict["max"] = max;
                    currentCtrlDict["step"] = step;

                }
                else if (ctrlType == "BinaryCtrl") {
                    auto defaultValue = ctrl.getAttr("default").toBool();
                    // Store the attributes specific to BinaryCtrl
                    currentCtrlDict["default"] = defaultValue;
                }
                else if (ctrlType == "TextInputCtrl") {
                    auto defaultValue = ctrl.getAttr("default").toStringRef();

                    // Store the attributes specific to TextInputCtrl
                    currentCtrlDict["default"] = defaultValue;
                }
                else if (ctrlType == "FloatInputCtrl") {
                    auto defaultValue = ctrl.getAttr("default").toDouble();
                    // Store the attributes specific to FloatInputCtrl
                    currentCtrlDict["default"] = defaultValue;
                }
                else if (ctrlType == "OptionCtrl") {
                    auto options = ctrl.getAttr("options").toListRef();

                    // Store the attributes specific to OptionCtrl
                    std::vector<std::string> optionStrings;
                    for (auto &option : options) {
                        optionStrings.push_back(option.toStringRef());
                    }
                    currentCtrlDict["options"] = optionStrings;
                }
                modelGuiAttributes.push_back(currentCtrlDict);
            }
        }
    }
    modelLoaded = true;
}

// // Model card listener method
// void EditorView::setModelCard(const ModelCard& card){
//     modelCard = card;
//     DBG("Eimai Mesa se EditorView::modelCardLoaded");
// }

// Model Card getter
ModelCard EditorView::getModelCard() const {
    return modelCard;
}

// Neural Model Attributes getter
ListOfDicts EditorView::getModelGuiAttributes() const {
    return modelGuiAttributes;
}