// -----------------------------------------------------------------------------
// Test rationale
// -----------------------------------------------------------------------------
// This file verifies deterministic, HARP-owned behavior in Client.h without
// exercising external model APIs or network communication.
//
// The SettingsBackedTest fixture creates an isolated temporary PropertiesFile
// so tests can verify API key persistence without touching real user settings.
//
// The MinimalClient class provides the smallest concrete Client implementation
// needed to instantiate and exercise Client's base behavior. Its overrides are
// intentionally simple because these tests target Client.h behavior, not a
// provider-specific subclass.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include "../../src/clients/Client.h"

using namespace juce;

namespace
{
// Provides an isolated settings store for tests that need to verify persistent
// API key behavior without reading or writing real application settings.
class SettingsBackedTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        testDirectory = File::getSpecialLocation(File::tempDirectory)
                            .getChildFile("HARP_Client_Tests_" + Uuid().toString());
        ASSERT_TRUE(testDirectory.createDirectory());

        PropertiesFile::Options options;
        options.applicationName = "HARPClientTest";
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

// Minimal concrete Client used to test base-class behavior without invoking a
// provider-specific implementation or external communication.
class MinimalClient : public Client
{
public:
    MinimalClient() { provider = Provider::HuggingFace; }

    String inferHostSlashModel(String modelPath) override { return modelPath; }
    String inferEndpointPath(String modelPath) override { return modelPath; }
    String inferDocumentationPath(String modelPath) override { return modelPath; }
    OpResult queryControls(String, DynamicObject::Ptr&) override { return OpResult::ok(); }
    var wrapPayloadElement(var payloadElement, bool = false, String = "") override { return payloadElement; }
    OpResult process(String, String&, std::vector<File>&, LabelList&) override { return OpResult::ok(); }
};
} // namespace

// Verifies the stable contract between provider enums and persistent settings keys.
TEST_F(SettingsBackedTest, ProviderToSettingsKeyDefinesStableProviderContracts)
{
    SharedAPIKeys keys;

    EXPECT_EQ(keys.providerToSettingsKey(Provider::HuggingFace), "apikeys.HuggingFace");
    EXPECT_EQ(keys.providerToSettingsKey(Provider::Stability), "apikeys.Stability");
}

// Verifies that persisted provider keys are restored into the in-memory token map.
TEST_F(SettingsBackedTest, InitializeAPIKeysRestoresPersistedKeys)
{
    Settings::setValue("apikeys.HuggingFace", String("hf-token"));
    Settings::setValue("apikeys.Stability", String("stability-token"));

    SharedAPIKeys keys;
    keys.initializeAPIKeys();

    ASSERT_TRUE(keys.savedTokens.contains(Provider::HuggingFace));
    ASSERT_TRUE(keys.savedTokens.contains(Provider::Stability));
    EXPECT_EQ(keys.savedTokens[Provider::HuggingFace], "hf-token");
    EXPECT_EQ(keys.savedTokens[Provider::Stability], "stability-token");
}

// Verifies that absent or empty persisted keys do not create usable credentials.
TEST_F(SettingsBackedTest, InitializeAPIKeysIgnoresMissingAndEmptyKeys)
{
    Settings::setValue("apikeys.HuggingFace", String(""));

    SharedAPIKeys keys;
    keys.initializeAPIKeys();

    EXPECT_FALSE(keys.savedTokens.contains(Provider::HuggingFace));
    EXPECT_FALSE(keys.savedTokens.contains(Provider::Stability));
}

// Verifies that updating a provider key changes both memory and persistent storage.
TEST_F(SettingsBackedTest, UpdateKeyUpdatesMemoryAndPersistentSettings)
{
    SharedAPIKeys keys;

    keys.updateKey(Provider::HuggingFace, "new-token");

    ASSERT_TRUE(keys.savedTokens.contains(Provider::HuggingFace));
    EXPECT_EQ(keys.savedTokens[Provider::HuggingFace], "new-token");
    EXPECT_EQ(Settings::getString("apikeys.HuggingFace"), "new-token");
}

