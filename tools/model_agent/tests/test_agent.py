import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from tools.model_agent.agent import (
    HARP_GRADIO_VERSION,
    PYHARP_REQUIREMENT,
    DeploySpaceError,
    EndpointProbeError,
    HarpEndpointClient,
    HarpModelAgent,
    ModelPackage,
    SpaceCandidate,
    VenvSetupError,
    _parse_github_url,
    build_generated_app_package,
    lint_generated_app,
    merge_frozen_pins,
    parse_freeze,
    reconcile_readme,
    reconcile_requirements,
    classify_task,
    detect_inference_framework,
    evaluate_license,
    extract_io_signature,
    render_pyharp_app,
    score_compatibility,
)
from tools.model_agent.analyze import analyze_app_source, analyze_path
from tools.model_agent.cli import attach_health, _ensure_github_pip
from tools.model_agent.llm import (
    GeminiProvider,
    LLMError,
    RecipeGenerationContext,
    build_recipe_user_prompt,
    complete_recipe,
    default_examples,
    generate_recipe,
    provider_from_env,
)
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


class VenvTest(unittest.TestCase):
    """Verify the isolated smoke-test venv builds, caches, and rebuilds.

    pip/venv calls are stubbed so the test stays offline and fast; only the
    caching/invalidation logic is exercised.
    """

    @staticmethod
    def _interpreter_path(venv_dir: Path) -> Path:
        if os.name == "nt":
            return venv_dir / "Scripts" / "python.exe"
        return venv_dir / "bin" / "python"

    def _patch_pip(self, agent: HarpModelAgent, calls: list):
        def fake_pip(cmd, *, log=None, check=True):
            calls.append(list(cmd))
            # Simulate `python -m venv <dir>` by materializing the interpreter.
            if "venv" in cmd:
                interpreter = self._interpreter_path(Path(cmd[-1]))
                interpreter.parent.mkdir(parents=True, exist_ok=True)
                interpreter.write_text("", encoding="utf-8")

        agent._run_pip = fake_pip  # type: ignore[assignment]

    def test_builds_then_reuses_then_rebuilds(self):
        agent = HarpModelAgent()
        calls: list = []
        self._patch_pip(agent, calls)

        with tempfile.TemporaryDirectory() as tmp:
            pkg = Path(tmp) / "pkg"
            pkg.mkdir()
            (pkg / "requirements.txt").write_text("pyharp\n", encoding="utf-8")

            python_a = agent.ensure_package_venv(pkg)
            self.assertTrue(Path(python_a).exists())
            self.assertTrue(calls, "first run should build the venv")

            calls.clear()
            python_b = agent.ensure_package_venv(pkg)
            self.assertEqual(python_a, python_b)
            self.assertEqual(calls, [], "unchanged requirements should reuse the venv")

            (pkg / "requirements.txt").write_text("pyharp\ntorch\n", encoding="utf-8")
            agent.ensure_package_venv(pkg)
            self.assertTrue(calls, "changed requirements should rebuild the venv")

    def test_pip_failure_raises_venv_setup_error(self):
        agent = HarpModelAgent()

        def failing_pip(cmd, *, log=None, check=True):
            if "venv" in cmd:
                interpreter = self._interpreter_path(Path(cmd[-1]))
                interpreter.parent.mkdir(parents=True, exist_ok=True)
                interpreter.write_text("", encoding="utf-8")
                return
            if check:
                raise VenvSetupError("pip exploded")

        agent._run_pip = failing_pip  # type: ignore[assignment]

        with tempfile.TemporaryDirectory() as tmp:
            pkg = Path(tmp) / "pkg"
            pkg.mkdir()
            (pkg / "requirements.txt").write_text("pyharp\n", encoding="utf-8")
            with self.assertRaises(VenvSetupError):
                agent.ensure_package_venv(pkg)


