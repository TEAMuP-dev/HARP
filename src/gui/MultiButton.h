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
 *         "Click to start playback",
 *         MultiButton::DrawingMode::IconOnly,
 *         fontawesome::Play,
 *     };
 *     stopButtonInfo = MultiButton::Mode {
 *         "Stop",
 *         [this] { stop(); },
 *         juce::Colours::orangered,
 *         "Click to stop playback",
 *         MultiButton::DrawingMode::IconOnly
 *         fontawesome::FontAwesome_Stop,
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

    enum class IconType
    {
        None,
        FontAwesome,
        FontAudio
    };
    struct Mode
    {
        juce::String label;
        std::function<void()> callback;
        juce::Colour color;
        // std::shared_ptr<juce::Image> icon;
        // juce::String iconName;
        juce::String instructionMessage;
        DrawingMode drawingMode;
        // juce::Colour iconColor;
        // juce::String iconName;

        IconType iconType = IconType::None;
        fontawesome::IconName awesomeIcon {}; // For FontAwesome
        fontaudio::IconName audioIcon {};  

        // 1) Constructor for text-only
        Mode(const juce::String& lbl,
             std::function<void()> cb,
             juce::Colour col,
             const juce::String& instr,
             DrawingMode dm)
            : label(lbl),
              callback(cb),
              color(col),
              instructionMessage(instr),
              drawingMode(dm),
              iconType(IconType::None)
        {
        } 

        // 2) Template constructor for an icon (FontAwesome or FontAudio)
        template <typename IconT>
        Mode(const juce::String& lbl,
             std::function<void()> cb,
             juce::Colour col,
             const juce::String& instr,
             DrawingMode dm,
             IconT icon)
            : label(lbl),
              callback(cb),
              color(col),
              instructionMessage(instr),
              drawingMode(dm)
        {
            if constexpr (std::is_same_v<IconT, fontawesome::IconName>)
            {
                iconType = IconType::FontAwesome;
                awesomeIcon = icon;
            }
            else if constexpr (std::is_same_v<IconT, fontaudio::IconName>)
            {
                iconType = IconType::FontAudio;
                audioIcon = icon;
            }
        }

        Mode()
        : drawingMode(DrawingMode::TextOnly),
          iconType(IconType::None)
        {
        }
    };

    std::function<void()> onMouseEnter;
    std::function<void()> onMouseExit;

    // MultiButton(const Mode& mode1, const Mode& mode2);

    MultiButton(const juce::String& buttonName = "MultiButton");
    // MultiButton();

    void setMode(const juce::String& modeName);
    juce::String getModeName();
    void addMode(const Mode& mode);

    // template <class IconT>
    // void addMode(const juce::String& modeLabel,
    //             std::function<void()> callback,
    //             juce::Colour color,
    //             IconT icon,
    //             const juce::String& instruction,
    //             DrawingMode drawingMode);

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
