// -----------------------------------------------------------------------------
// Test rationale
// -----------------------------------------------------------------------------
// This file verifies deterministic, HARP-owned behavior in GradioClient.h
// without exercising external Gradio, Hugging Face, HTTP, or file-download
// communication.
//
// The tests focus on three categories of GradioClient behavior:
//
// Category: Path validation
// - GradioClient::matchesPathSpec: verifies that supported local, Gradio,
//   and Hugging Face path formats are accepted while unsupported or ambiguous
//   paths are rejected.
//
// Category: Path inference
// - GradioClient::inferHostSlashModel: verifies canonical host/model extraction.
// - GradioClient::inferEndpointPath: verifies endpoint URL normalization and
//   Hugging Face Space URL construction.
// - GradioClient::inferDocumentationPath: verifies documentation URL inference
//   for supported Hugging Face path formats.
//
// Category: Payload transformation
// - GradioClient::wrapPayloadElement: verifies that file payloads are wrapped
//   with Gradio-specific metadata while non-file payloads remain unchanged.
//
// The makePayloadObject helper creates the minimal DynamicObject needed by
// payload transformation tests. It keeps those tests focused on Gradio metadata
// behavior rather than repeated object setup.
// -----------------------------------------------------------------------------

#include <gtest/gtest.h>

#include <juce_core/juce_core.h>

#include <juce_data_structures/juce_data_structures.h>

#include "../../src/clients/GradioClient.h"

using namespace juce;

namespace
{
// Creates a minimal file-like payload object used by wrapPayloadElement tests.
DynamicObject::Ptr makePayloadObject(const String& path = "input.wav")
{
    DynamicObject::Ptr object = new DynamicObject();
    object->setProperty("path", path);
    return object;
}
} // namespace

// Verifies that local paths accepted by HARP are recognized before network use.
TEST(GradioClientPathValidationTest, AcceptsSupportedLocalPaths)
{
    EXPECT_TRUE(GradioClient::matchesPathSpec("localhost:7860"));
    EXPECT_TRUE(GradioClient::matchesPathSpec("http://localhost:7860"));
    EXPECT_TRUE(GradioClient::matchesPathSpec("127.0.0.1:7860"));
    EXPECT_TRUE(GradioClient::matchesPathSpec("192.168.1.10:7860"));
}

// Verifies that temporary Gradio share URLs are accepted as supported model paths.
TEST(GradioClientPathValidationTest, AcceptsSupportedGradioLivePaths)
{
    EXPECT_TRUE(GradioClient::matchesPathSpec("https://abc123.gradio.live"));
    EXPECT_TRUE(GradioClient::matchesPathSpec("https://my-temporary-space.gradio.live"));
}

// Verifies that all supported Hugging Face path formats are accepted.
TEST(GradioClientPathValidationTest, AcceptsSupportedHuggingFacePaths)
{
    EXPECT_TRUE(GradioClient::matchesPathSpec("https://owner-model.hf.space/"));
    EXPECT_TRUE(GradioClient::matchesPathSpec("https://huggingface.co/spaces/owner/model_name"));
    EXPECT_TRUE(GradioClient::matchesPathSpec("owner/model_name"));
}

// Verifies that unsupported or malformed paths are rejected before endpoint inference.
TEST(GradioClientPathValidationTest, RejectsUnsupportedPaths)
{
    EXPECT_FALSE(GradioClient::matchesPathSpec(""));
    EXPECT_FALSE(GradioClient::matchesPathSpec("   "));
    EXPECT_FALSE(GradioClient::matchesPathSpec("https://example.com/model"));
    EXPECT_FALSE(GradioClient::matchesPathSpec("ftp://owner-model.hf.space/"));
    EXPECT_FALSE(GradioClient::matchesPathSpec("not a path"));
}

// Verifies that ambiguous short Hugging Face paths are rejected instead of guessed.
TEST(GradioClientPathValidationTest, RejectsAmbiguousShortHuggingFacePath)
{
    EXPECT_FALSE(GradioClient::matchesPathSpec("https://owner-model-extra.hf.space/"));
}

// Verifies that local and Gradio paths map to the expected local host/model identity.
TEST(GradioClientPathInferenceTest, InferHostSlashModelReturnsLocalhostForLocalAndGradioPaths)
{
    GradioClient client;

    EXPECT_EQ(client.inferHostSlashModel("localhost:7860"), "localhost");
    EXPECT_EQ(client.inferHostSlashModel("http://localhost:7860"), "localhost");
    EXPECT_EQ(client.inferHostSlashModel("https://abc123.gradio.live"), "localhost");
}

// Verifies host/model extraction from short Hugging Face Space URLs.
TEST(GradioClientPathInferenceTest, InferHostSlashModelParsesShortHuggingFacePath)
{
    GradioClient client;

    EXPECT_EQ(client.inferHostSlashModel("https://owner-model.hf.space/"), "owner/model");
}

// Verifies host/model extraction from long Hugging Face Space URLs.
TEST(GradioClientPathInferenceTest, InferHostSlashModelParsesLongHuggingFacePath)
{
    GradioClient client;

    EXPECT_EQ(client.inferHostSlashModel("https://huggingface.co/spaces/owner/model_name"), "owner/model_name");
}

// Verifies that abbreviated Hugging Face paths already in owner/model form are preserved.
TEST(GradioClientPathInferenceTest, InferHostSlashModelPreservesAbbreviatedHuggingFacePath)
{
    GradioClient client;

    EXPECT_EQ(client.inferHostSlashModel("owner/model_name"), "owner/model_name");
}

