/**
 * @file PopupPanel.h
 * @brief Popup shown inside a window, over a dimmed view of whatever else the window holds.
 * @author cwitkowitz
 */

#pragma once

#include <functional>
#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Palette.h"
#include "RoundIconButton.h"

#include "../utils/Interface.h"

#include "../windows/tutorial/TutorialTargets.h"

using namespace juce;

/**
 * Shows a component in a titled panel at the center of a window, and takes every click and
 * key press until it is dismissed with its close button, the Escape key, or a click beside it.
 *
 * The popup is a child of the window's content rather than a window of its own. It therefore
 * asks nothing of the window manager, under which a dialog window sometimes opens as an empty
 * sliver on Linux (see docs/DEVELOPING.md), and it follows the window as that is moved or resized.
 *
 * Whatever shows a popup owns it, and deletes it once onDismiss has been called.
 */
class PopupPanel : public Component
{
public:
    // The content is shown at the size it has when it is passed in
    PopupPanel(const String& panelTitle, std::unique_ptr<Component> contentToShow)
        : frame(panelTitle),
          content(std::move(contentToShow)),
          contentWidth(content->getWidth()),
          contentHeight(content->getHeight())
    {
        setWantsKeyboardFocus(true);
        // Keeps the Tab key within the popup
        setFocusContainerType(FocusContainerType::keyboardFocusContainer);

        frame.setComponentID(TutorialTargets::popup);
        addAndMakeVisible(frame);

        frame.addAndMakeVisible(*content);

        closeButton.onClick = [this] { dismiss(); };
        frame.addAndMakeVisible(closeButton);
    }

    // Called once the popup has been dismissed, when nothing of it is in use any longer
    std::function<void()> onDismiss;

    /**
     * Shows the popup over the window that holds a component.
     */
    void showOver(Component& anchor)
    {
        previousFocus = Component::getCurrentlyFocusedComponent();

        Component* host = anchor.getTopLevelComponent();

        // A window lays out its content component itself, and takes no other children
        if (auto* window = dynamic_cast<ResizableWindow*>(host);
            window != nullptr && window->getContentComponent() != nullptr)
        {
            host = window->getContentComponent();
        }

        host->addAndMakeVisible(this);
        setBounds(host->getLocalBounds());

        toFront(true);
    }

    void dismiss()
    {
        if (dismissed)
        {
            return;
        }

        dismissed = true;

        setVisible(false);

        if (auto* focus = previousFocus.getComponent(); focus != nullptr && focus->isShowing())
        {
            focus->grabKeyboardFocus();
        }

        /* Deferred, since this is reached from the buttons of the popup, and its owner deletes
           it in response. A button must not be destroyed from inside its own callback. */
        MessageManager::callAsync(
            [safeThis = SafePointer<PopupPanel>(this)]
            {
                if (safeThis != nullptr && safeThis->onDismiss)
                {
                    safeThis->onDismiss();
                }
            });
    }

    void paint(Graphics& g) override { g.fillAll(Colours::black.withAlpha(0.5f)); }

    void resized() override
    {
        // Never larger than the window, even if that leaves the content less room than its own size
        const auto available = getLocalBounds().reduced(windowMargin);

        const int frameWidth = jmin(available.getWidth(), contentWidth + 2 * padding);
        const int frameHeight =
            jmin(available.getHeight(), contentHeight + headerHeight + contentGap + 2 * padding);

        frame.setBounds(
            Rectangle<int>(frameWidth, frameHeight).withCentre(getLocalBounds().getCentre()));

        auto area = frame.getLocalBounds().reduced(padding);

        closeButton.setBounds(area.removeFromTop(headerHeight)
                                  .removeFromRight(closeButtonSize)
                                  .withSizeKeepingCentre(closeButtonSize, closeButtonSize));

        area.removeFromTop(contentGap);

        content->setBounds(area);
    }

    void parentSizeChanged() override
    {
        if (auto* parent = getParentComponent())
        {
            setBounds(parent->getLocalBounds());
        }
    }

    // Only reached by a click beside the frame, which takes those within it
    void mouseDown(const MouseEvent&) override { dismiss(); }

    bool keyPressed(const KeyPress& key) override
    {
        if (key == KeyPress::escapeKey)
        {
            dismiss();
        }

        // Kept from the rest of the window, where a shortcut would act behind the popup
        return true;
    }

private:
    /* The panel itself, which paints behind the content and takes the clicks that miss it, so
       that only a click beside the panel dismisses the popup */
    class Frame : public Component
    {
    public:
        explicit Frame(const String& frameTitle) : title(frameTitle) {}

        void paint(Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat().reduced(0.5f);

            g.setColour(
                getUIColourIfAvailable(LookAndFeel_V4::ColourScheme::UIColour::windowBackground));
            g.fillRoundedRectangle(bounds, cornerSize);

            g.setColour(ModelStyle::outline);
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

            // Headed like a section of a model tab, with the close button at the end of its rule
            ModelStyle::drawSectionHeader(g,
                                          title,
                                          getLocalBounds()
                                              .reduced(padding)
                                              .removeFromTop(headerHeight)
                                              .withTrimmedRight(closeButtonSize + headerGap));
        }

    private:
        const String title;
    };

    static constexpr float cornerSize = 6.0f;
    static constexpr int windowMargin = 12;
    static constexpr int padding = 12;
    static constexpr int headerHeight = ModelStyle::sectionHeaderHeight;
    static constexpr int headerGap = 6;
    static constexpr int contentGap = 6;
    static constexpr int closeButtonSize = 18;

    Frame frame;

    std::unique_ptr<Component> content;
    const int contentWidth;
    const int contentHeight;

    CloseButton closeButton { "Click to close this popup." };

    // Whatever had the keyboard before the popup took it
    SafePointer<Component> previousFocus;

    bool dismissed = false;
};
