#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "MediaDisplayComponent.h"

class AudioThumbnailWrapper : public Component
{
public:
    AudioThumbnailWrapper(AudioThumbnail& t, Range<double>& v) : thumbnail(t), visibleRange(v) {}

    void paint(Graphics& g) override
    {
        g.setColour(Colours::lightblue);

        thumbnail.drawChannels(
            g, getLocalBounds(), visibleRange.getStart(), visibleRange.getEnd(), 1.0f);
    }

private:
    AudioThumbnail& thumbnail;
    Range<double>& visibleRange;
};

class AudioDisplayComponent : public MediaDisplayComponent
{
public:
    AudioDisplayComponent();
    AudioDisplayComponent(String trackName);
    ~AudioDisplayComponent();

    static StringArray getSupportedExtensions();
    StringArray getInstanceExtensions() { return AudioDisplayComponent::getSupportedExtensions(); }

    // void repositionContent() override;

    Component* getMediaComponent() { return &thumbnailComponent; }

    void loadMediaFile(const URL& filePath) override;

    double getTotalLengthInSecs() override { return thumbnail.getTotalLength(); }
    double getTimeAtOrigin() override { return visibleRange.getStart(); }

    void addLabels(LabelList& labels) override;

    void resized() override;

private:
    void resetDisplay() override;

    void postLoadActions(const URL& filePath) override;

    TimeSliceThread thread { "Audio File Thread" };

    std::unique_ptr<AudioFormatReaderSource> audioFileSource;

    AudioThumbnailCache thumbnailCache { 5 };
    AudioThumbnail thumbnail = AudioThumbnail(512, formatManager, thumbnailCache);

    AudioThumbnailWrapper thumbnailComponent { thumbnail, visibleRange };
};
