import json
import tempfile
import unittest
from pathlib import Path

from tools.model_agent.agent import (
    EndpointProbeError,
    HarpEndpointClient,
    HarpModelAgent,
    ModelPackage,
    SpaceCandidate,
    build_generated_app_package,
    classify_task,
    detect_inference_framework,
    evaluate_license,
    extract_io_signature,
    render_pyharp_app,
    score_compatibility,
)


class EndpointInferenceTest(unittest.TestCase):
    def test_infers_hf_space_endpoint_from_abbrev_path(self):
        self.assertEqual(
            HarpEndpointClient.infer_endpoint_url("example/audio_model"),
            "https://example-audio-model.hf.space/",
        )

    def test_infers_documentation_url_from_short_hf_url(self):
        self.assertEqual(
            HarpEndpointClient.infer_documentation_url(
                "https://example-audio-model.hf.space/"
            ),
            "https://huggingface.co/spaces/example/audio-model",
        )

    def test_parses_plain_json_gradio_response(self):
        payload = [{"card": {}, "inputs": [], "outputs": []}]
        self.assertEqual(
            HarpEndpointClient._parse_gradio_response(json.dumps(payload)),
            payload,
        )

    def test_parses_sse_gradio_response(self):
        payload = [{"card": {"name": "Demo"}, "inputs": [], "outputs": []}]
        response = "event: complete\ndata: " + json.dumps(payload) + "\n\n"
        self.assertEqual(HarpEndpointClient._parse_gradio_response(response), payload)


class CanonicalPathResolutionTest(unittest.TestCase):
    def test_recovers_underscores_from_gradio_config(self):
        # A short *.hf.space URL flattens "example/audio_model" into
        # "example-audio-model" and loses the underscore; the live config
        # endpoint reports the authoritative id and lets us recover it.
        class FakeConfigClient(HarpEndpointClient):
            def _get_text(self, url):
                if url.endswith("/gradio_api/config"):
                    return json.dumps({"space_id": "example/audio_model"})
                raise EndpointProbeError(f"unexpected GET {url}")

        client = FakeConfigClient()
        self.assertEqual(
            client.resolve_canonical_path("https://example-audio-model.hf.space/"),
            "example/audio_model",
        )

    def test_falls_back_to_string_inference_without_config(self):
        class FailingConfigClient(HarpEndpointClient):
            def _get_text(self, url):
                raise EndpointProbeError(f"no config at {url}")

        client = FailingConfigClient()
        self.assertEqual(
            client.resolve_canonical_path("https://example-audio-model.hf.space/"),
            "example/audio-model",
        )

    def test_trusts_full_hf_spaces_url(self):
        # Full HF Space URLs carry the exact id, so no network call is needed.
        client = HarpEndpointClient()
        self.assertEqual(
            client.resolve_canonical_path(
                "https://huggingface.co/spaces/example/audio_model"
            ),
            "example/audio_model",
        )


class SpaceCandidateTest(unittest.TestCase):
    def test_candidate_from_api_payload(self):
        candidate = SpaceCandidate.from_api(
            {
                "id": "example/audio-model",
                "likes": 3,
                "tags": ["gradio"],
                "cardData": {"sdk": "gradio", "license": "mit"},
            }
        )
        self.assertTrue(candidate.looks_gradio())
        self.assertTrue(candidate.looks_open_source())
        self.assertEqual(candidate.license, "mit")


class DiscoveryTest(unittest.TestCase):
    def test_filters_discovered_spaces_to_open_gradio_candidates(self):
        class FakeScraper:
            def discover(self, **_kwargs):
                return [
                    SpaceCandidate(id="example/open-gradio", sdk="gradio"),
                    SpaceCandidate(id="example/private-gradio", sdk="gradio", private=True),
                    SpaceCandidate(id="example/static-space", sdk="static"),
                ]

        candidates = HarpModelAgent(scraper=FakeScraper()).discover_open_gradio_spaces()

        self.assertEqual([candidate.id for candidate in candidates], ["example/open-gradio"])