// Verifies that updating one provider does not corrupt unrelated provider keys.
TEST_F(SettingsBackedTest, UpdateKeyOnlyChangesSelectedProvider)
{
    SharedAPIKeys keys;
    keys.updateKey(Provider::HuggingFace, "hf-token");
    keys.updateKey(Provider::Stability, "stability-token");

    keys.updateKey(Provider::HuggingFace, "updated-hf-token");

    EXPECT_EQ(keys.savedTokens[Provider::HuggingFace], "updated-hf-token");
    EXPECT_EQ(keys.savedTokens[Provider::Stability], "stability-token");
    EXPECT_EQ(Settings::getString("apikeys.HuggingFace"), "updated-hf-token");
    EXPECT_EQ(Settings::getString("apikeys.Stability"), "stability-token");
}

// Verifies that removing one provider key leaves unrelated provider keys intact.
TEST_F(SettingsBackedTest, RemoveKeyRemovesOnlySelectedProvider)
{
    SharedAPIKeys keys;
    keys.updateKey(Provider::HuggingFace, "hf-token");
    keys.updateKey(Provider::Stability, "stability-token");

    keys.removeKey(Provider::HuggingFace);

    EXPECT_FALSE(keys.savedTokens.contains(Provider::HuggingFace));
    ASSERT_TRUE(keys.savedTokens.contains(Provider::Stability));
    EXPECT_EQ(keys.savedTokens[Provider::Stability], "stability-token");
    EXPECT_FALSE(Settings::containsKey("apikeys.HuggingFace"));
    EXPECT_EQ(Settings::getString("apikeys.Stability"), "stability-token");
}

// Verifies that removing an absent provider key is safe and does not alter existing keys.
TEST_F(SettingsBackedTest, RemoveMissingKeyIsNoOp)
{
    SharedAPIKeys keys;
    keys.updateKey(Provider::Stability, "stability-token");

    keys.removeKey(Provider::HuggingFace);

    EXPECT_FALSE(keys.savedTokens.contains(Provider::HuggingFace));
    ASSERT_TRUE(keys.savedTokens.contains(Provider::Stability));
    EXPECT_EQ(keys.savedTokens[Provider::Stability], "stability-token");
}

// Verifies that valid JSON is parsed into a JUCE var without losing object data.
TEST(ClientJsonTest, ParseJSONStringAcceptsValidJson)
{
    var data;

    OpResult result = parseJSONString(R"({"name":"harp"})", data);

    EXPECT_TRUE(result.wasOk());
    ASSERT_TRUE(data.isObject());
    EXPECT_EQ(data.getDynamicObject()->getProperty("name").toString(), "harp");
}

// Verifies that malformed JSON is rejected with the expected JSON error type.
TEST(ClientJsonTest, ParseJSONStringRejectsInvalidJson)
{
    var data;
    
    OpResult result = parseJSONString(R"({"name":)", data);
    
    EXPECT_TRUE(result.failed());
    const auto* error = std::get_if<JsonError>(&result.getError());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->type, JsonError::Type::InvalidJSON);
}

// Verifies that dictionary JSON is converted into a DynamicObject pointer.
TEST(ClientJsonTest, StringJSONToDictAcceptsDictionary)
{
    DynamicObject::Ptr dict;

    OpResult result = stringJSONToDict(R"({"value": 5})", dict);

    EXPECT_TRUE(result.wasOk());
    ASSERT_NE(dict, nullptr);
    EXPECT_EQ(static_cast<int>(dict->getProperty("value")), 5);
}

// Verifies that array JSON is converted into a JUCE Array for downstream parsing.
TEST(ClientJsonTest, StringJSONToListAcceptsArray)
{
    Array<var> list;

    OpResult result = stringJSONToList(R"([1,2,3])", list);

    EXPECT_TRUE(result.wasOk());
    ASSERT_EQ(list.size(), 3);
    EXPECT_EQ(static_cast<int>(list[0]), 1);
}