class DeploySpaceTest(unittest.TestCase):
    """Verify Space deployment wiring with a stubbed huggingface_hub."""

    @staticmethod
    def _fake_hub(calls: dict):
        import types

        module = types.ModuleType("huggingface_hub")

        class FakeApi:
            def __init__(self, token=None):
                calls["token"] = token

            def create_repo(self, **kwargs):
                calls["create"] = kwargs

            def upload_folder(self, **kwargs):
                calls["upload"] = kwargs

        module.HfApi = FakeApi  # type: ignore[attr-defined]
        return module

    def _make_package(self, root: Path) -> Path:
        pkg = root / "pkg"
        pkg.mkdir()
        (pkg / "app.py").write_text("print('hi')\n", encoding="utf-8")
        (pkg / "README.md").write_text("---\nsdk: gradio\n---\n", encoding="utf-8")
        (pkg / "requirements.txt").write_text("pyharp\n", encoding="utf-8")
        return pkg

    def test_creates_space_and_uploads_folder(self):
        import sys

        calls: dict = {}
        with mock.patch.dict(sys.modules, {"huggingface_hub": self._fake_hub(calls)}):
            with tempfile.TemporaryDirectory() as tmp:
                pkg = self._make_package(Path(tmp))
                result = HarpModelAgent().deploy_space(
                    pkg, "https://huggingface.co/spaces/me/cool-space", token="tok"
                )

        self.assertEqual(result["repo_id"], "me/cool-space")
        self.assertEqual(result["space_url"], "https://huggingface.co/spaces/me/cool-space")
        self.assertTrue(result["authenticated"])
        self.assertEqual(calls["token"], "tok")
        self.assertEqual(calls["create"].get("repo_id"), "me/cool-space")
        self.assertEqual(calls["create"].get("repo_type"), "space")
        self.assertEqual(calls["create"].get("space_sdk"), "gradio")
        self.assertEqual(calls["upload"].get("repo_id"), "me/cool-space")
        self.assertEqual(calls["upload"].get("repo_type"), "space")
        self.assertIn(".venv/*", calls["upload"].get("ignore_patterns", []))

    def test_missing_app_py_raises(self):
        with tempfile.TemporaryDirectory() as tmp:
            empty = Path(tmp) / "empty"
            empty.mkdir()
            with self.assertRaises(DeploySpaceError):
                HarpModelAgent().deploy_space(empty, "me/space")

    def test_bad_repo_id_raises(self):
        with tempfile.TemporaryDirectory() as tmp:
            pkg = self._make_package(Path(tmp))
            with self.assertRaises(DeploySpaceError):
                HarpModelAgent().deploy_space(pkg, "no-slash-here")

    def test_missing_library_raises_clear_error(self):
        import sys

        with tempfile.TemporaryDirectory() as tmp:
            pkg = self._make_package(Path(tmp))
            with mock.patch.dict(sys.modules, {"huggingface_hub": None}):
                with self.assertRaises(DeploySpaceError) as ctx:
                    HarpModelAgent().deploy_space(pkg, "me/space")
        self.assertIn("huggingface_hub", str(ctx.exception))


class ReconcileTest(unittest.TestCase):
    """Dependency reconciliation aligns a Space on pyharp's gradio pin."""

    def test_requirements_pins_gradio_and_adds_pyharp(self):
        new, changes = reconcile_requirements("gradio==6.3.0\nnumpy\n")
        self.assertIn(f"gradio=={HARP_GRADIO_VERSION}", new)
        self.assertNotIn("6.3.0", new)
        self.assertIn("pyharp", new)
        self.assertTrue(changes)

    def test_requirements_preserves_extras(self):
        new, _ = reconcile_requirements("gradio[oauth,mcp]==6.3.0\n")
        self.assertIn(f"gradio[oauth,mcp]=={HARP_GRADIO_VERSION}", new)

    def test_requirements_adds_gradio_when_absent(self):
        new, _ = reconcile_requirements("torch\n")
        self.assertIn(f"gradio=={HARP_GRADIO_VERSION}", new)
        self.assertIn("torch", new)

    def test_requirements_does_not_duplicate_pyharp(self):
        new, _ = reconcile_requirements(f"{PYHARP_REQUIREMENT}\ngradio==6.3.0\n")
        self.assertEqual(new.count("TEAMuP-dev/pyharp"), 1)

    def test_requirements_preserves_directives_and_comments(self):
        new, _ = reconcile_requirements("# deps\n-r base.txt\ntorch\n")
        self.assertIn("# deps", new)
        self.assertIn("-r base.txt", new)

    def test_readme_sets_sdk_version(self):
        readme = '---\nsdk: gradio\nsdk_version: "6.3.0"\napp_file: app.py\n---\n# title\n'
        new, changes = reconcile_readme(readme)
        self.assertIn(f'sdk_version: "{HARP_GRADIO_VERSION}"', new)
        self.assertNotIn('"6.3.0"', new)
        self.assertTrue(changes)

    def test_readme_adds_sdk_version_when_missing(self):
        new, _ = reconcile_readme("---\nsdk: gradio\napp_file: app.py\n---\n")
        self.assertIn(f'sdk_version: "{HARP_GRADIO_VERSION}"', new)

    def test_readme_noop_without_frontmatter(self):
        new, changes = reconcile_readme("# just a readme\n")
        self.assertEqual(changes, [])
        self.assertEqual(new, "# just a readme\n")


