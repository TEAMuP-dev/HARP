// -----------------------------------------------------------------------------
// Test rationale
// -----------------------------------------------------------------------------
// This file verifies deterministic, HARP-owned state-management behavior in
// Settings.h without reading from or writing to real user settings.
//
// The SettingsTest fixture creates an isolated temporary PropertiesFile so each
// test can verify settings behavior without affecting application state outside
// the test process.
//
// Category sections are placed near the tests they describe so the rationale for
// each group remains close to the behavior being verified.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include "../../src/utils/Settings.h"

using namespace juce;

namespace
{
// Provides an isolated settings store for tests that need initialized Settings.
class SettingsTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        testDirectory = File::getSpecialLocation(File::tempDirectory)
                            .getChildFile("HARP_Settings_Tests_" + Uuid().toString());
        ASSERT_TRUE(testDirectory.createDirectory());

        PropertiesFile::Options options;
        options.applicationName = "HARPSettingsTest";
        options.filenameSuffix = "settings";
        options.folderName = testDirectory.getFullPathName();
        options.osxLibrarySubFolder = "Application Support";
        options.storageFormat = PropertiesFile::storeAsXML;

        appProperties.setStorageParameters(options);
        Settings::initialize(&appProperties);
    }

    void TearDown() override
    {
        Settings::initialize(nullptr);
        appProperties.closeFiles();
        testDirectory.deleteRecursively();
    }

    File testDirectory;
    ApplicationProperties appProperties;
};
} // namespace

// -----------------------------------------------------------------------------
// Category: Null-safe uninitialized behavior
// -----------------------------------------------------------------------------
// These tests verify that Settings operations remain safe before initialization.

// Verifies that uninitialized Settings calls return defaults and do not crash.
TEST(SettingsUninitializedTest, OperationsAreNullSafeAndReturnDefaults)
{
    Settings::initialize(nullptr);

    EXPECT_EQ(Settings::getUserSettings(), nullptr);
    EXPECT_FALSE(Settings::containsKey("missing"));
    EXPECT_EQ(Settings::getString("missing", "fallback"), "fallback");
    EXPECT_EQ(Settings::getIntValue("missing", 17), 17);
    EXPECT_DOUBLE_EQ(Settings::getDoubleValue("missing", 3.5), 3.5);
    EXPECT_TRUE(Settings::getBoolValue("missing", true));

    Settings::setValue("key", "value");
    Settings::setValue("flag", true);
    Settings::removeValue("key");
    Settings::saveIfNeeded();

    EXPECT_FALSE(Settings::containsKey("key"));
}

// -----------------------------------------------------------------------------
// Category: Initialization behavior
// -----------------------------------------------------------------------------
// These tests verify that Settings correctly binds to the active properties store.

// Verifies that initialization provides access to a user settings file.
TEST_F(SettingsTest, InitializeProvidesUserSettings)
{
    EXPECT_NE(Settings::getUserSettings(), nullptr);
}

// Verifies that reinitializing Settings switches the active backing store.
TEST_F(SettingsTest, ReinitializationSwitchesSettingsStore)
{
    Settings::setValue("key", String("first"));
    EXPECT_EQ(Settings::getString("key"), "first");

    File secondDirectory = File::getSpecialLocation(File::tempDirectory)
                               .getChildFile("HARP_Settings_Tests_Second_" + Uuid().toString());
    ASSERT_TRUE(secondDirectory.createDirectory());

    ApplicationProperties secondProperties;
    PropertiesFile::Options options;
    options.applicationName = "HARPSettingsTestSecond";
    options.filenameSuffix = "settings";
    options.folderName = secondDirectory.getFullPathName();
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = PropertiesFile::storeAsXML;
    secondProperties.setStorageParameters(options);

    Settings::initialize(&secondProperties);

    EXPECT_FALSE(Settings::containsKey("key"));
    EXPECT_EQ(Settings::getString("key", "default"), "default");

    secondProperties.closeFiles();
    secondDirectory.deleteRecursively();
}