class CompatibilityScoringTest(unittest.TestCase):
    def test_classifies_audio_task_from_pipeline_tag(self):
        card = {"meta": {"pipeline_tag": "audio-to-audio", "tags": []}, "readme": ""}

        self.assertEqual(classify_task(card), "audio-to-audio")

    def test_classifies_midi_from_readme(self):
        card = {"meta": {"pipeline_tag": "", "tags": []}, "readme": "This model generates MIDI notes."}

        self.assertEqual(classify_task(card), "midi")

    def test_evaluates_license_status(self):
        self.assertFalse(evaluate_license("mit")["is_blocking"])
        self.assertTrue(evaluate_license("cc-by-nc-4.0")["is_blocking"])

    def test_scores_candidate_with_rationale(self):
        card = {
            "meta": {
                "id": "example/audio-model",
                "author": "example",
                "pipeline_tag": "audio-to-audio",
                "tags": ["audio-to-audio"],
                "license": "mit",
            },
            "files": ["config.json", "model.safetensors"],
            "readme": "This source separation model works at 48 kHz in stereo. " * 20,
        }

        result = score_compatibility(card)

        self.assertEqual(result["task"], "audio-to-audio")
        self.assertEqual(result["blockers"], [])
        self.assertGreater(result["score"], 0.9)
        self.assertEqual(extract_io_signature(card)["sample_rate_hz"], 48000)


def _speechbrain_card():
    return {
        "meta": {
            "id": "example/sepformer-model",
            "author": "example",
            "pipeline_tag": "audio-to-audio",
            "library_name": "speechbrain",
            "tags": ["audio-to-audio", "speechbrain", "Source Separation"],
            "license": "apache-2.0",
        },
        "files": ["hyperparams.yaml", "model.ckpt"],
        "readme": "A SpeechBrain source separation model trained at 8 kHz.",
    }


class TemplateGenerationTest(unittest.TestCase):
    def test_detects_speechbrain_framework(self):
        self.assertEqual(detect_inference_framework(_speechbrain_card()), "speechbrain")

    def test_renders_speechbrain_pyharp_app(self):
        app_py = render_pyharp_app(_speechbrain_card())

        self.assertIn('REPO_ID = "example/sepformer-model"', app_py)
        self.assertIn("build_endpoint", app_py)
        self.assertIn("SepformerSeparation", app_py)
        # The old, non-runnable transformers pipeline must not be emitted.
        self.assertNotIn('pipeline("audio-to-audio"', app_py)

    def test_refuses_unsupported_framework(self):
        card = {
            "meta": {
                "id": "example/audio-model",
                "author": "example",
                "pipeline_tag": "audio-to-audio",
                "tags": ["audio-to-audio"],
                "license": "mit",
            },
            "files": ["config.json", "model.safetensors"],
            "readme": "A test model with no recognized framework.",
        }

        with self.assertRaises(NotImplementedError):
            render_pyharp_app(card)

    def test_writes_generated_app_package(self):
        with tempfile.TemporaryDirectory() as tmp:
            package = build_generated_app_package(_speechbrain_card())
            self.assertEqual(package.framework, "speechbrain")
            folder = HarpModelAgent().write_generated_app_package(package, Path(tmp))

            self.assertTrue((folder / "app.py").exists())
            self.assertTrue((folder / "requirements.txt").exists())
            self.assertIn("speechbrain", (folder / "requirements.txt").read_text(encoding="utf-8"))
            self.assertTrue((folder / "README.md").exists())
            self.assertTrue((folder / "packages.txt").exists())
            self.assertTrue((folder / ".harp" / "manifest.json").exists())


class PackageWriterTest(unittest.TestCase):
    def test_writes_package_artifacts(self):
        package = ModelPackage(
            model_path="example/audio-model",
            source_url="https://huggingface.co/spaces/example/audio-model",
            endpoint_url="https://example-audio-model.hf.space/",
            documentation_url="https://huggingface.co/spaces/example/audio-model",
            scraped_at="2026-05-26T00:00:00Z",
            card={
                "name": "Example Audio Model",
                "description": "A fake model used only by tests.",
                "author": "Example",
                "tags": [],
            },
            inputs=[],
            outputs=[],
            raw_controls={"card": {}, "inputs": [], "outputs": []},
        )
        with tempfile.TemporaryDirectory() as tmp:
            folder = HarpModelAgent().write_package(package, Path(tmp))
            self.assertTrue((folder / "manifest.json").exists())
            self.assertTrue((folder / "controls.json").exists())
            self.assertIn(
                "Example Audio Model",
                (folder / "README.md").read_text(encoding="utf-8"),
            )


if __name__ == "__main__":
    unittest.main()