class FreezeMergeTest(unittest.TestCase):
    """Locking requirements to a known-good freeze closure."""

    FREEZE = (
        "gradio==6.3.0\n"
        "transformers==4.46.0\n"
        "tokenizers==0.20.1\n"
        "huggingface-hub==0.26.0\n"
        "numpy==2.1.0\n"
        "torch==2.4.0\n"
        "pyharp @ git+https://github.com/TEAMuP-dev/pyharp.git@v0.3.0\n"
        "-e git+https://example.com/x.git#egg=x\n"
    )

    def test_parse_freeze_skips_vcs_and_editable(self):
        frozen = parse_freeze(self.FREEZE)
        self.assertEqual(frozen["transformers"], ("transformers", "4.46.0"))
        self.assertEqual(frozen["huggingface-hub"], ("huggingface-hub", "0.26.0"))
        self.assertNotIn("x", frozen)
        self.assertNotIn("pyharp", frozen)

    def test_repins_declared_deps_to_frozen_versions(self):
        req = "transformers\ntokenizers==0.19.0\nsoundfile\n"
        new, changes = merge_frozen_pins(req, self.FREEZE)
        self.assertIn("transformers==4.46.0", new)
        self.assertIn("tokenizers==0.20.1", new)
        self.assertNotIn("0.19.0", new)
        self.assertTrue(any("tokenizers" in c for c in changes))

    def test_forces_gradio_and_ignores_frozen_gradio(self):
        new, _ = merge_frozen_pins("gradio==6.3.0\n", self.FREEZE)
        self.assertIn(f"gradio=={HARP_GRADIO_VERSION}", new)
        self.assertNotIn("gradio==6.3.0", new)
        self.assertIn("pyharp", new)

    def test_adds_critical_ml_libs_even_if_undeclared(self):
        # torch/transformers/tokenizers are in the allowlist; numpy is not (it is a
        # gradio dep, so we must not transplant gradio-6's numpy).
        new, _ = merge_frozen_pins("soundfile\n", self.FREEZE)
        self.assertIn("torch==2.4.0", new)
        self.assertIn("transformers==4.46.0", new)
        self.assertNotIn("numpy==2.1.0", new)
        self.assertNotIn("huggingface-hub==0.26.0", new)

    def test_canonicalizes_names(self):
        frozen = parse_freeze("Torch_Audio==2.4.0\n")
        self.assertIn("torch-audio", frozen)

    def test_strips_local_build_tag(self):
        frozen = parse_freeze("torch==2.4.0+cu121\n")
        self.assertEqual(frozen["torch"], ("torch", "2.4.0"))

    def test_keeps_gradio_coupled_dep_as_declared(self):
        # huggingface_hub is gradio-coupled: the freeze (gradio-6 era) has 0.26.0,
        # but the Space's own '>=0.20.0' is correct and resolvable with
        # gradio==5.28.0 + transformers. We must keep the declared constraint and
        # never transplant the frozen version.
        req = "huggingface_hub>=0.20.0\ntransformers\n"
        new, _ = merge_frozen_pins(req, self.FREEZE)
        self.assertIn("huggingface_hub>=0.20.0", new)
        self.assertNotIn("huggingface-hub==0.26.0", new)
        self.assertNotIn("huggingface_hub==0.26.0", new)
        # the genuinely model-critical lib is still locked
        self.assertIn("transformers==4.46.0", new)

    def test_preserves_numpy_upper_bound(self):
        # 'numpy<2.0.0' is a deliberate constraint for NumPy-1.x C-extensions;
        # it must survive untouched (NOT be repinned to the freeze's numpy 2.x,
        # NOT be loosened away).
        new, _ = merge_frozen_pins("numpy<2.0.0\n", self.FREEZE)
        self.assertIn("numpy<2.0.0", new)
        self.assertNotIn("numpy==2.1.0", new)