// -----------------------------------------------------------------------------
// Category: String setting lifecycle
// -----------------------------------------------------------------------------
// These tests verify string storage, retrieval, replacement, and removal.

// Verifies that string settings remain consistent across set, get, update, and remove.
TEST_F(SettingsTest, StringSettingLifecycleIsConsistent)
{
    EXPECT_FALSE(Settings::containsKey("prompt"));
    EXPECT_EQ(Settings::getString("prompt", "default"), "default");

    Settings::setValue("prompt", String("hello"));

    EXPECT_TRUE(Settings::containsKey("prompt"));
    EXPECT_EQ(Settings::getString("prompt", "default"), "hello");

    Settings::setValue("prompt", String("updated"));
    EXPECT_EQ(Settings::getString("prompt", "default"), "updated");

    Settings::removeValue("prompt");
    EXPECT_FALSE(Settings::containsKey("prompt"));
    EXPECT_EQ(Settings::getString("prompt", "default"), "default");
}

// Verifies that empty strings are treated as stored values rather than missing keys.
TEST_F(SettingsTest, EmptyStringIsStoredAndDiscoverable)
{
    Settings::setValue("empty", String(""));

    EXPECT_TRUE(Settings::containsKey("empty"));
    EXPECT_EQ(Settings::getString("empty", "default"), "");
}

// Verifies that Unicode strings round-trip through the settings store.
TEST_F(SettingsTest, UnicodeStringRoundTrips)
{
    const String value = CharPointer_UTF8("こんにちは 🎵");

    Settings::setValue("unicode", value);

    EXPECT_TRUE(Settings::containsKey("unicode"));
    EXPECT_EQ(Settings::getString("unicode"), value);
}

// -----------------------------------------------------------------------------
// Category: Boolean setting representation
// -----------------------------------------------------------------------------
// These tests verify HARP's explicit string representation for boolean settings.

// Verifies that boolean settings are stored as the strings expected by Settings.h.
TEST_F(SettingsTest, BooleanSettingLifecycleStoresStringRepresentation)
{
    Settings::setValue("enabled", true);

    EXPECT_TRUE(Settings::containsKey("enabled"));
    EXPECT_EQ(Settings::getString("enabled"), "true");

    Settings::setValue("enabled", false);

    EXPECT_EQ(Settings::getString("enabled"), "false");
}

// -----------------------------------------------------------------------------
// Category: Numeric setting retrieval
// -----------------------------------------------------------------------------
// These tests verify default and stored-value behavior for numeric settings.

// Verifies that integer settings return defaults when missing and stored values when present.
TEST_F(SettingsTest, IntegerValuesReturnStoredValueOrDefault)
{
    EXPECT_EQ(Settings::getIntValue("count", 9), 9);

    Settings::setValue("count", 42);
    EXPECT_EQ(Settings::getIntValue("count", 9), 42);

    Settings::setValue("count", -7);
    EXPECT_EQ(Settings::getIntValue("count", 9), -7);
}

// Verifies that double settings return defaults when missing and stored values when present.
TEST_F(SettingsTest, DoubleValuesReturnStoredValueOrDefault)
{
    EXPECT_DOUBLE_EQ(Settings::getDoubleValue("amount", 1.25), 1.25);

    Settings::setValue("amount", 2.5);
    EXPECT_DOUBLE_EQ(Settings::getDoubleValue("amount", 1.25), 2.5);

    Settings::setValue("amount", -3.75);
    EXPECT_DOUBLE_EQ(Settings::getDoubleValue("amount", 1.25), -3.75);
}

// -----------------------------------------------------------------------------
// Category: Removal behavior
// -----------------------------------------------------------------------------
// These tests verify that remove operations are safe and isolated.

// Verifies that removing a missing key does not disturb existing settings.
TEST_F(SettingsTest, RemovingMissingKeyIsNoOp)
{
    Settings::setValue("present", String("value"));

    Settings::removeValue("missing");

    EXPECT_TRUE(Settings::containsKey("present"));
    EXPECT_EQ(Settings::getString("present"), "value");
}
