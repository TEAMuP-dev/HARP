#pragma once

class TrackHeader : public Component,
                    private ARARegionSequence::Listener,
                    private ARAEditorView::Listener
{
public:
    TrackHeader (ARAEditorView& editorView, ARARegionSequence& regionSequenceIn)
        : araEditorView (editorView), regionSequence (regionSequenceIn)
    {
        updateTrackName (regionSequence.getName());
        onNewSelection (araEditorView.getViewSelection());

        addAndMakeVisible (trackNameLabel);

        regionSequence.addListener (this);
        araEditorView.addListener (this);
    }

    ~TrackHeader() override
    {
        araEditorView.removeListener (this);
        regionSequence.removeListener (this);
    }

    void willUpdateRegionSequenceProperties (ARARegionSequence*, ARARegionSequence::PropertiesPtr newProperties) override
    {
        if (regionSequence.getName() != newProperties->name)
            updateTrackName (newProperties->name);
        if (regionSequence.getColor() != newProperties->color)
            repaint();
    }

    void resized() override
    {
        trackNameLabel.setBounds (getLocalBounds().reduced (2));
    }

    void paint (Graphics& g) override
    {
        const auto backgroundColour = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
        g.setColour (isSelected ? backgroundColour.brighter() : backgroundColour);
        g.fillRoundedRectangle (getLocalBounds().reduced (2).toFloat(), 6.0f);
        g.setColour (backgroundColour.contrasting());
        g.drawRoundedRectangle (getLocalBounds().reduced (2).toFloat(), 6.0f, 1.0f);

        if (auto colour = regionSequence.getColor())
        {
            g.setColour (convertARAColour (colour));
            g.fillRect (getLocalBounds().removeFromTop (16).reduced (6));
            g.fillRect (getLocalBounds().removeFromBottom (16).reduced (6));
        }
    }

    void onNewSelection (const ARAViewSelection& viewSelection) override
    {
        const auto& selectedRegionSequences = viewSelection.getRegionSequences();
        const bool selected = std::find (selectedRegionSequences.begin(), selectedRegionSequences.end(), &regionSequence) != selectedRegionSequences.end();

        if (selected != isSelected)
        {
            isSelected = selected;
            repaint();
        }
    }

private:
    void updateTrackName (ARA::ARAUtf8String optionalName)
    {
        trackNameLabel.setText (optionalName ? optionalName : "No track name",
                                NotificationType::dontSendNotification);
    }

    ARAEditorView& araEditorView;
    ARARegionSequence& regionSequence;
    Label trackNameLabel;
    bool isSelected = false;
};
