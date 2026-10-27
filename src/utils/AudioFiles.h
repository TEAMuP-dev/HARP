/**
 * @file AudioFiles.h
 * @brief The audio formats HARP reads, and conversion between the ones it writes.
 * @author cwitkowitz
 */

#pragma once

#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>

#include "Logging.h"

using namespace juce;

namespace AudioFiles
{
/* The formats that every platform reads and that every endpoint can be expected to decode */
inline const StringArray& portableExtensions()
{
    static const StringArray extensions {
        ".wav", ".bwf", ".aiff", ".aif", ".flac", ".ogg", ".mp3"
    };

    return extensions;
}

/* The formats that the decoders of this platform add, which JUCE reads through Core Audio on
   macOS and through Windows Media on Windows. A Space has only the decoders of its own
   libraries, so a file in one of these is converted before it is uploaded (see forUpload). */
inline const StringArray& platformExtensions()
{
    static const StringArray extensions {
#if JUCE_MAC
        ".m4a", ".mp4", ".aac", ".caf"
#elif JUCE_WINDOWS
        ".wma"
#endif
    };

    return extensions;
}

/* Rewrites an audio file in the format the extension of the target names (WAV, AIFF, or
   FLAC), at the sample rate and channel count of the source and 16 bits per sample. */
inline bool convert(const File& source, const File& target)
{
    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<AudioFormatReader> reader(formatManager.createReaderFor(source));

    if (! reader)
    {
        DBG_AND_LOG("AudioFiles::convert: Could not read source file \"" << source.getFullPathName()
                                                                         << "\".");

        return false;
    }

    String ext = target.getFileExtension().toLowerCase();
    AudioFormat* format = nullptr;

    if (ext == ".wav")
        format = formatManager.findFormatForFileExtension("wav");
    else if (ext == ".aiff" || ext == ".aif")
        format = formatManager.findFormatForFileExtension("aiff");
    else if (ext == ".flac")
        format = formatManager.findFormatForFileExtension("flac");
    else
    {
        DBG_AND_LOG("AudioFiles::convert: Unsupported target format \"" << ext << "\".");

        return false;
    }

    target.deleteFile();

    std::unique_ptr<OutputStream> outputStream = target.createOutputStream();

    if (! outputStream)
    {
        DBG_AND_LOG("AudioFiles::convert: Could not create output file \""
                    << target.getFullPathName() << "\".");

        return false;
    }

    // Takes ownership of the stream only if it succeeds
    std::unique_ptr<AudioFormatWriter> writer =
        format->createWriterFor(outputStream,
                                AudioFormatWriterOptions {}
                                    .withSampleRate(reader->sampleRate)
                                    .withNumChannels((int) reader->numChannels)
                                    .withBitsPerSample(16));

    if (! writer)
    {
        DBG_AND_LOG("AudioFiles::convert: Could not create writer for \""
                    << target.getFullPathName() << "\".");

        return false;
    }

    const int blockSize = 4096;
    AudioBuffer<float> buffer((int) reader->numChannels, blockSize);
    int64 totalFrames = (int64) reader->lengthInSamples;
    int64 position = 0;

    while (position < totalFrames)
    {
        int64 framesToRead = jmin((int64) blockSize, totalFrames - position);
        reader->read(&buffer, 0, (int) framesToRead, position, true, true);
        writer->writeFromAudioSampleBuffer(buffer, 0, (int) framesToRead);
        position += framesToRead;
    }

    DBG_AND_LOG("AudioFiles::convert: Converted and saved \""
                << source.getFullPathName() << "\" to \"" << target.getFullPathName() << "\".");

    return true;
}

/* The file to upload for an audio input: the file itself when every endpoint decodes its
   format, and otherwise a WAV written among the temporary files. The file itself is returned
   when the conversion fails, so that the endpoint reports what it cannot read. */
inline File forUpload(const File& file)
{
    if (! platformExtensions().contains(file.getFileExtension().toLowerCase()))
        return file;

    const File converted = File::getSpecialLocation(File::tempDirectory)
                               .getChildFile(file.getFileNameWithoutExtension() + ".wav")
                               .getNonexistentSibling();

    return convert(file, converted) ? converted : file;
}
} // namespace AudioFiles
