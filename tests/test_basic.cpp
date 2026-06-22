#include <gtest/gtest.h>
#include "../src/media/pianoroll/AudioDisplayComponent.h"

TEST(AudioDisplayComponentTest, SupportedExtensionsAreCorrect)
{
    StringArray exts = AudioDisplayComponent::getSupportedExtensions();

    // Expected extensions
    StringArray expected {
        ".wav", ".bwf", ".aiff", ".aif", ".flac", ".ogg", ".mp3"
    };

    // Same number of extensions
    EXPECT_EQ(exts.size(), expected.size());

    // Check that each expected extension is present
    for (auto& ext : expected)
        EXPECT_TRUE(exts.contains(ext));
}