class DeployIntoSpaceTest(unittest.TestCase):
    """Overlaying onto an existing Space reconciles deps and uploads per-file."""

    @staticmethod
    def _fake_hub(calls: dict, files: dict):
        import tempfile
        import types

        module = types.ModuleType("huggingface_hub")
        tmpdir = tempfile.mkdtemp()

        def hf_hub_download(repo_id, filename, repo_type=None, token=None):
            if filename not in files:
                raise FileNotFoundError(filename)
            path = Path(tmpdir) / filename.replace("/", "_")
            path.write_text(files[filename], encoding="utf-8")
            return str(path)

        class FakeApi:
            def __init__(self, token=None):
                calls["token"] = token

            def upload_file(self, path_or_fileobj=None, path_in_repo=None, **_kwargs):
                calls.setdefault("uploads", {})[path_in_repo] = path_or_fileobj

        module.HfApi = FakeApi  # type: ignore[attr-defined]
        module.hf_hub_download = hf_hub_download  # type: ignore[attr-defined]
        return module

    def test_reconciles_and_uploads_per_file(self):
        import sys

        calls: dict = {}
        files = {
            "requirements.txt": "gradio==6.3.0\ntorch\n",
            "README.md": '---\nsdk: gradio\nsdk_version: "6.3.0"\napp_file: app.py\n---\n# x\n',
        }
        module = self._fake_hub(calls, files)
        with mock.patch.dict(sys.modules, {"huggingface_hub": module}):
            with tempfile.TemporaryDirectory() as tmp:
                pkg = Path(tmp) / "pkg"
                pkg.mkdir()
                (pkg / "app.py").write_text("print('wrapper')\n", encoding="utf-8")
                result = HarpModelAgent().deploy_into_space(pkg, "me/dup", token="tok")

        self.assertEqual(result["mode"], "into-space")
        uploads = calls["uploads"]
        self.assertEqual(set(uploads), {"app.py", "requirements.txt", "README.md"})

        req = uploads["requirements.txt"].decode("utf-8")
        self.assertIn(f"gradio=={HARP_GRADIO_VERSION}", req)
        self.assertNotIn("6.3.0", req)
        self.assertIn("pyharp", req)
        self.assertIn("torch", req)

        readme = uploads["README.md"].decode("utf-8")
        self.assertIn(f'sdk_version: "{HARP_GRADIO_VERSION}"', readme)

        # app.py uploaded verbatim from the package folder.
        self.assertEqual(uploads["app.py"], b"print('wrapper')\n")

    def test_requires_existing_space(self):
        import sys

        calls: dict = {}
        module = self._fake_hub(calls, {})  # download always raises -> Space absent
        with mock.patch.dict(sys.modules, {"huggingface_hub": module}):
            with tempfile.TemporaryDirectory() as tmp:
                pkg = Path(tmp) / "pkg"
                pkg.mkdir()
                (pkg / "app.py").write_text("x\n", encoding="utf-8")
                with self.assertRaises(DeploySpaceError):
                    HarpModelAgent().deploy_into_space(pkg, "me/dup", token="tok")

    def test_freeze_from_locks_closure(self):
        import sys

        calls: dict = {}
        files = {"requirements.txt": "transformers\ngradio==6.3.0\n"}
        module = self._fake_hub(calls, files)
        with mock.patch.dict(sys.modules, {"huggingface_hub": module}):
            with tempfile.TemporaryDirectory() as tmp:
                root = Path(tmp)
                pkg = root / "pkg"
                pkg.mkdir()
                (pkg / "app.py").write_text("print('x')\n", encoding="utf-8")
                freeze = root / "working.txt"
                freeze.write_text(
                    "transformers==4.46.0\ntokenizers==0.20.1\ngradio==6.3.0\n",
                    encoding="utf-8",
                )
                result = HarpModelAgent().deploy_into_space(
                    pkg, "me/dup", token="tok", freeze_from=freeze
                )

        req = calls["uploads"]["requirements.txt"].decode("utf-8")
        self.assertIn("transformers==4.46.0", req)
        self.assertIn("tokenizers==0.20.1", req)
        self.assertIn(f"gradio=={HARP_GRADIO_VERSION}", req)
        self.assertNotIn("gradio==6.3.0", req)
        self.assertTrue(any("transformers" in change for change in result["changes"]))

    def test_freeze_from_missing_file_raises(self):
        import sys

        calls: dict = {}
        files = {"requirements.txt": "torch\n"}
        module = self._fake_hub(calls, files)
        with mock.patch.dict(sys.modules, {"huggingface_hub": module}):
            with tempfile.TemporaryDirectory() as tmp:
                pkg = Path(tmp) / "pkg"
                pkg.mkdir()
                (pkg / "app.py").write_text("x\n", encoding="utf-8")
                with self.assertRaises(DeploySpaceError):
                    HarpModelAgent().deploy_into_space(
                        pkg, "me/dup", token="tok", freeze_from=Path(tmp) / "nope.txt"
                    )


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
        # GPU wrappers must import `spaces` defensively so they still run where
        # the Hugging Face-only package is absent (local smoke-tests, etc.).
        self.assertIn("except ImportError", app)
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
        # No leftover TODO for choices we resolved.
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


class _FakeProvider:
    """A provider stub that replays canned JSON responses (no network)."""

    name = "fake"
    model = "fake-1"

    def __init__(self, responses):
        self._responses = list(responses)
        self.calls = []

    def complete_json(self, system, user, *, schema=None):
        self.calls.append({"system": system, "user": user, "schema": schema})
        if not self._responses:
            raise AssertionError("no more canned responses")
        return self._responses.pop(0)