// Verifies that invalid paths fail closed by returning an empty host/model string.
TEST(GradioClientPathInferenceTest, InferHostSlashModelReturnsEmptyForInvalidPath)
{
    GradioClient client;

    EXPECT_EQ(client.inferHostSlashModel("https://example.com/model"), "");
}

// Verifies that local paths without a protocol are normalized to HTTP endpoints.
TEST(GradioClientPathInferenceTest, InferEndpointPathNormalizesLocalPathWithoutProtocol)
{
    GradioClient client;

    EXPECT_EQ(client.inferEndpointPath("localhost:7860"), "http://localhost:7860");
    EXPECT_EQ(client.inferEndpointPath("127.0.0.1:7860"), "http://127.0.0.1:7860");
}

// Verifies that local paths already using HTTP are not modified.
TEST(GradioClientPathInferenceTest, InferEndpointPathPreservesLocalPathWithHttpProtocol)
{
    GradioClient client;

    EXPECT_EQ(client.inferEndpointPath("http://localhost:7860"), "http://localhost:7860");
}

// Verifies that endpoint inference preserves direct Gradio and short Hugging Face URLs.
TEST(GradioClientPathInferenceTest, InferEndpointPathPreservesGradioLiveAndShortHuggingFacePaths)
{
    GradioClient client;

    EXPECT_EQ(client.inferEndpointPath("https://abc123.gradio.live"), "https://abc123.gradio.live");
    EXPECT_EQ(client.inferEndpointPath("https://owner-model.hf.space/"), "https://owner-model.hf.space/");
}

// Verifies endpoint construction from long Hugging Face paths, including underscore normalization.
TEST(GradioClientPathInferenceTest, InferEndpointPathBuildsHuggingFaceSpaceUrlFromLongPath)
{
    GradioClient client;

    EXPECT_EQ(client.inferEndpointPath("https://huggingface.co/spaces/owner/model_name"),
              "https://owner-model-name.hf.space/");
}

// Verifies endpoint construction from abbreviated Hugging Face owner/model paths.
TEST(GradioClientPathInferenceTest, InferEndpointPathBuildsHuggingFaceSpaceUrlFromAbbreviatedPath)
{
    GradioClient client;

    EXPECT_EQ(client.inferEndpointPath("owner/model_name"), "https://owner-model-name.hf.space/");
}

// Verifies that invalid paths fail closed by returning an empty endpoint path.
TEST(GradioClientPathInferenceTest, InferEndpointPathReturnsEmptyForInvalidPath)
{
    GradioClient client;

    EXPECT_EQ(client.inferEndpointPath("https://example.com/model"), "");
}

// Verifies documentation URL construction from short and abbreviated Hugging Face paths.
TEST(GradioClientPathInferenceTest, InferDocumentationPathBuildsHuggingFaceLongDocumentationUrl)
{
    GradioClient client;

    EXPECT_EQ(client.inferDocumentationPath("https://owner-model.hf.space/"),
              "https://huggingface.co/spaces/owner/model");
    EXPECT_EQ(client.inferDocumentationPath("owner/model_name"),
              "https://huggingface.co/spaces/owner/model_name");
}

// Verifies that long Hugging Face documentation paths are already canonical and are preserved.
TEST(GradioClientPathInferenceTest, InferDocumentationPathPreservesLongHuggingFacePath)
{
    GradioClient client;

    EXPECT_EQ(client.inferDocumentationPath("https://huggingface.co/spaces/owner/model_name"),
              "https://huggingface.co/spaces/owner/model_name");
}

// Verifies that non-file payloads are not modified by Gradio-specific file wrapping.
TEST(GradioClientPayloadTest, WrapPayloadElementLeavesNonFilePayloadUnchanged)
{
    GradioClient client;
    var payload("hello");

    var wrapped = client.wrapPayloadElement(payload, false);

    EXPECT_EQ(wrapped.toString(), "hello");
}

// Verifies that void file payloads remain unchanged instead of receiving metadata.
TEST(GradioClientPayloadTest, WrapPayloadElementLeavesVoidFilePayloadUnchanged)
{
    GradioClient client;
    var payload;

    var wrapped = client.wrapPayloadElement(payload, true);

    EXPECT_TRUE(wrapped.isVoid());
}

// Verifies that file payloads receive the Gradio FileData metadata required by Gradio.
TEST(GradioClientPayloadTest, WrapPayloadElementAddsGradioFileMetadataForFilePayload)
{
    GradioClient client;
    DynamicObject::Ptr object = makePayloadObject();

    var wrapped = client.wrapPayloadElement(var(object), true);

    ASSERT_TRUE(wrapped.isObject());
    auto* wrappedObject = wrapped.getDynamicObject();
    ASSERT_NE(wrappedObject, nullptr);
    ASSERT_TRUE(wrappedObject->hasProperty("meta"));

    var meta = wrappedObject->getProperty("meta");
    ASSERT_TRUE(meta.isObject());
    EXPECT_EQ(meta.getDynamicObject()->getProperty("_type").toString(), "gradio.FileData");
}

// Verifies that Gradio file wrapping replaces stale metadata with the required FileData type.
TEST(GradioClientPayloadTest, WrapPayloadElementReplacesExistingMetadataForFilePayload)
{
    GradioClient client;
    DynamicObject::Ptr object = makePayloadObject();
    DynamicObject::Ptr originalMeta = new DynamicObject();
    originalMeta->setProperty("_type", "old");
    object->setProperty("meta", var(originalMeta));

    var wrapped = client.wrapPayloadElement(var(object), true);

    ASSERT_TRUE(wrapped.isObject());
    var meta = wrapped.getDynamicObject()->getProperty("meta");
    ASSERT_TRUE(meta.isObject());
    EXPECT_EQ(meta.getDynamicObject()->getProperty("_type").toString(), "gradio.FileData");
}
