/**
 * @file
 *
 * @brief Implementation of the ARA Editor View.
 * @author xribene
 */

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
                std::any defaultValue;
                currentCtrlDict["widget_type"] = widgetType;
                currentCtrlDict["widget_type_val"] = widgetTypeVal;

                if (ctrlType == "ContinuousCtrl") {
                    defaultValue = ctrl.getAttr("default").toDouble();
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
                    defaultValue = ctrl.getAttr("default").toBool();
                    // Store the attributes specific to BinaryCtrl
                    currentCtrlDict["default"] = defaultValue;
                }
                else if (ctrlType == "TextInputCtrl") {
                    defaultValue = ctrl.getAttr("default").toStringRef();

                    // Store the attributes specific to TextInputCtrl
                    currentCtrlDict["default"] = defaultValue;
                    
                }
                else if (ctrlType == "FloatInputCtrl") {
                    defaultValue = ctrl.getAttr("default").toDouble();
                    // Store the attributes specific to FloatInputCtrl
                    currentCtrlDict["default"] = defaultValue;
                }
                else if (ctrlType == "OptionCtrl") {
                    auto options = ctrl.getAttr("options").toListRef();
                    auto defaultOption = ctrl.getAttr("default").toStringRef();
                    // Store the attributes specific to OptionCtrl
                    std::vector<std::tuple<std::string, int>> optionStrings;
                    int counter = 1;
                    for (auto &option : options) {
                        std::string tmp = option.toStringRef();
                        optionStrings.push_back(std::make_tuple(tmp, counter));
                        if (tmp == defaultOption) {
                            defaultValue = counter;
                        }
                        counter++;
                    }
                    currentCtrlDict["options"] = optionStrings;
                    currentCtrlDict["default"] = defaultValue;
                }
                currentCtrlDict["current"] = defaultValue;
                modelGuiAttributes.push_back(currentCtrlDict);
            }
        }
    }
    modelLoaded = true;
}

void EditorView::triggerRepaint() {
    sendChangeMessage();
}
// Model Card getter
ModelCard EditorView::getModelCard() const {
    return modelCard;
}

// Neural Model Attributes getter
ListOfDicts EditorView::getModelGuiAttributes() const {
    return modelGuiAttributes;
}

void EditorView::setCurrentCtrlValue(std::string nameId, std::any value) {
    for (auto &ctrl : modelGuiAttributes) {
        auto nameIdValue = std::any_cast<std::string>(ctrl["nameId"]);
        if (nameIdValue == nameId) {
            auto ctrl_type = std::any_cast<std::string>(ctrl["ctrl_type"]);
            if (ctrl_type == "ContinuousCtrl")
                ctrl["current"] = std::any_cast<double>(value);
            else if (ctrl_type == "BinaryCtrl")
                ctrl["current"] = std::any_cast<bool>(value);
            else if (ctrl_type == "TextInputCtrl")
                ctrl["current"] = std::any_cast<std::string>(value);
            else if (ctrl_type == "FloatInputCtrl")
                ctrl["current"] = std::any_cast<double>(value);
            else if (ctrl_type == "OptionCtrl")
                ctrl["current"] = std::any_cast<int>(value);
        }
    }
}