class LLMRecipeTest(unittest.TestCase):
    CARD = {
        "meta": {
            "id": "example/demucs",
            "author": "example",
            "pipeline_tag": "audio-to-audio",
            "library_name": "demucs",
            "license": "mit",
            "tags": ["audio-to-audio"],
        },
        "files": ["app.py", "model.th"],
        "readme": "Demucs separates a song into stems.",
    }

    VALID_RECIPE = {
        # No "model" key on purpose: the agent must backfill it from the card.
        "framework": {"import": "demucs", "pip": ["demucs"], "gpu": True},
        "inputs": [{"name": "input_audio", "type": "audio", "label": "In", "required": True}],
        "outputs": [{"name": "out", "type": "audio", "label": "Out"}],
        "inference": {"setup": "MODEL = None", "body": "return input_audio"},
    }

    def _context(self):
        return RecipeGenerationContext.from_card(
            self.CARD, target_inputs=["audio"], target_outputs=["audio"]
        )

    def test_from_card_extracts_grounding(self):
        context = self._context()
        self.assertEqual(context.model_id, "example/demucs")
        self.assertEqual(context.author, "example")
        self.assertEqual(context.pipeline_tag, "audio-to-audio")
        self.assertEqual(context.target_inputs, ["audio"])
        prompt = build_recipe_user_prompt(context)
        self.assertIn("example/demucs", prompt)
        self.assertIn("audio-to-audio", prompt)

    def test_generates_and_backfills_on_first_try(self):
        provider = _FakeProvider([self.VALID_RECIPE])
        draft = generate_recipe(self._context(), provider, max_repairs=2)

        self.assertEqual(draft.attempts, 1)
        self.assertEqual(draft.provider, "fake")
        # Model card fields were backfilled from the card.
        self.assertEqual(draft.recipe["model"]["id"], "example/demucs")
        self.assertEqual(draft.recipe["model"]["author"], "example")
        # The rendered wrapper compiles and reflects the recipe.
        compile(draft.app_py, "<llm>", "exec")
        self.assertIn("from pyharp import *", draft.app_py)
        self.assertIn("@spaces.GPU", draft.app_py)

    def test_repairs_an_invalid_first_response(self):
        invalid = {"framework": {}, "inputs": [], "outputs": []}  # missing inference + components
        provider = _FakeProvider([invalid, self.VALID_RECIPE])
        draft = generate_recipe(self._context(), provider, max_repairs=2)

        self.assertEqual(draft.attempts, 2)
        # The repair prompt fed the validation error back to the model.
        self.assertIn("failed validation", provider.calls[1]["user"])

    def test_raises_after_exhausting_repairs(self):
        invalid = {"inputs": [], "outputs": [], "inference": {}}
        provider = _FakeProvider([invalid, invalid])
        with self.assertRaises(LLMError):
            generate_recipe(self._context(), provider, max_repairs=1)

    def test_provider_from_env_requires_configuration(self):
        with mock.patch.dict(os.environ, {}, clear=True):
            with self.assertRaises(LLMError):
                provider_from_env()

    def test_provider_from_env_auto_detects_key(self):
        with mock.patch.dict(os.environ, {"OPENAI_API_KEY": "sk-test"}, clear=True):
            provider = provider_from_env()
            self.assertEqual(provider.name, "openai")

    def test_provider_from_env_generic_key_defaults_provider(self):
        with mock.patch.dict(os.environ, {"HARP_LLM_API_KEY": "generic"}, clear=True):
            provider = provider_from_env()
            self.assertEqual(provider.name, "gemini")
            self.assertEqual(provider.api_key, "generic")

    def test_provider_from_env_generic_key_honors_explicit_provider(self):
        with mock.patch.dict(os.environ, {"HARP_LLM_API_KEY": "generic"}, clear=True):
            provider = provider_from_env("anthropic")
            self.assertEqual(provider.name, "anthropic")
            self.assertEqual(provider.api_key, "generic")

    def test_gemini_default_model_is_not_retired_1_5(self):
        with mock.patch.dict(os.environ, {"GEMINI_API_KEY": "k"}, clear=True):
            provider = provider_from_env()
            self.assertEqual(provider.name, "gemini")
            self.assertNotIn("1.5", provider.model)

    def test_gemini_does_not_send_strict_response_schema(self):
        # A strict responseSchema makes Gemini strip undeclared fields off our
        # polymorphic components, yielding empty {} objects. JSON mode only.
        captured = {}

        def fake_post(url, payload, headers, timeout):
            captured["payload"] = payload
            return {"candidates": [{"content": {"parts": [{"text": "{}"}]}}]}

        with mock.patch("tools.model_agent.llm._http_post_json", side_effect=fake_post):
            provider = GeminiProvider(api_key="k", model="gemini-2.5-flash")
            provider.complete_json("sys", "user", schema={"type": "object"})

        config = captured["payload"]["generationConfig"]
        self.assertNotIn("responseSchema", config)
        self.assertEqual(config["responseMimeType"], "application/json")

    def test_gemini_list_models_filters_generate_content(self):
        payload = {
            "models": [
                {"name": "models/gemini-2.5-flash", "supportedGenerationMethods": ["generateContent"]},
                {"name": "models/embedding-001", "supportedGenerationMethods": ["embedContent"]},
            ]
        }
        with mock.patch("tools.model_agent.llm._http_get_json", return_value=payload):
            provider = GeminiProvider(api_key="k", model="gemini-2.5-flash")
            self.assertEqual(provider.list_models(), ["gemini-2.5-flash"])

    def test_default_examples_load_and_render(self):
        examples = default_examples()
        self.assertTrue(examples)
        for example in examples:
            render_app_from_recipe(example)


