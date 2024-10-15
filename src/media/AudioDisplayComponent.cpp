#include "AudioDisplayComponent.h"


AudioDisplayComponent::AudioDisplayComponent()
{
    thread.startThread(Thread::Priority::normal);

    formatManager.registerBasicFormats();

    deviceManager.initialise(0, 2, nullptr, true, {}, nullptr);
    deviceManager.addAudioCallback(&sourcePlayer);

    sourcePlayer.setSource(&transportSource);

    addAndMakeVisible(thumbnailComponent);

    mediaHandlerInstructions = "Audio waveform.\nClick and drag to start playback from any point in the waveform\nVertical scroll to zoom in/out.\nHorizontal scroll to move the waveform.";
}

AudioDisplayComponent::~AudioDisplayComponent()
{
    deviceManager.removeAudioCallback(&sourcePlayer);

    transportSource.setSource(nullptr);
    sourcePlayer.setSource(nullptr);
}

StringArray AudioDisplayComponent::getSupportedExtensions()
{
    StringArray extensions;

    extensions.add(".wav");
    extensions.add(".bwf");
    extensions.add(".aiff");
    extensions.add(".aif");
    extensions.add(".flac");
    extensions.add(".ogg");
    extensions.add(".mp3");

    return extensions;
}

void AudioDisplayComponent::paintMedia(Graphics& g, Rectangle<int>& a)
{
    thumbnailComponent.paintThumbnail(g, visibleRange);
    thumbnailComponent.setBounds(a);
}

void AudioDisplayComponent::loadMediaFile(const URL& filePath)
{
    const auto source = std::make_unique<URLInputSource>(filePath);

    File audioFile = filePath.getLocalFile();

    if (source == nullptr) {
        DBG("AudioDisplayComponent::loadMediaFile: File " << audioFile.getFullPathName() << " does not exist.");
        // TODO - better error handing
        jassertfalse;
        return;
    }

    auto stream = rawToUniquePtr(source->createInputStream());

    if (stream == nullptr) {
        DBG("AudioDisplayComponent::loadMediaFile: Failed to load file " << audioFile.getFullPathName() << ".");
        // TODO - better error handing
        jassertfalse;
        return;
    }

    auto reader = rawToUniquePtr(formatManager.createReaderFor(std::move(stream)));

    if (reader == nullptr) {
        DBG("AudioDisplayComponent::loadMediaFile: Failed to read file " << audioFile.getFullPathName() << ".");
        // TODO - better error handing
        jassertfalse;
        return;
    }

    audioFileSource = std::make_unique<AudioFormatReaderSource>(reader.release(), true);

    // ..and plug it into our transport source
    transportSource.setSource(audioFileSource.get(),
                                32768, // tells it to buffer this many samples ahead
                                &thread, // this is the background thread to use for reading-ahead
                                audioFileSource->getAudioFormatReader()->sampleRate); // allows for sample rate correction
}

void AudioDisplayComponent::addLabels(LabelList& labels)
{
    MediaDisplayComponent::addLabels(labels);

    for (const auto& l : labels) {
        if (auto audioLabel = dynamic_cast<AudioLabel*>(l.get())) {
            String lbl = l->label;
            String dsc = l->description;

            if (dsc.isEmpty()) {
                dsc = lbl;
            }

            float dur = 0.0f;

            if ((l->duration).has_value()) {
                dur = (l->duration).value();
            }

            if ((audioLabel->amplitude).has_value()) {
                float amp = (audioLabel->amplitude).value();

                float y = LabelOverlayComponent::amplitudeToRelativeY(amp);

                addLabelOverlay(LabelOverlayComponent((double) l->t, lbl, y, (double) dur, dsc));
            } else {
                // TODO - OverheadLabelComponent((double) l->t, lbl, (double) dur, dsc);
            }
        }
    }
}

void AudioDisplayComponent::resetDisplay()
{
    transportSource.stop();
    transportSource.setSource(nullptr);

    audioFileSource.reset();

    thumbnail.clear();
}

void AudioDisplayComponent::postLoadActions(const URL& filePath)
{
    if (auto inputSource = std::make_unique<URLInputSource>(filePath)) {
        thumbnailCache.clear();
        thumbnail.setSource(inputSource.release());
    }
}
