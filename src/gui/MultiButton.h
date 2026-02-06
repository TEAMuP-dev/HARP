/**
 * @file MultiButton.h
 * @brief Flexible button component that supports multiple modes, hover instructions, and icons.
 * @author xribene
 * 
 * The MultiButton class extends the JUCE TextButton to provide a button that can switch between 
 * multiple modes. Each mode can have its own label, callback function, color, icon, and hover 
 * message. The button can display text or an icon, depending on the specified drawing mode.
 *
 * ### Example: Creating a Play/Stop Button
 *
 * ```cpp
 * void initializePlayStopButton()
 * {
 *     playButtonInfo = MultiButton::Mode {
 *         "Play",
 *         "Click to start playback.",
 *         [this] { play(); },
 *         MultiButton::DrawingMode::IconOnly,
 *         Colours::limegreen,
 *         fontawesome::Play
 *     };
 *     stopButtonInfo = MultiButton::Mode {
 *         "Stop",
 *         "Click to stop playback.",
 *         [this] { stop(); },
 *         MultiButton::DrawingMode::IconOnly,
 *         Colours::orangered,
 *         fontawesome::FontAwesome_Stop
 *     };
 *     playStopButton.addMode(playButtonInfo);
 *     playStopButton.addMode(stopButtonInfo);
 *     playStopButton.setMode(playButtonInfo.label);
 *     addAndMakeVisible(playStopButton);
 * }
 * ```
 */

#pragma once

#include <string>
#include <unordered_map>

#include <juce_gui_basics/juce_gui_basics.h>

#include "../external/fontaudio/src/FontAudio.h"
#include "../external/fontawesome/src/FontAwesome.h"

#include "../widgets/StatusAreaWidget.h"

#include "../utils/Logging.h"

class MultiButton : public TextButton
{
public:
    enum class DrawingMode
    {
        TextOnly,
        IconOnly
    };

    enum class IconType
    {
        None,
        FontAwesome,
        FontAudio
    };

    struct Mode
    {
        String displayLabel;
        String instructions;
        std::function<void()> callback;
        DrawingMode drawingMode;

        Colour iconColor;
        IconType iconType = IconType::None;

        fontawesome::IconName awesomeIcon {};
        fontaudio::IconName audioIcon {};

        // Full text-only constructor
        Mode(const String& lbl, const String& ins, std::function<void()> cb, DrawingMode dm)
            : displayLabel(lbl), instructions(ins), callback(cb), drawingMode(dm)
        {
        }

        // Full template icon constructor (FontAwesome or FontAudio)
        template <typename IconT>
        Mode(const String& lbl,
             const String& ins,
             std::function<void()> cb,
             DrawingMode dm,
             Colour clr,
             IconT icon)
            : Mode(lbl, ins, cb, dm)
        {
            iconColor = clr;

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

        // Default constructor for empty button
        Mode() : Mode("", "", [] {}, DrawingMode::TextOnly) {}
    };

    MultiButton(const String& buttonName = "");

    void paintButton(Graphics& g,
                     bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;

    void resized() override;

    String getModeName() { return currentModeKey; }

    int getIconSize()
    {
        // Only square icons are supported
        return jmin(getWidth(), getHeight());
    }

    void addMode(const Mode& mode);
    void setMode(const String& modeKey);

    void mouseEnter(const MouseEvent& event) override;
    void mouseExit(const MouseEvent& event) override;

    // Optional callbacks
    std::function<void()> onMouseEnter;
    std::function<void()> onMouseExit;

private:
    void updateInstructions();

    std::shared_ptr<fontawesome::IconHelper> fontawesomeHelper;
    std::shared_ptr<fontaudio::IconHelper> fontaudioHelper;

    std::unordered_map<String, Mode> modes;

    String currentModeKey;

    SharedResourcePointer<InstructionsMessage> instructionsMessage;
};