class CompleteRecipeTest(unittest.TestCase):
    def _scaffold(self):
        # Mimic scaffold-recipe output: resolved I/O + TODO stubs.
        record = analyze_app_source(AnalyzeTest.SAMPLE_APP)
        return recipe_skeleton_from_analysis(record, model_id="teamup-tech/demucs")

    def test_completion_fills_stubs_and_preserves_io(self):
        scaffold = self._scaffold()
        base_inputs = [(spec["name"], spec["type"]) for spec in scaffold["inputs"]]
        base_outputs = [(spec["name"], spec["type"]) for spec in scaffold["outputs"]]

        # The LLM tries to *change* the I/O contract and fill the glue; the
        # completion must keep the scaffold's I/O while taking the inference glue.
        llm_response = {
            "model": {"description": "Real description from the LLM."},
            "framework": {"import": "demucs", "pip": ["demucs", "torch"], "gpu": True},
            "inputs": [{"name": "rogue", "type": "textbox", "label": "Rogue"}],
            "outputs": [{"name": "rogue_out", "type": "audio", "label": "Rogue"}],
            "inference": {
                "setup": "import demucs\nMODEL = None",
                "body": "return input_audio, input_audio, LabelList().to_json()",
            },
        }
        provider = _FakeProvider([llm_response])
        draft = complete_recipe(scaffold, provider, max_repairs=1)

        self.assertEqual(draft.attempts, 1)
        # I/O contract preserved from the scaffold (not the LLM's rogue shapes).
        self.assertEqual([(s["name"], s["type"]) for s in draft.recipe["inputs"]], base_inputs)
        self.assertEqual([(s["name"], s["type"]) for s in draft.recipe["outputs"]], base_outputs)
        # Glue + framework taken from the LLM.
        self.assertEqual(draft.recipe["framework"]["pip"], ["demucs", "torch"])
        self.assertIn("import demucs", draft.recipe["inference"]["setup"])
        self.assertEqual(draft.recipe["model"]["description"], "Real description from the LLM.")
        # No scaffold meta leaks into the finished recipe.
        self.assertNotIn("_todo", draft.recipe)
        compile(draft.app_py, "<complete>", "exec")

    def test_completion_fills_dropdown_choices_when_scaffold_stubbed_them(self):
        # A scaffold whose dropdown choices could not be resolved statically.
        scaffold = {
            "_todo": ["inputs.mode.choices: set the real dropdown options"],
            "model": {"id": "ex/m", "name": "M"},
            "framework": {"import": "TODO", "pip": ["TODO"], "gpu": False},
            "inputs": [
                {"name": "audio", "type": "audio", "label": "In", "required": True},
                {
                    "name": "mode",
                    "type": "dropdown",
                    "label": "Mode",
                    "choices": ["TODO_option_1", "TODO_option_2"],
                },
            ],
            "outputs": [{"name": "out", "type": "audio", "label": "Out"}],
            "inference": {"setup": "x", "body": "y"},
        }
        llm_response = {
            "framework": {"import": "pkg", "pip": ["pkg"]},
            "inputs": [
                {"name": "audio", "type": "audio", "label": "In"},
                {"name": "mode", "type": "dropdown", "label": "Mode", "choices": ["fast", "slow"], "default": "fast"},
            ],
            "outputs": [{"name": "out", "type": "audio", "label": "Out"}],
            "inference": {"setup": "MODEL = None", "body": "return audio"},
        }
        provider = _FakeProvider([llm_response])
        draft = complete_recipe(scaffold, provider, max_repairs=1)

        mode = draft.recipe["inputs"][1]
        self.assertEqual(mode["choices"], ["fast", "slow"])
        self.assertEqual(mode["default"], "fast")


class ExampleRecipeFilesTest(unittest.TestCase):
    def test_committed_recipes_render(self):
        examples_dir = Path(__file__).resolve().parent.parent / "examples"
        recipe_files = sorted(examples_dir.glob("recipe_*.json"))
        self.assertTrue(recipe_files, "expected committed example recipes")
        for recipe_file in recipe_files:
            recipe = json.loads(recipe_file.read_text(encoding="utf-8"))
            app = render_app_from_recipe(recipe)
            compile(app, str(recipe_file), "exec")


class SoulXSingerRecipeTest(unittest.TestCase):
    """The hand-authored SoulX-Singer wrapper recipe must render correctly."""

    def _recipe(self):
        path = Path(__file__).resolve().parent.parent / "examples" / "soulx_singer_recipe.json"
        return json.loads(path.read_text(encoding="utf-8"))

    def test_renders_and_wraps_real_function(self):
        recipe = self._recipe()
        validate_recipe(recipe)
        app = render_app_from_recipe(recipe)
        compile(app, "soulx_singer_recipe.json", "exec")

        # Reuses the original Space's pipeline rather than reimplementing it.
        self.assertIn("from webui import synthesis_function", app)
        self.assertIn("synthesis_function(", app)
        # Correct two-audio interface (the whole point of the fix).
        self.assertEqual(app.count('gr.Audio(type="filepath"'), 3)  # 2 inputs + 1 output
        self.assertIn(
            "def process_fn(prompt_audio, target_audio, control, prompt_lyric_lang, "
            "target_lyric_lang, prompt_vocal_sep, target_vocal_sep, auto_shift, pitch_shift):",
            app,
        )
        # gpu false -> must NOT add a second @spaces.GPU (synthesis_function has its own).
        self.assertNotIn("@spaces.GPU", app)

    def test_mirrors_original_ui_defaults_for_lyric_path(self):
        app = render_app_from_recipe(self._recipe())
        # Language + vocal-separation flags are passed through (NOT left to
        # synthesis_function's own defaults), so the lyric/ASR path matches the
        # original UI: prompt_vocal_sep defaults False, target_vocal_sep True,
        # languages default English.
        self.assertIn("prompt_lyric_lang=prompt_lyric_lang", app)
        self.assertIn("target_lyric_lang=target_lyric_lang", app)
        self.assertIn("prompt_vocal_sep=prompt_vocal_sep", app)
        self.assertIn("target_vocal_sep=target_vocal_sep", app)
        # Wrapping the single entry point cleanly => no lint warnings.
        self.assertEqual(lint_generated_app(app), [])


