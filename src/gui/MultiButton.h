/**
 * @file MultiButton.h
 * @brief A flexible button component that supports multiple modes, hover instructions, and icons.
 * @author xribene
 * 
 * The MultiButton class extends the JUCE TextButton to provide a button that can switch between 
 * multiple modes. Each mode can have its own label, callback function, color, icon, and hover 
 * instruction message. The button can display text, an icon, or both, depending on the specified 
 * drawing mode.
 *
 * ## Usage
 *
 * ### Example: Creating a Play/Stop Button
 *
 * ```cpp
 * void initPlayStopButton()
 * {
 *     playButtonInfo = MultiButton::Mode {
 *         "Play",
 *         [this] { play(); },
 *         juce::Colours::limegreen,
 *         std::make_shared<juce::Image>(fontawesomeHelper->getIcon(
 *             fontawesome::FontAwesome_Play, 1.0f, juce::Colours::limegreen)),
 *         fontawesome::FontAwesome_Play,
 *         "Click to start playback",
 *         MultiButton::DrawingMode::IconOnly
 *     };
 *     stopButtonInfo = MultiButton::Mode {
 *         "Stop",
 *         [this] { stop(); },
 *         juce::Colours::orangered,
 *         std::make_shared<juce::Image>(fontawesomeHelper->getIcon(
 *             fontawesome::FontAwesome_Stop, 1.0f, juce::Colours::orangered)),
 *         fontawesome::FontAwesome_Stop,
 *         "Click to stop playback",
 *         MultiButton::DrawingMode::IconOnly
 *     };
 *     playStopButton.addMode(playButtonInfo);
 *     playStopButton.addMode(stopButtonInfo);
 *     playStopButton.setMode(playButtonInfo.label);
 *     addAndMakeVisible(playStopButton);
 * }
 * ```
 *
 * The MultiButton class is very flexible, allowing for multiple modes, hover-on instructions, 
 * and icons.
 */
#pragma once
#include "../external/fontaudio/src/FontAudio.h"
#include "../external/fontawesome/src/FontAwesome.h"
#include "StatusComponent.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include <string>
#include <unordered_map>
class MultiButton : public juce::TextButton
{
public:
    enum class DrawingMode
    {
        TextOnly,
        IconOnly,
        TextAndIcon
    };
    struct Mode
    {
        juce::String label;
        std::function<void()> callback;
        juce::Colour color;
        std::shared_ptr<juce::Image> icon;
        juce::String iconName;
        juce::String instructionMessage;
        DrawingMode drawingMode;
        // juce::Colour iconColor;
        // juce::String iconName;
    };

    std::function<void()> onMouseEnter;
    std::function<void()> onMouseExit;

    // MultiButton(const Mode& mode1, const Mode& mode2);

    MultiButton(const juce::String& buttonName = "MultiButton");
    // MultiButton();

    void setMode(const juce::String& modeName);
    juce::String getModeName();
    void addMode(const Mode& mode);
    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void paint(juce::Graphics& g) override;
    void paintButton(juce::Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

    void resized() override;

private:
    std::unordered_map<juce::String, Mode> modes;
    juce::String currentMode;
    // juce::Drawable* currentIcon = nullptr;
    std::shared_ptr<fontawesome::IconHelper> fontawesomeHelper;
    std::shared_ptr<fontaudio::IconHelper> fontaudioHelper;
    SharedResourcePointer<InstructionBox> instructionBox;
    DrawingMode drawingMode;
};
