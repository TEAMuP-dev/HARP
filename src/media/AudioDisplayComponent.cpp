#include "AudioDisplayComponent.h"

AudioDisplayComponent::AudioDisplayComponent()
    : AudioDisplayComponent("Audio Track")
{
}
AudioDisplayComponent::AudioDisplayComponent(String trackName)
    : MediaDisplayComponent(trackName)
{
    thread.startThread(Thread::Priority::normal);

    thumbnailComponent.addMouseListener(this, true);
    addAndMakeVisible(thumbnailComponent);

    thumbnail.addChangeListener(this);

    mediaHandlerInstructions =
        "Audio waveform.\nClick and drag to start playback from any point in the waveform\nVertical scroll to zoom in/out.\nHorizontal scroll to move the waveform.";

    mediaComponent.addAndMakeVisible(thumbnailComponent);
}

AudioDisplayComponent::~AudioDisplayComponent()
{
    resetTransport();

    thumbnail.removeChangeListener(this);
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

// void AudioDisplayComponent::repositionContent()
// {
//     // thumbnailComponent.setBounds(getContentBounds());
// }

void AudioDisplayComponent::loadMediaFile(const URL& filePath)
{
    const auto source = std::make_unique<URLInputSource>(filePath);

    File audioFile = filePath.getLocalFile();

    if (source == nullptr)
    {
        DBG("AudioDisplayComponent::loadMediaFile: File " << audioFile.getFullPathName()
                                                          << " does not exist.");
        // TODO - better error handing
        jassertfalse;
        return;
    }

    auto stream = rawToUniquePtr(source->createInputStream());

    if (stream == nullptr)
    {
        DBG("AudioDisplayComponent::loadMediaFile: Failed to load file "
            << audioFile.getFullPathName() << ".");
        // TODO - better error handing
        jassertfalse;
        return;
    }

    auto reader = rawToUniquePtr(formatManager.createReaderFor(std::move(stream)));

    if (reader == nullptr)
    {
        DBG("AudioDisplayComponent::loadMediaFile: Failed to read file "
            << audioFile.getFullPathName() << ".");
        // TODO - better error handing
        jassertfalse;
        return;
    }

    audioFileSource = std::make_unique<AudioFormatReaderSource>(reader.release(), true);

    // ..and plug it into our transport source
    transportSource.setSource(
        audioFileSource.get(),
        32768, // tells it to buffer this many samples ahead
        &thread, // this is the background thread to use for reading-ahead
        audioFileSource->getAudioFormatReader()->sampleRate); // allows for sample rate correction
}

void AudioDisplayComponent::addLabels(LabelList& labels)
{
    MediaDisplayComponent::addLabels(labels);

    for (const auto& l : labels)
    {
        if (auto audioLabel = dynamic_cast<AudioLabel*>(l.get()))
        {
            String lbl = l->label;
            String dsc = l->description;

            if (dsc.isEmpty())
            {
                dsc = lbl;
            }

            float dur = 0.0f;

            if ((l->duration).has_value())
            {
                dur = (l->duration).value();
            }

            Colour clr = Colours::purple.withAlpha(0.8f);

            if ((l->color).has_value())
            {
                clr = Colour((l->color).value());
            }

            String lnk = "";

            if ((l->link).has_value()) {
                lnk = (l->link).value();
            }


            if ((audioLabel->amplitude).has_value()) {
                float amp = (audioLabel->amplitude).value();

                float y = LabelOverlayComponent::amplitudeToRelativeY(amp);

                addLabelOverlay(LabelOverlayComponent((double) l->t, lbl, y, (double) dur, dsc, clr, lnk));
            } else {
                // TODO - OverheadLabelComponent((double) l->t, lbl, (double) dur, dsc, clr, lnk);
            }
        }
    }
}

void AudioDisplayComponent::resized()
{
    MediaDisplayComponent::resized();
    // Set thumbnailComponent to fill mediaComponent
    thumbnailComponent.setBounds(mediaComponent.getLocalBounds());
}

void AudioDisplayComponent::resetDisplay()
{
    MediaDisplayComponent::resetTransport();

    audioFileSource.reset();
    thumbnail.clear();
}

void AudioDisplayComponent::postLoadActions(const URL& filePath)
{
    if (auto inputSource = std::make_unique<URLInputSource>(filePath))
    {
        thumbnailCache.clear();
        thumbnail.setSource(inputSource.release());
    }
}