class LintGeneratedAppTest(unittest.TestCase):
    """The generated-wrapper linter flags pipeline-reimplementation anti-patterns."""

    def test_flags_multistage_reimplementation(self):
        source = (
            "ok, msg = app_state.run_preprocess(prompt_path=p, target_path=t)\n"
            "ok, msg, out = app_state.run_svs(control=control, session_base=s)\n"
        )
        warnings = lint_generated_app(source)
        self.assertTrue(warnings)
        self.assertTrue(any("re-implement" in w for w in warnings))

    def test_flags_sr_none_load(self):
        source = "y, sr = librosa.load(prompt_audio, sr=None)\n"
        warnings = lint_generated_app(source)
        self.assertTrue(any("sr=None" in w for w in warnings))

    def test_clean_single_entry_wrapper_has_no_warnings(self):
        source = (
            "from webui import synthesis_function\n"
            "merged, _p, _t = synthesis_function(prompt_audio, target_audio)\n"
            "return merged\n"
        )
        self.assertEqual(lint_generated_app(source), [])


class SpaceSourceTest(unittest.TestCase):
    """fetch_space_sources crawls the entry file and its first-party imports."""

    class _FakeScraper:
        FILES = {
            "app.py": (
                "import os\n"
                "import gradio as gr\n"
                "from ensure_models import ensure_pretrained_models\n"
                "from webui import synthesis_function\n"
            ),
            "ensure_models.py": "def ensure_pretrained_models():\n    pass\n",
            "webui.py": (
                "import torch\n"
                "import numpy as np\n"
                "from cli.inference import process\n"
                "def synthesis_function(a, b):\n    return None\n"
            ),
            "cli/inference.py": "def process(*a, **k):\n    return None\n",
        }

        def get_space_file(self, space_id, filename="app.py"):
            return self.FILES.get(filename)

    def test_crawls_first_party_modules_and_skips_libs(self):
        agent = HarpModelAgent(scraper=self._FakeScraper())
        sources = agent.fetch_space_sources("me/space")

        # app.py + the first-party modules it (transitively) imports.
        self.assertIn("app.py", sources)
        self.assertIn("ensure_models.py", sources)
        self.assertIn("webui.py", sources)
        self.assertIn("cli/inference.py", sources)
        # stdlib (os) and third-party (gradio, torch, numpy) are never fetched.
        self.assertNotIn("os.py", sources)
        self.assertNotIn("gradio.py", sources)
        self.assertNotIn("torch.py", sources)

    def test_respects_max_files(self):
        agent = HarpModelAgent(scraper=self._FakeScraper())
        sources = agent.fetch_space_sources("me/space", max_files=2)
        self.assertEqual(len(sources), 2)
        self.assertIn("app.py", sources)


class SpaceGroundingPromptTest(unittest.TestCase):
    def test_space_sources_appear_in_prompt(self):
        context = RecipeGenerationContext(
            model_id="me/model",
            readme="A model.",
            space_sources={"webui.py": "def synthesis_function(a, b):\n    return None\n"},
        )
        prompt = build_recipe_user_prompt(context)
        self.assertIn("Original Space source", prompt)
        self.assertIn("## webui.py", prompt)
        self.assertIn("synthesis_function", prompt)

    def test_no_space_section_without_sources(self):
        context = RecipeGenerationContext(model_id="me/model", readme="A model.")
        prompt = build_recipe_user_prompt(context)
        self.assertNotIn("Original Space source", prompt)


class GitHubUrlParseTest(unittest.TestCase):
    def test_parses_common_shapes(self):
        self.assertEqual(_parse_github_url("https://github.com/o/r"), ("o", "r", None, ""))
        self.assertEqual(_parse_github_url("https://github.com/o/r.git"), ("o", "r", None, ""))
        self.assertEqual(_parse_github_url("http://www.github.com/o/r/"), ("o", "r", None, ""))
        self.assertEqual(_parse_github_url("git@github.com:o/r.git"), ("o", "r", None, ""))
        self.assertEqual(_parse_github_url("o/r"), ("o", "r", None, ""))

    def test_parses_tree_and_blob_refs(self):
        self.assertEqual(
            _parse_github_url("https://github.com/o/r/tree/dev/sub/dir"),
            ("o", "r", "dev", "sub/dir"),
        )
        self.assertEqual(
            _parse_github_url("https://github.com/o/r/blob/main/pkg/model.py"),
            ("o", "r", "main", "pkg/model.py"),
        )

    def test_rejects_incomplete(self):
        with self.assertRaises(ValueError):
            _parse_github_url("https://github.com/just-owner")


