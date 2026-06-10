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
from tools.model_agent.analyze import analyze_app_source, analyze_path
from tools.model_agent.cli import attach_health
from tools.model_agent.recipe import (
    RecipeError,
    build_package_from_recipe,
    recipe_skeleton_from_analysis,
    render_app_from_recipe,
    validate_recipe,
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


class HarvestTest(unittest.TestCase):
    def test_harvest_writes_app_files_and_index(self):
        class FakeScraper:
            def discover(self, **_kwargs):
                return [
                    SpaceCandidate(id="teamup-tech/alpha", sdk="gradio"),
                    SpaceCandidate(id="teamup-tech/beta", sdk="gradio"),
                ]

            def get_space_file(self, space_id, filename="app.py"):
                if space_id == "teamup-tech/alpha":
                    return "print('alpha app')\n"
                return None

        with tempfile.TemporaryDirectory() as tmp:
            results = HarpModelAgent(scraper=FakeScraper()).harvest_space_apps(
                Path(tmp), author="teamup-tech"
            )

            statuses = {record["id"]: record["status"] for record in results}
            self.assertEqual(statuses["teamup-tech/alpha"], "ok")
            self.assertEqual(statuses["teamup-tech/beta"], "missing")
            self.assertTrue((Path(tmp) / "teamup-tech-alpha" / "app.py").exists())
            self.assertTrue((Path(tmp) / "index.json").exists())


class AnalyzeTest(unittest.TestCase):
    SAMPLE_APP = '''
import spaces
import gradio as gr
from pyharp.core import ModelCard, build_endpoint
from pyharp.labels import LabelList

model_card = ModelCard(name="x", description="y", author="z", tags=[])


@spaces.GPU
def process_fn(audio_path, model_name):
    return audio_path, audio_path, {"labels": []}


with gr.Blocks() as demo:
    input_audio = gr.Audio(type="filepath", label="Input Audio").harp_required(True)
    model_dropdown = gr.Dropdown(choices=["a", "b"], label="Model", value="a")
    out_drums = gr.Audio(type="filepath", label="Drums")
    out_bass = gr.Audio(type="filepath", label="Bass")
    out_labels = gr.JSON(label="Labels")
    build_endpoint(
        model_card=model_card,
        input_components=[input_audio, model_dropdown],
        output_components=[out_drums, out_bass, out_labels],
        process_fn=process_fn,
    )
'''

    DYNAMIC_APP = (
        "import gradio as gr\n"
        "from pyharp import build_endpoint\n"
        "components = make_components()\n"
        "build_endpoint(input_components=components, output_components=components)\n"
    )

    EMPTY_APP = (
        "import gradio as gr\n"
        "from pyharp import build_endpoint\n"
        "build_endpoint(model_card, process_fn)\n"
    )

    def test_analyzes_input_output_shapes(self):
        record = analyze_app_source(self.SAMPLE_APP)
        self.assertTrue(record["build_endpoint_found"])
        self.assertEqual(record["inputs"], ["Audio", "Dropdown"])
        self.assertEqual(record["outputs"], ["Audio", "Audio", "JSON"])
        self.assertTrue(record["uses_spaces_gpu"])
        self.assertTrue(record["uses_labellist"])
        self.assertIn("pyharp.core", record["pyharp_imports"])
        self.assertIn("pyharp.labels", record["pyharp_imports"])

    def test_clean_wrapper_is_recipe_eligible(self):
        record = analyze_app_source(self.SAMPLE_APP)
        self.assertTrue(record["recipe_eligible"])
        self.assertEqual(record["unresolved_reason"], "")

    def test_dynamic_components_not_recipe_eligible(self):
        record = analyze_app_source(self.DYNAMIC_APP)
        self.assertEqual(record["inputs"], ["dynamic"])
        self.assertFalse(record["recipe_eligible"])
        self.assertEqual(record["unresolved_reason"], "dynamic/unresolved component types")

    def test_missing_components_not_recipe_eligible(self):
        record = analyze_app_source(self.EMPTY_APP)
        self.assertEqual(record["inputs"], [])
        self.assertFalse(record["recipe_eligible"])
        self.assertEqual(record["unresolved_reason"], "no resolvable components")

    def test_resolves_inline_components(self):
        source = (
            "import gradio as gr\n"
            "from pyharp.core import build_endpoint\n"
            "build_endpoint(\n"
            "    input_components=[gr.Audio(type='filepath').harp_required(True)],\n"
            "    output_components=[gr.Audio()],\n"
            ")\n"
        )
        record = analyze_app_source(source)
        self.assertEqual(record["inputs"], ["Audio"])
        self.assertEqual(record["outputs"], ["Audio"])
        self.assertFalse(record["uses_spaces_gpu"])

    def test_harp_required_reads_boolean_argument(self):
        # ``.harp_required(False)`` must read as *not* required (the canonical
        # HARP 3.0.0 template uses it on an optional audio input).
        source = (
            "import gradio as gr\n"
            "from pyharp import *\n"
            "build_endpoint(\n"
            "    input_components=[\n"
            "        gr.Audio(type='filepath', label='Optional')"
            ".harp_required(False).set_info('not used'),\n"
            "        gr.Textbox(label='Prompt').harp_required(True),\n"
            "    ],\n"
            "    output_components=[gr.Audio(label='Out')],\n"
            ")\n"
        )
        record = analyze_app_source(source)
        audio, textbox = record["input_details"]
        self.assertFalse(audio["harp_required"])
        self.assertEqual(audio["info"], "not used")
        self.assertTrue(textbox["harp_required"])

    def test_extracts_choices_info_and_file_types(self):
        source = (
            "import gradio as gr\n"
            "from pyharp import *\n"
            "build_endpoint(\n"
            "    input_components=[\n"
            "        gr.Dropdown(choices=['a', 'b'], value='b', label='Pick', info='hint'),\n"
            "        gr.Slider(minimum=0, maximum=20, step=2, value=4, label='Amt'),\n"
            "    ],\n"
            "    output_components=[\n"
            "        gr.File(type='filepath', label='MIDI', file_types=['.mid', '.midi']),\n"
            "    ],\n"
            ")\n"
        )
        record = analyze_app_source(source)
        dropdown, slider = record["input_details"]
        self.assertEqual(dropdown["choices"], ["a", "b"])
        self.assertEqual(dropdown["default"], "b")
        self.assertEqual(dropdown["info"], "hint")
        self.assertEqual((slider["min"], slider["max"], slider["step"]), (0, 20, 2))
        self.assertEqual(slider["default"], 4)
        self.assertEqual(record["output_details"][0]["file_types"], [".mid", ".midi"])

    def test_reports_syntax_errors(self):
        record = analyze_app_source("def broken(:\n    pass\n")
        self.assertIn("error", record)

    def test_aggregates_directory(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "alpha").mkdir()
            (root / "beta").mkdir()
            (root / "alpha" / "app.py").write_text(self.SAMPLE_APP, encoding="utf-8")
            (root / "beta" / "app.py").write_text(
                "import gradio as gr\n"
                "from pyharp.core import build_endpoint\n"
                "a = gr.Audio()\n"
                "build_endpoint(input_components=[a], output_components=[gr.Textbox()])\n",
                encoding="utf-8",
            )
            report = analyze_path(root)
            summary = report["summary"]
            self.assertEqual(summary["apps_analyzed"], 2)
            self.assertEqual(summary["apps_with_errors"], 0)
            self.assertEqual(summary["uses_spaces_gpu"], 1)
            self.assertEqual(summary["input_component_types"]["Audio"], 2)
            self.assertEqual(summary["output_component_types"]["Textbox"], 1)
            self.assertEqual(summary["recipe_eligible"], 2)
            self.assertEqual(summary["unresolved"], 0)

    def test_flags_unresolved_apps_in_summary(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "good").mkdir()
            (root / "dyn").mkdir()
            (root / "good" / "app.py").write_text(self.SAMPLE_APP, encoding="utf-8")
            (root / "dyn" / "app.py").write_text(self.DYNAMIC_APP, encoding="utf-8")
            report = analyze_path(root)
            summary = report["summary"]
            self.assertEqual(summary["recipe_eligible"], 1)
            self.assertEqual(summary["unresolved"], 1)
            self.assertEqual(len(summary["unresolved_apps"]), 1)


class HealthCheckTest(unittest.TestCase):
    class _FakeEndpointClient:
        def __init__(self, alive_ids):
            self.alive_ids = set(alive_ids)

        def fetch_controls(self, model_path):
            if model_path in self.alive_ids:
                return {"card": {}, "inputs": [{}, {}], "outputs": [{}]}
            raise EndpointProbeError(f"{model_path} is sleeping")

    def test_agent_reports_alive_and_dead(self):
        agent = HarpModelAgent(endpoint_client=self._FakeEndpointClient(["author/alive"]))
        alive = agent.check_endpoint_health("author/alive")
        dead = agent.check_endpoint_health("author/dead")
        self.assertEqual(alive["status"], "alive")
        self.assertEqual(alive["n_inputs"], 2)
        self.assertEqual(alive["n_outputs"], 1)
        self.assertEqual(dead["status"], "dead")
        self.assertIn("sleeping", dead["reason"])

    def test_attach_health_joins_index_json(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "teamup-tech-alive").mkdir()
            (root / "teamup-tech-dead").mkdir()
            (root / "teamup-tech-alive" / "app.py").write_text(
                AnalyzeTest.SAMPLE_APP, encoding="utf-8"
            )
            (root / "teamup-tech-dead" / "app.py").write_text(
                AnalyzeTest.SAMPLE_APP, encoding="utf-8"
            )
            index = [
                {
                    "id": "teamup-tech/alive",
                    "path": str(root / "teamup-tech-alive" / "app.py"),
                    "status": "ok",
                },
                {
                    "id": "teamup-tech/dead",
                    "path": str(root / "teamup-tech-dead" / "app.py"),
                    "status": "ok",
                },
            ]
            (root / "index.json").write_text(json.dumps(index), encoding="utf-8")

            agent = HarpModelAgent(
                endpoint_client=HealthCheckTest._FakeEndpointClient(["teamup-tech/alive"])
            )
            report = analyze_path(root)
            attach_health(agent, root, report)

            health_by_slug = {
                Path(record["path"]).parent.name: record["health"] for record in report["apps"]
            }
            self.assertEqual(health_by_slug["teamup-tech-alive"]["status"], "alive")
            self.assertEqual(health_by_slug["teamup-tech-dead"]["status"], "dead")
            self.assertEqual(report["summary"]["health"], {"alive": 1, "dead": 1, "unknown": 0})


class RecipeTest(unittest.TestCase):
    STEM_RECIPE = {
        "model": {
            "id": "example/demucs-stem-separation",
            "name": "Demucs Stem Separation",
            "description": "Separate a song into stems.",
            "author": "example",
            "tags": ["audio-to-audio", "stem-separation"],
        },
        "framework": {
            "import": "demucs",
            "pip": ["demucs", "torch"],
            "apt": ["ffmpeg"],
            "gpu": True,
        },
        "inputs": [
            {"name": "input_audio", "type": "audio", "label": "Input Audio", "required": True},
            {
                "name": "model_name",
                "type": "dropdown",
                "label": "Demucs Model",
                "choices": ["htdemucs", "mdx_extra"],
                "default": "htdemucs",
            },
        ],
        "outputs": [
            {"name": "drums", "type": "audio", "label": "Drums"},
            {"name": "labels", "type": "labels", "label": "Labels"},
        ],
        "inference": {
            "setup": "MODEL = None",
            "body": "return input_audio, {}",
        },
    }

    def test_renders_runnable_app(self):
        app = render_app_from_recipe(self.STEM_RECIPE)
        # Compiles as valid Python.
        compile(app, "<recipe-app>", "exec")
        self.assertIn("import spaces", app)
        self.assertIn("@spaces.GPU", app)
        # Match HARP's reference wrappers: a star import exposes every helper.
        self.assertIn("from pyharp import *", app)
        self.assertIn("def process_fn(input_audio, model_name):", app)
        self.assertIn('gr.Audio(type="filepath", label="Input Audio").harp_required(True)', app)
        self.assertIn('gr.Dropdown(choices=["htdemucs", "mdx_extra"], value="htdemucs"', app)
        self.assertIn('gr.JSON(label="Labels")', app)

    def test_no_gpu_imports(self):
        recipe = json.loads(json.dumps(self.STEM_RECIPE))
        recipe["framework"]["gpu"] = False
        recipe["outputs"] = [{"name": "out", "type": "audio", "label": "Output"}]
        app = render_app_from_recipe(recipe)
        self.assertNotIn("import spaces", app)
        self.assertNotIn("@spaces.GPU", app)
        self.assertIn("from pyharp import *", app)

    def test_renders_info_and_file_types(self):
        recipe = {
            "model": {"id": "example/m", "name": "M"},
            "inputs": [
                {
                    "name": "input_audio",
                    "type": "audio",
                    "label": "In",
                    "required": False,
                    "info": "optional input track",
                },
                {
                    "name": "intensity",
                    "type": "slider",
                    "label": "Intensity",
                    "min": 0,
                    "max": 10,
                    "info": "how strong",
                },
            ],
            "outputs": [
                {
                    "name": "midi",
                    "type": "file",
                    "label": "MIDI",
                    "file_types": [".mid", ".midi"],
                    "info": "the rendered midi",
                },
            ],
            "inference": {"body": "return None"},
        }
        app = render_app_from_recipe(recipe)
        compile(app, "<info-app>", "exec")
        # Native components carry tooltips via info=, media via .set_info().
        self.assertIn('gr.Slider(minimum=0, maximum=10, label="Intensity", info="how strong")', app)
        self.assertIn(
            'gr.Audio(type="filepath", label="In").set_info("optional input track")', app
        )
        self.assertIn(
            'gr.File(type="filepath", label="MIDI", file_types=[".mid", ".midi"])'
            '.set_info("the rendered midi")',
            app,
        )

    def test_validation_rejects_bad_recipe(self):
        with self.assertRaises(RecipeError):
            validate_recipe({"model": {}, "inputs": [], "outputs": []})
        with self.assertRaises(RecipeError):
            validate_recipe(
                {
                    "model": {"id": "a/b", "name": "X"},
                    "inputs": [{"name": "x", "type": "dropdown", "label": "X"}],
                    "outputs": [{"name": "y", "type": "audio", "label": "Y"}],
                    "inference": {"body": "return x"},
                }
            )

    def test_builds_package_files(self):
        package = build_package_from_recipe(self.STEM_RECIPE)
        self.assertEqual(package.repo_id, "example/demucs-stem-separation")
        self.assertIn("demucs", package.requirements)
        self.assertIn("ffmpeg", package.packages_txt)
        self.assertEqual(package.io["outputs"], ["audio", "labels"])
        self.assertIn("pyharp", package.requirements)


class RecipeScaffoldTest(unittest.TestCase):
    def test_scaffold_from_clean_wrapper(self):
        record = analyze_app_source(AnalyzeTest.SAMPLE_APP)
        recipe = recipe_skeleton_from_analysis(record, model_id="teamup-tech/demucs")

        # The skeleton must itself be a valid recipe and render a stub wrapper.
        validate_recipe(recipe)
        app = render_app_from_recipe(recipe)
        compile(app, "<scaffold>", "exec")
        self.assertIn("raise NotImplementedError", app)

        self.assertEqual([spec["type"] for spec in recipe["inputs"]], ["audio", "dropdown"])
        self.assertEqual(
            [spec["type"] for spec in recipe["outputs"]], ["audio", "audio", "labels"]
        )
        self.assertTrue(recipe["inputs"][0]["required"])
        self.assertTrue(recipe["framework"]["gpu"])
        self.assertEqual(recipe["model"]["id"], "teamup-tech/demucs")
        self.assertTrue(recipe["_todo"])

    def test_scaffold_fills_choices_from_analysis(self):
        # The SAMPLE_APP dropdown has concrete choices/value; the scaffold must
        # carry them through instead of emitting TODO placeholders.
        record = analyze_app_source(AnalyzeTest.SAMPLE_APP)
        recipe = recipe_skeleton_from_analysis(record, model_id="teamup-tech/demucs")
        dropdown = recipe["inputs"][1]
        self.assertEqual(dropdown["type"], "dropdown")
        self.assertEqual(dropdown["choices"], ["a", "b"])
        self.assertEqual(dropdown["default"], "a")
        # No leftover TODO for choices resolved.
        self.assertFalse(any("choices" in todo for todo in recipe["_todo"]))

    def test_scaffold_rejects_unmappable_component(self):
        record = {
            "build_endpoint_found": True,
            "recipe_eligible": True,
            "input_details": [{"type": "Image", "label": "Pic"}],
            "output_details": [{"type": "Audio", "label": "Out"}],
            "uses_spaces_gpu": False,
        }
        with self.assertRaises(RecipeError):
            recipe_skeleton_from_analysis(record)


class ExampleRecipeFilesTest(unittest.TestCase):
    def test_committed_recipes_render(self):
        examples_dir = Path(__file__).resolve().parent.parent / "examples"
        recipe_files = sorted(examples_dir.glob("recipe_*.json"))
        self.assertTrue(recipe_files, "expected committed example recipes")
        for recipe_file in recipe_files:
            recipe = json.loads(recipe_file.read_text(encoding="utf-8"))
            app = render_app_from_recipe(recipe)
            compile(app, str(recipe_file), "exec")


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
