#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "MediaDisplayComponent.h"


class AudioThumbnailWrapper : public Component
{
public:

    AudioThumbnailWrapper(juce::AudioThumbnail& t) : thumbnail(t) {}

    void paintThumbnail(juce::Graphics& g, Range<double> visibleRange)
    {
        if (thumbnail.getTotalLength() > 0.0)
        {
            thumbnail.drawChannels(g, getLocalBounds(), visibleRange.getStart(), visibleRange.getEnd(), 1.0f);
        }
    }

private:

    AudioThumbnail& thumbnail;
};


class AudioDisplayComponent : public MediaDisplayComponent
{
public:

    AudioDisplayComponent();
    ~AudioDisplayComponent();

    static StringArray getSupportedExtensions();
    StringArray getInstanceExtensions() { return AudioDisplayComponent::getSupportedExtensions(); }

    void paintMedia(Graphics& g, Rectangle<int>& a) override;

    Component* getMediaComponent() { return &thumbnailComponent; }

    void loadMediaFile(const URL& filePath) override;

    void setPlaybackPosition(double t) override { transportSource.setPosition(t); }
    double getPlaybackPosition() override { return transportSource.getCurrentPosition(); }

    bool isPlaying() override { return transportSource.isPlaying(); }
    void startPlaying() override { transportSource.start(); }
    void stopPlaying() override { transportSource.stop(); }

    double getTotalLengthInSecs() override { return thumbnail.getTotalLength(); }

    void addLabels(LabelList& labels) override;

private:

    void resetDisplay() override;

    void postLoadActions(const URL& filePath) override;

    TimeSliceThread thread{ "Audio File Thread" };
    
    AudioFormatManager formatManager;
    AudioDeviceManager deviceManager;

    std::unique_ptr<AudioFormatReaderSource> audioFileSource;

    AudioSourcePlayer sourcePlayer;
    AudioTransportSource transportSource;

    AudioThumbnailCache thumbnailCache{ 5 };
    AudioThumbnail thumbnail = AudioThumbnail(512, formatManager, thumbnailCache);

    AudioThumbnailWrapper thumbnailComponent{ thumbnail };
};