class _FakeGitHubScraper:
    TREE = [
        "app.py",
        "pkg/__init__.py",
        "pkg/model.py",
        "pkg/utils.py",
        "tests/test_x.py",
        "scripts/run.py",
        "README.md",
        "requirements.txt",
    ]
    FILES = {
        "app.py": "import gradio as gr\nfrom pkg.model import load_model\nimport numpy as np\n",
        "pkg/model.py": "from pkg.utils import helper\nimport torch\n",
        "pkg/utils.py": "def helper():\n    return 1\n",
        "README.md": "# Cool Model\n\nDoes audio things.\n",
    }

    def get_repo_info(self, owner, repo):
        return {
            "default_branch": "main",
            "topics": ["audio", "tts"],
            "license": {"spdx_id": "MIT"},
        }

    def list_tree(self, owner, repo, ref):
        return list(self.TREE)

    def get_file(self, owner, repo, ref, path):
        return self.FILES.get(path)


class GitHubSourceTest(unittest.TestCase):
    """fetch_github_sources crawls entry/inference files and their imports."""

    def _agent(self):
        return HarpModelAgent(github_scraper=_FakeGitHubScraper())

    def test_crawls_first_party_modules_and_skips_libs_and_tests(self):
        sources = self._agent().fetch_github_sources("owner/repo")
        self.assertIn("app.py", sources)
        self.assertIn("pkg/model.py", sources)  # imported by app.py
        self.assertIn("pkg/utils.py", sources)  # imported by pkg/model.py
        self.assertNotIn("tests/test_x.py", sources)  # test dir, never imported

    def test_respects_max_files(self):
        sources = self._agent().fetch_github_sources("owner/repo", max_files=2)
        self.assertLessEqual(len(sources), 2)
        self.assertIn("app.py", sources)

    def test_get_github_card_shape(self):
        card = self._agent().get_github_card("https://github.com/owner/repo")
        self.assertEqual(card["meta"]["id"], "owner/repo")
        self.assertEqual(card["meta"]["name"], "repo")
        self.assertEqual(card["meta"]["author"], "owner")
        self.assertEqual(card["meta"]["license"], "MIT")
        self.assertIn("audio", card["meta"]["tags"])
        self.assertIn("Cool Model", card["readme"])
        self.assertIn("app.py", card["files"])

    def test_resolve_and_pip_requirement(self):
        agent = self._agent()
        self.assertEqual(agent.resolve_github_target("owner/repo"), ("owner", "repo", "main"))
        self.assertEqual(
            agent.github_pip_requirement("owner/repo"),
            "git+https://github.com/owner/repo.git@main",
        )
        # An explicit ref skips the default-branch lookup.
        self.assertEqual(
            agent.github_pip_requirement("owner/repo", ref="v1.2.0"),
            "git+https://github.com/owner/repo.git@v1.2.0",
        )


class GitHubGroundingPromptTest(unittest.TestCase):
    def test_github_sources_use_github_framing(self):
        context = RecipeGenerationContext(
            model_id="owner/repo",
            readme="A model.",
            space_sources={"pkg/model.py": "def load_model():\n    return None\n"},
            grounding_origin="github",
            source_repo_url="git+https://github.com/owner/repo.git@main",
        )
        prompt = build_recipe_user_prompt(context)
        self.assertIn("Upstream GitHub source", prompt)
        self.assertIn("git+https://github.com/owner/repo.git@main", prompt)
        self.assertIn("## pkg/model.py", prompt)
        # GitHub grounding must NOT claim the wrapper is deployed into a Space.
        self.assertNotIn("Original Space source", prompt)


class EnsureGitHubPipTest(unittest.TestCase):
    REQUIREMENT = "git+https://github.com/owner/repo.git@main"

    def test_injects_missing_requirement_first(self):
        recipe = {"framework": {"pip": ["numpy"]}}
        _ensure_github_pip(recipe, self.REQUIREMENT)
        self.assertEqual(recipe["framework"]["pip"], [self.REQUIREMENT, "numpy"])

    def test_creates_framework_and_pip_when_absent(self):
        recipe = {}
        _ensure_github_pip(recipe, self.REQUIREMENT)
        self.assertEqual(recipe["framework"]["pip"], [self.REQUIREMENT])

    def test_keeps_existing_pin_for_same_repo(self):
        # The LLM already listed the repo with its own ref; do not duplicate.
        recipe = {"framework": {"pip": ["git+https://github.com/owner/repo.git@v2", "numpy"]}}
        _ensure_github_pip(recipe, self.REQUIREMENT)
        self.assertEqual(
            recipe["framework"]["pip"],
            ["git+https://github.com/owner/repo.git@v2", "numpy"],
        )

    def test_noop_without_requirement(self):
        recipe = {"framework": {"pip": ["numpy"]}}
        _ensure_github_pip(recipe, "")
        self.assertEqual(recipe["framework"]["pip"], ["numpy"])


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
