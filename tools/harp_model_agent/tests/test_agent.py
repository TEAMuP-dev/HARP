import json
import tempfile
import unittest
from pathlib import Path

from tools.harp_model_agent.agent import HarpEndpointClient, HarpModelAgent, ModelPackage, SpaceCandidate


class EndpointInferenceTest(unittest.TestCase):
    def test_infers_hf_space_endpoint_from_abbrev_path(self):
        self.assertEqual(
            HarpEndpointClient.infer_endpoint_url("teamup-tech/midi_synthesizer"),
            "https://teamup-tech-midi-synthesizer.hf.space/",
        )

    def test_infers_documentation_url_from_short_hf_url(self):
        self.assertEqual(
            HarpEndpointClient.infer_documentation_url(
                "https://teamup-tech-demucs-source-separation.hf.space/"
            ),
            "https://huggingface.co/spaces/teamup-tech/demucs-source-separation",
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


class SpaceCandidateTest(unittest.TestCase):
    def test_candidate_from_api_payload(self):
        candidate = SpaceCandidate.from_api(
            {
                "id": "teamup-tech/demo",
                "likes": 3,
                "tags": ["gradio"],
                "cardData": {"sdk": "gradio", "license": "mit"},
            }
        )
        self.assertTrue(candidate.looks_gradio())
        self.assertTrue(candidate.looks_open_source())
        self.assertEqual(candidate.license, "mit")


class PackageWriterTest(unittest.TestCase):
    def test_writes_package_artifacts(self):
        package = ModelPackage(
            model_path="teamup-tech/demo",
            source_url="https://huggingface.co/spaces/teamup-tech/demo",
            endpoint_url="https://teamup-tech-demo.hf.space/",
            documentation_url="https://huggingface.co/spaces/teamup-tech/demo",
            scraped_at="2026-05-26T00:00:00Z",
            card={"name": "Demo", "description": "A demo model.", "author": "TEAMuP", "tags": []},
            inputs=[],
            outputs=[],
            raw_controls={"card": {}, "inputs": [], "outputs": []},
        )
        with tempfile.TemporaryDirectory() as tmp:
            folder = HarpModelAgent().write_package(package, Path(tmp))
            self.assertTrue((folder / "manifest.json").exists())
            self.assertTrue((folder / "controls.json").exists())
            self.assertIn("Demo", (folder / "README.md").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
