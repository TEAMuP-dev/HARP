/**
 * @file ModelRegistry.h
 * @brief Temporary model registry accessors for model discovery.
 */

#pragma once

#include <vector>

#include <juce_core/juce_core.h>

using namespace juce;

namespace ModelRegistry
{
struct Entry
{
    String path;
    String displayName;
    String summary;
    String provider;
};

inline String getFallbackModelDisplayName(const String& modelPath)
{
    auto cleaned = modelPath.upToFirstOccurrenceOf(" [", false, false).trim();
    auto tokens = StringArray::fromTokens(cleaned, "/", "");

    if (tokens.size() > 0)
        return tokens[tokens.size() - 1].replaceCharacter('-', ' ');

    return cleaned;
}

inline std::vector<Entry> getFeaturedModels()
{
    return {
        { "stability/text-to-audio",
          "Stable Audio Text to Audio",
          "Generate music, sound effects, or soundscapes from a text prompt.",
          "Stability AI" },
        { "stability/audio-to-audio",
          "Stable Audio Audio to Audio",
          "Create variations or transfer style using text and audio conditioning.",
          "Stability AI" },
        { "teamup-tech/text2midi-symbolic-music-generation",
          "Text2Midi",
          "Generate symbolic MIDI music from a text description.",
          "Hugging Face" },
        { "teamup-tech/demucs-source-separation",
          "Demucs",
          "Split a music recording into drums, bass, vocals, and instrumental stems.",
          "Hugging Face" },
        { "teamup-tech/solo-piano-audio-to-midi-transcription",
          "High Resolution Piano Transcription",
          "Convert solo piano audio into a corresponding MIDI performance.",
          "Hugging Face" },
        { "teamup-tech/transkun",
          "Transkun",
          "Transcribe musical audio into symbolic note events.",
          "Hugging Face" },
        { "teamup-tech/TRIA",
          "TRIA",
          "Generate drum accompaniment conditioned on rhythmic input.",
          "Hugging Face" },
        { "teamup-tech/anticipatory-music-transformer",
          "Anticipatory Music Transformer",
          "Harmonize MIDI melodies by generating musically compatible notes.",
          "Hugging Face" },
        { "teamup-tech/vampnet-conditional-music-generation",
          "VampNet",
          "Generate controllable variations of an input music recording.",
          "Hugging Face" },
        { "teamup-tech/harmonic-percussive-separation",
          "Harmonic/Percussive Separation",
          "Separate audio into harmonic and percussive components.",
          "Hugging Face" },
        { "teamup-tech/Kokoro-TTS",
          "Kokoro TTS",
          "Generate speech from text using a selected voice preset.",
          "Hugging Face" },
        { "teamup-tech/MegaTTS3-Voice-Cloning",
          "MegaTTS3 Voice Cloning",
          "Generate speech from text conditioned on a reference voice recording.",
          "Hugging Face" },
        { "teamup-tech/midi-synthesizer",
          "MIDI Synthesizer",
          "Render MIDI into audio using the standard MuseScore SoundFont.",
          "Hugging Face" },
        { "teamup-tech/audioseal",
          "AudioSeal",
          "Apply or inspect audio watermarking for generated audio workflows.",
          "Hugging Face" },
    };
}

inline std::vector<std::string> getFeaturedModelPaths()
{
    std::vector<std::string> paths { "click here to enter a custom path..." };

    for (const auto& entry : getFeaturedModels())
        paths.push_back(entry.path.toStdString());

    return paths;
}

inline Entry getEntryForPath(const String& modelPath)
{
    const auto cleanedPath = modelPath.upToFirstOccurrenceOf(" [", false, false).trim();

    for (const auto& entry : getFeaturedModels())
    {
        if (entry.path == cleanedPath)
        {
            auto result = entry;
            result.path = modelPath;
            return result;
        }
    }

    return { modelPath,
             getFallbackModelDisplayName(modelPath),
             "Custom or recently used HARP-compatible model endpoint.",
             cleanedPath.startsWith("stability/") ? "Stability AI" : "Custom" };
}
} // namespace ModelRegistry