// Verifies that non-array JSON is rejected when an array response is required.
TEST(ClientJsonTest, StringJSONToListRejectsDictionary)
{
    Array<var> list;

    OpResult result = stringJSONToList(R"({"value": 5})", list);

    EXPECT_TRUE(result.failed());
    const auto* error = std::get_if<JsonError>(&result.getError());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->type, JsonError::Type::NotAnArray);
}

// Verifies successful extraction of a required object property.
TEST(ClientJsonTest, GetRequiredDictPropertyAcceptsObjectProperty)
{
    DynamicObject::Ptr child = new DynamicObject();
    child->setProperty("name", "child");
    DynamicObject::Ptr parent = new DynamicObject();
    parent->setProperty("child", var(child));

    DynamicObject::Ptr out;
    OpResult result = getRequiredDictProperty(parent, Identifier("child"), out);

    EXPECT_TRUE(result.wasOk());
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->getProperty("name").toString(), "child");
}

// Verifies that missing required object properties are rejected explicitly.
TEST(ClientJsonTest, GetRequiredDictPropertyRejectsMissingKey)
{
    DynamicObject::Ptr parent = new DynamicObject();
    DynamicObject::Ptr out;

    OpResult result = getRequiredDictProperty(parent, Identifier("child"), out);

    EXPECT_TRUE(result.failed());
    const auto* error = std::get_if<JsonError>(&result.getError());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->type, JsonError::Type::MissingKey);
}

// Verifies that required object properties reject values with the wrong type.
TEST(ClientJsonTest, GetRequiredDictPropertyRejectsNonObjectValue)
{
    DynamicObject::Ptr parent = new DynamicObject();
    parent->setProperty("child", 5);
    DynamicObject::Ptr out;

    OpResult result = getRequiredDictProperty(parent, Identifier("child"), out);

    EXPECT_TRUE(result.failed());
    const auto* error = std::get_if<JsonError>(&result.getError());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->type, JsonError::Type::NotADictionary);
}

// Verifies successful extraction of a required array property.
TEST(ClientJsonTest, GetRequiredArrayPropertyAcceptsArrayProperty)
{
    Array<var> values;
    values.add(1);
    values.add(2);
    DynamicObject::Ptr parent = new DynamicObject();
    parent->setProperty("values", var(values));

    Array<var>* out = nullptr;
    OpResult result = getRequiredArrayProperty(parent, Identifier("values"), out);

    EXPECT_TRUE(result.wasOk());
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->size(), 2);
}

// Verifies that missing required array properties are rejected explicitly.
TEST(ClientJsonTest, GetRequiredArrayPropertyRejectsMissingKey)
{
    DynamicObject::Ptr parent = new DynamicObject();
    Array<var>* out = nullptr;

    OpResult result = getRequiredArrayProperty(parent, Identifier("values"), out);

    EXPECT_TRUE(result.failed());
    const auto* error = std::get_if<JsonError>(&result.getError());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->type, JsonError::Type::MissingKey);
}

// Verifies that required array properties reject values with the wrong type.
TEST(ClientJsonTest, GetRequiredArrayPropertyRejectsNonArrayValue)
{
    DynamicObject::Ptr parent = new DynamicObject();
    parent->setProperty("values", "not-an-array");
    Array<var>* out = nullptr;

    OpResult result = getRequiredArrayProperty(parent, Identifier("values"), out);

    EXPECT_TRUE(result.failed());
    const auto* error = std::get_if<JsonError>(&result.getError());
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->type, JsonError::Type::NotAnArray);
}

// Verifies the base Client upload behavior without invoking provider-specific network upload.
TEST(ClientDefaultBehaviorTest, UploadFilePassesThroughLocalPath)
{
    MinimalClient client;
    File file = File::createFileWithoutCheckingPath("/tmp/input.wav");
    String remotePath;

    OpResult result = client.uploadFile("model", file, remotePath);

    EXPECT_TRUE(result.wasOk());
    EXPECT_EQ(remotePath, file.getFullPathName());
}

// Verifies that the base Client cancel operation is a safe no-op.
TEST(ClientDefaultBehaviorTest, CancelDefaultsToOk)
{
    MinimalClient client;

    OpResult result = client.cancel("model");

    EXPECT_TRUE(result.wasOk());
}
