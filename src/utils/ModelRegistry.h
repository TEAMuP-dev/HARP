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
    std::vector<String> tags;

    Entry() = default;
    Entry(String p, String dn, String s, String pr, std::vector<String> t = {})
        : path(std::move(p)), displayName(std::move(dn)), summary(std::move(s)), provider(std::move(pr)), tags(std::move(t))
    {}
};

inline String getCleanModelPath(const String& modelPath)
{
    auto cleaned = modelPath.trim();

    for (const auto& tag : { String(" [ERROR]"), String(" [DOWN]"), String(" [TRY AGAIN]"), String(" [SLEEPING]") })
        cleaned = cleaned.replace(tag, "");

    return cleaned.trim();
}

inline String getFallbackModelDisplayName(const String& modelPath)
{
    auto cleaned = getCleanModelPath(modelPath).upToFirstOccurrenceOf(" [", false, false).trim();
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
          "Stability AI",
          { "Generation" } },
        { "stability/audio-to-audio",
          "Stable Audio Audio to Audio",
          "Create variations or transfer style using text and audio conditioning.",
          "Stability AI",
          { "Generation", "Effects" } },
        { "teamup-tech/text2midi-symbolic-music-generation",
          "Text2Midi",
          "Generate symbolic MIDI music from a text description.",
          "Hugging Face",
          { "Generation" } },
        { "teamup-tech/demucs-source-separation",
          "Demucs",
          "Split a music recording into drums, bass, vocals, and instrumental stems.",
          "Hugging Face",
          { "Source Separation" } },
        { "teamup-tech/solo-piano-audio-to-midi-transcription",
          "High Resolution Piano Transcription",
          "Convert solo piano audio into a corresponding MIDI performance.",
          "Hugging Face",
          { "Analysis" } },
        { "teamup-tech/transkun",
          "Transkun",
          "Transcribe musical audio into symbolic note events.",
          "Hugging Face",
          { "Analysis" } },
        { "teamup-tech/TRIA",
          "TRIA",
          "Generate drum accompaniment conditioned on rhythmic input.",
          "Hugging Face",
          { "Performance Rendering and Synthesis", "Generation" } },
        { "teamup-tech/anticipatory-music-transformer",
          "Anticipatory Music Transformer",
          "Harmonize MIDI melodies by generating musically compatible notes.",
          "Hugging Face",
          { "Generation" } },
        { "teamup-tech/vampnet-conditional-music-generation",
          "VampNet",
          "Generate controllable variations of an input music recording.",
          "Hugging Face",
          { "Generation", "Effects" } },
        { "teamup-tech/harmonic-percussive-separation",
          "Harmonic/Percussive Separation",
          "Separate audio into harmonic and percussive components.",
          "Hugging Face",
          { "Source Separation" } },
        { "teamup-tech/Kokoro-TTS",
          "Kokoro TTS",
          "Generate speech from text using a selected voice preset.",
          "Hugging Face",
          { "Performance Rendering and Synthesis" } },
        { "teamup-tech/MegaTTS3-Voice-Cloning",
          "MegaTTS3 Voice Cloning",
          "Generate speech from text conditioned on a reference voice recording.",
          "Hugging Face",
          { "Performance Rendering and Synthesis" } },
        { "teamup-tech/midi-synthesizer",
          "MIDI Synthesizer",
          "Render MIDI into audio using the standard MuseScore SoundFont.",
          "Hugging Face",
          { "Performance Rendering and Synthesis" } },
        { "teamup-tech/audioseal",
          "AudioSeal",
          "Apply or inspect audio watermarking for generated audio workflows.",
          "Hugging Face",
          { "Analysis", "Production" } },
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
    const auto cleanedPath = getCleanModelPath(modelPath).upToFirstOccurrenceOf(" [", false, false).trim();

    for (const auto& entry : getFeaturedModels())
    {
        if (entry.path == cleanedPath)
        {
            auto result = entry;
            result.path = cleanedPath;
            return result;
        }
    }

    return { cleanedPath,
             getFallbackModelDisplayName(modelPath),
             "Custom or recently used HARP-compatible model endpoint.",
             cleanedPath.startsWith("stability/") ? "Stability AI" : "Custom" };
}
} // namespace ModelRegistry
