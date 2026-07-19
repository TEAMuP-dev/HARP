import json
import os
import subprocess
import sys
import tempfile
import unittest
import io
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from unittest import mock

from tools.model_agent.agent import (
    HARP_GRADIO_VERSION,
    PYHARP_REQUIREMENT,
    DeploySpaceError,
    EndpointProbeError,
    HarpEndpointClient,
    HarpModelAgent,
    SpaceCandidate,
    VenvSetupError,
    _parse_github_url,
    lint_generated_app,
    merge_frozen_pins,
    parse_freeze,
    reconcile_readme,
    reconcile_requirements,
)
from tools.model_agent.cli import (
    _apply_card_metadata,
    _ensure_github_backend_pip,
    _ensure_github_pip,
    _pip_installable_from_signals,
    _repo_is_pip_installable,
    _run_deploy,
    _strip_repo_backend_pip,
    _strip_repo_pip,
    build_parser,
)
from tools.model_agent.llm import (
    GeminiProvider,
    LLMError,
    RecipeGenerationContext,
    build_recipe_user_prompt,
    build_remote_refine_prompt,
    complete_recipe,
    default_examples,
    generate_recipe,
    pick_remote_endpoint,
    provider_from_env,
    refine_remote_recipe,
)
from tools.model_agent.recipe import (
    RecipeError,
    _satisfies,
    apply_dependency_fixes,
    build_package_from_recipe,
    collect_pip_requirements,
    component_props_for_endpoint,
    find_dependency_conflicts,
    guess_primary_endpoint,
    lint_recipe_requirements,
    rank_named_endpoints,
    remote_recipe_from_api_info,
    render_app_from_recipe,
    validate_recipe,
)
from tools.model_agent.knowledge import (
    KnowledgeBase,
    dependency_fingerprint,
    fingerprint_for_recipe,
    match_repair_rules,
)
from tools.model_agent.classifier import (
    analyze_signals,
    classify,
    detect_resource_warnings,
    recommend_mode,
    resource_headsup,
)
from tools.model_agent.orchestrator import (
    build_steps,
    decide_plan,
    detect_ref,
    extract_space_links,
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
        "pyharp @ git+https://github.com/TEAMuP-dev/pyharp.git@develop\n"
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
                (pkg / "app.py").write_bytes(b"print('wrapper')\n")
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

    def test_file_component_defaults_file_types_when_unset(self):
        # pyharp's get_harp_component crashes (TypeError) on a gr.File whose
        # file_types is None, so a recipe that omits file_types must still render
        # a gr.File with a concrete file_types list.
        recipe = {
            "model": {"id": "example/m", "name": "M"},
            "inputs": [{"name": "melody", "type": "file", "label": "Melody"}],
            "outputs": [{"name": "out", "type": "audio", "label": "Out"}],
            "inference": {"body": "return None"},
        }
        app = render_app_from_recipe(recipe)
        compile(app, "<file-default>", "exec")
        self.assertIn(
            'gr.File(type="filepath", label="Melody", file_types=[".mid", ".midi"])',
            app,
        )
        self.assertNotIn("gr.File(type=\"filepath\", label=\"Melody\")", app)

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
    DUAL_RECIPE = {
        # No "model" key on purpose: the agent must backfill it from the card.
        "framework": {
            "dual": {
                "backend_python": "3.10",
                "apt": ["ffmpeg"],
                "backend_pip": ["soundfile"],
                "worker": {
                    "imports": "import os",
                    "body": "outputs = {'out': inputs['input_audio']}",
                },
            }
        },
        "inputs": [{"name": "input_audio", "type": "audio", "label": "In", "required": True}],
        "outputs": [{"name": "out", "type": "audio", "label": "Out"}],
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

    def test_backend_recipe_marks_framework_and_omits_pyharp(self):
        provider = _FakeProvider([self.VALID_RECIPE])
        draft = generate_recipe(self._context(), provider, max_repairs=2, backend=True)

        # The recipe is flagged so the renderer emits a plain-Gradio backend.
        self.assertTrue(draft.recipe["framework"]["backend"])
        # The backend prompt (not the pyharp one) was used, and the rendered app
        # is a plain gr.Interface with no pyharp import.
        self.assertIn("DOES NOT USE PYHARP", provider.calls[0]["system"])
        self.assertIn("PLAIN-GRADIO BACKEND", provider.calls[0]["user"])
        self.assertNotIn("from pyharp import *", draft.app_py)
        self.assertIn("gr.Interface", draft.app_py)
        compile(draft.app_py, "<llm-backend>", "exec")

    def test_dual_recipe_generates_docker_frontend_and_worker(self):
        provider = _FakeProvider([self.DUAL_RECIPE])
        draft = generate_recipe(self._context(), provider, max_repairs=2, dual=True)

        self.assertEqual(draft.recipe["model"]["id"], "example/demucs")
        self.assertIn("dual-interpreter", provider.calls[0]["system"])
        self.assertIn("DUAL-INTERPRETER DOCKER", provider.calls[0]["user"])
        self.assertIn("_call_backend", draft.app_py)
        package = build_package_from_recipe(draft.recipe)
        self.assertEqual(package.framework, "dual-interpreter")
        self.assertIn("frontend_app.py", package.extra_files)
        self.assertIn("backend_worker.py", package.extra_files)
        self.assertNotIn("pyharp", "\n".join(draft.recipe["framework"]["dual"]["backend_pip"]))
        compile(package.extra_files["frontend_app.py"], "<dual-frontend>", "exec")
        compile(package.extra_files["backend_worker.py"], "<dual-worker>", "exec")

    def test_dual_prompt_points_dependencies_at_backend_pip(self):
        context = RecipeGenerationContext(
            model_id="owner/repo",
            readme="A model.",
            dependency_manifests={"requirements.txt": "httpx<0.20\nsoundfile\n"},
        )
        prompt = build_recipe_user_prompt(context, dual=True)
        self.assertIn("framework.dual.backend_pip", prompt)
        self.assertIn("Do not include pyharp or gradio in backend_pip", prompt)

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
        with mock.patch.dict(os.environ, {"GOOGLE_API_KEY": "sk-test"}, clear=True):
            provider = provider_from_env()
            self.assertEqual(provider.name, "gemini")

    def test_provider_from_env_generic_key_defaults_provider(self):
        with mock.patch.dict(os.environ, {"HARP_LLM_API_KEY": "generic"}, clear=True):
            provider = provider_from_env()
            self.assertEqual(provider.name, "gemini")
            self.assertEqual(provider.api_key, "generic")

    def test_provider_from_env_generic_key_honors_explicit_provider(self):
        with mock.patch.dict(os.environ, {"HARP_LLM_API_KEY": "generic"}, clear=True):
            provider = provider_from_env("gemini")
            self.assertEqual(provider.name, "gemini")
            self.assertEqual(provider.api_key, "generic")

    def test_provider_from_env_rejects_unknown_provider(self):
        with mock.patch.dict(os.environ, {"HARP_LLM_API_KEY": "generic"}, clear=True):
            with self.assertRaises(LLMError):
                provider_from_env("anthropic")

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
        # A scaffolded recipe: resolved I/O + TODO stubs for framework/inference.
        return {
            "_todo": [
                "framework.import / framework.pip: set the real package(s)",
                "inference.setup / inference.body: fill in model loading and inference",
            ],
            "model": {
                "id": "teamup-tech/demucs",
                "name": "Demucs",
                "description": "TODO: describe this model.",
                "author": "teamup-tech",
                "tags": [],
            },
            "framework": {
                "import": "TODO_package",
                "pip": ["TODO-package"],
                "apt": [],
                "gpu": True,
            },
            "inputs": [
                {"name": "input_audio", "type": "audio", "label": "Input Audio", "required": True},
                {
                    "name": "model",
                    "type": "dropdown",
                    "label": "Model",
                    "choices": ["a", "b"],
                    "default": "a",
                },
            ],
            "outputs": [
                {"name": "drums", "type": "audio", "label": "Drums"},
                {"name": "bass", "type": "audio", "label": "Bass"},
                {"name": "labels", "type": "labels", "label": "Labels"},
            ],
            "inference": {
                "setup": "# TODO: import the model package and load the model here.",
                "body": 'raise NotImplementedError("Fill in the inference body.")',
            },
        }

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


class RemoteRecipeRenderTest(unittest.TestCase):
    RECIPE = {
        "model": {"id": "owner/backend", "name": "Backend", "description": "d", "author": "owner"},
        "framework": {
            "gpu": False,
            "pip": [],
            "remote": {
                "space": "owner/backend",
                "api_name": "/synthesis_function",
                "token_env": "HF_TOKEN",
                "args": [
                    {"from": "prompt_audio", "file": True},
                    {"const": None},
                    {"from": "steps", "cast": "int"},
                ],
                "returns": [{"index": 0, "to": "generated"}],
            },
        },
        "inputs": [
            {"name": "prompt_audio", "type": "audio", "label": "Ref", "required": True},
            {"name": "steps", "type": "number", "label": "Steps", "default": 10},
        ],
        "outputs": [{"name": "generated", "type": "audio", "label": "Out"}],
    }

    def test_renders_valid_proxy_app(self):
        app = render_app_from_recipe(self.RECIPE)
        compile(app, "<remote-app>", "exec")
        # Proxies via gradio_client, never imports the model or spaces GPU shim.
        self.assertIn("from gradio_client import Client, handle_file", app)
        self.assertNotIn("import spaces", app)
        self.assertIn('_BACKEND_SPACE = "owner/backend"', app)
        self.assertIn('api_name="/synthesis_function"', app)
        # Arg mapping: file wrapped, const emitted, cast applied.
        self.assertIn("handle_file(prompt_audio)", app)
        # Optional file inputs arrive as None; handle_file(None) would raise, so
        # the wrapped call must forward None instead of crashing.
        self.assertIn("(handle_file(prompt_audio) if prompt_audio else None)", app)
        self.assertIn("        None,", app)
        self.assertIn("int(steps)", app)
        # Return mapping + guard for the media output.
        self.assertIn("_out_generated = _values[0]", app)
        self.assertIn("return _out_generated", app)

    def test_opaque_backend_error_gets_actionable_hint(self):
        # MusicGen regression: the backend hides its exception behind a generic
        # "Internal Gradio error"; the frontend must point at where the real cause
        # lives instead of parroting the useless message.
        api_info = {
            "named_endpoints": {
                "/predict_batched": {
                    "parameters": [{"component": "Textbox"}, {"component": "Audio"}],
                    "returns": [{"component": "Audio"}],
                }
            }
        }
        recipe = remote_recipe_from_api_info("facebook/MusicGen", api_info, user_token=True)
        app = render_app_from_recipe(recipe)
        compile(app, "<hint>", "exec")
        ns = {"_ACCEPT_USER_TOKEN": True, "_BACKEND_SPACE": "facebook/MusicGen"}
        start = app.index("def _quota_hint(")
        end = app.index("\n\n\n", start)
        exec(app[start:end], ns)
        hint = ns["_quota_hint"]
        opaque = hint("Internal Gradio error")
        self.assertIn("Logs", opaque)
        self.assertIn("facebook/MusicGen", opaque)
        self.assertIn("token", opaque.lower())
        # An empty backend message is treated the same (also opaque).
        self.assertIn("Logs", hint(""))
        # A real, descriptive error is passed through unchanged.
        self.assertEqual(hint("File not found: foo.wav"), "File not found: foo.wav")
        # Quota errors keep their dedicated message.
        self.assertIn("quota", hint("You exceeded your ZeroGPU quota").lower())

    def test_wakes_and_retries_a_sleeping_backend(self):
        app = render_app_from_recipe(self.RECIPE)
        compile(app, "<remote-wake>", "exec")
        # A sleeping backend is woken + retried instead of failing the first hit.
        self.assertIn("def _wake_backend()", app)
        self.assertIn("def _is_cold_start(message)", app)
        self.assertIn("read operation timed out", app)
        self.assertIn(".hf.space/", app)
        self.assertIn("for _attempt in range(_CALL_RETRIES + 1)", app)
        self.assertIn("_reset_client()", app)
        # The retry only fires for cold-start-style errors, not real app errors.
        self.assertIn("if _attempt < _CALL_RETRIES and _is_cold_start(str(_exc)):", app)

    def test_requirements_are_conflict_free(self):
        package = build_package_from_recipe(self.RECIPE)
        self.assertIn("gradio_client", package.requirements)
        self.assertIn("pyharp", package.requirements)
        # The whole point: none of the backend's (conflicting) deps are installed.
        self.assertNotIn("nemo", package.requirements)
        self.assertNotIn("torch", package.requirements)
        self.assertEqual(package.manifest.get("deploy_mode"), "remote-backend")
        self.assertEqual(package.manifest.get("backend_space"), "owner/backend")

    def test_multiple_outputs_return_tuple(self):
        recipe = json.loads(json.dumps(self.RECIPE))
        recipe["outputs"].append({"name": "meta", "type": "labels", "label": "Meta"})
        recipe["framework"]["remote"]["returns"].append({"index": 1, "to": "meta"})
        app = render_app_from_recipe(recipe)
        compile(app, "<remote-multi>", "exec")
        self.assertIn("return _out_generated, _out_meta", app)


class RemoteRecipeValidationTest(unittest.TestCase):
    def _base(self):
        return json.loads(json.dumps(RemoteRecipeRenderTest.RECIPE))

    def test_valid_recipe_passes(self):
        validate_recipe(self._base())  # does not raise

    def test_inference_not_required_for_remote(self):
        recipe = self._base()
        self.assertNotIn("inference", recipe)
        validate_recipe(recipe)

    def test_missing_space_fails(self):
        recipe = self._base()
        recipe["framework"]["remote"]["space"] = ""
        with self.assertRaises(RecipeError):
            validate_recipe(recipe)

    def test_missing_api_name_fails(self):
        recipe = self._base()
        del recipe["framework"]["remote"]["api_name"]
        with self.assertRaises(RecipeError):
            validate_recipe(recipe)

    def test_arg_from_must_reference_declared_input(self):
        recipe = self._base()
        recipe["framework"]["remote"]["args"][0] = {"from": "does_not_exist", "file": True}
        with self.assertRaises(RecipeError):
            validate_recipe(recipe)

    def test_arg_needs_exactly_one_of_from_or_const(self):
        recipe = self._base()
        recipe["framework"]["remote"]["args"][1] = {"from": "steps", "const": None}
        with self.assertRaises(RecipeError):
            validate_recipe(recipe)

    def test_returns_must_cover_every_output(self):
        recipe = self._base()
        recipe["outputs"].append({"name": "extra", "type": "audio", "label": "Extra"})
        with self.assertRaises(RecipeError):
            validate_recipe(recipe)

    def test_bad_cast_fails(self):
        recipe = self._base()
        recipe["framework"]["remote"]["args"][2]["cast"] = "complex"
        with self.assertRaises(RecipeError):
            validate_recipe(recipe)


class RemoteScaffoldTest(unittest.TestCase):
    API_INFO = {
        "named_endpoints": {
            "/synthesis_function": {
                "parameters": [
                    {
                        "label": "Reference",
                        "parameter_name": "prompt_audio",
                        "component": "Audio",
                        "parameter_default": None,
                    },
                    {
                        "label": "Steps",
                        "parameter_name": "steps",
                        "component": "Slider",
                        "parameter_default": 10,
                    },
                    {"label": "Session", "component": "State", "parameter_default": None},
                    {
                        "label": "Prompt",
                        "parameter_name": "prompt",
                        "component": "Textbox",
                        "parameter_default": "hi",
                    },
                ],
                "returns": [
                    {"label": "Output", "component": "Audio"},
                    {"label": "Debug", "component": "State"},
                ],
            }
        },
        "unnamed_endpoints": {},
    }

    def test_scaffolds_valid_renderable_recipe(self):
        recipe = remote_recipe_from_api_info("owner/backend", self.API_INFO)
        validate_recipe(recipe)
        compile(render_app_from_recipe(recipe), "<scaffold>", "exec")

        remote = recipe["framework"]["remote"]
        self.assertEqual(remote["api_name"], "/synthesis_function")
        # Audio param -> input + file arg; unmapped State -> const placeholder.
        self.assertEqual(
            remote["args"],
            [
                {"from": "prompt_audio", "file": True},
                {"from": "steps"},
                {"const": None},
                {"from": "prompt"},
            ],
        )
        names = [i["name"] for i in recipe["inputs"]]
        self.assertEqual(names, ["prompt_audio", "steps", "prompt"])
        # Only the Audio return is a recognized HARP output.
        self.assertEqual(recipe["outputs"][0]["type"], "audio")
        self.assertEqual(remote["returns"], [{"index": 0, "to": "output"}])

    def test_scaffolded_dropdown_and_slider_defaults_stay_in_range(self):
        # DiffRhythm2 regression: /info gives a dropdown's default ("mp3") but not
        # its choices, and sliders' defaults (16, 1.3) but not their ranges. The
        # scaffold must still render a valid app -- a default outside choices /
        # slider range makes Gradio raise at launch.
        info = {
            "named_endpoints": {
                "/infer": {
                    "parameters": [
                        {
                            "label": "Output Format",
                            "parameter_name": "file_type",
                            "component": "Dropdown",
                            "parameter_default": "mp3",
                        },
                        {
                            "label": "Diffusion Steps",
                            "parameter_name": "steps",
                            "component": "Slider",
                            "parameter_default": 16,
                        },
                        {
                            "label": "CFG Strength",
                            "parameter_name": "cfg_strength",
                            "component": "Slider",
                            "parameter_default": 1.3,
                        },
                    ],
                    "returns": [{"label": "Audio", "component": "Audio"}],
                }
            },
            "unnamed_endpoints": {},
        }
        recipe = remote_recipe_from_api_info("owner/backend", info)
        validate_recipe(recipe)
        compile(render_app_from_recipe(recipe), "<scaffold>", "exec")

        by_name = {spec["name"]: spec for spec in recipe["inputs"]}
        dropdown = by_name["file_type"]
        self.assertIn(dropdown["default"], dropdown["choices"])
        for slider_name in ("steps", "cfg_strength"):
            slider = by_name[slider_name]
            self.assertLessEqual(slider["min"], slider["default"])
            self.assertLessEqual(slider["default"], slider["max"])

    def test_renderer_recovers_dropdown_default_missing_from_choices(self):
        # Even a hand-written / LLM recipe whose dropdown default isn't among the
        # choices must render (the default is surfaced as a selectable choice).
        recipe = remote_recipe_from_api_info("owner/backend", self.API_INFO)
        recipe["inputs"].append(
            {
                "name": "fmt",
                "type": "dropdown",
                "label": "Format",
                "choices": ["a", "b"],
                "default": "mp3",
            }
        )
        recipe["framework"]["remote"]["args"].append({"const": "mp3"})
        app = render_app_from_recipe(recipe)
        compile(app, "<recover>", "exec")
        self.assertIn('gr.Dropdown(choices=["mp3", "a", "b"], value="mp3"', app)

    def test_default_scaffold_has_no_user_token_field(self):
        app = render_app_from_recipe(remote_recipe_from_api_info("owner/backend", self.API_INFO))
        self.assertNotIn('type="password"', app)
        self.assertNotIn("_hf_user_token", app)

    def test_user_token_adds_masked_field_and_per_user_client(self):
        recipe = remote_recipe_from_api_info("owner/backend", self.API_INFO, user_token=True)
        validate_recipe(recipe)
        self.assertTrue(recipe["framework"]["remote"]["user_token"])
        # The token control does NOT add a positional backend arg (args unchanged).
        self.assertEqual(
            recipe["framework"]["remote"]["args"],
            [
                {"from": "prompt_audio", "file": True},
                {"from": "steps"},
                {"const": None},
                {"from": "prompt"},
            ],
        )
        app = render_app_from_recipe(recipe)
        compile(app, "<user-token>", "exec")
        # Masked, optional token control appended as the LAST input.
        self.assertIn('type="password"', app)
        self.assertIn("_hf_user_token=''", app)
        # A user token gets a fresh per-call client (never the shared cache),
        # created in _make_conn from the per-call token.
        self.assertIn("_tok = (_hf_user_token or '').strip()", app)
        self.assertIn("Client(_BACKEND_SPACE, hf_token=tok)", app)
        self.assertIn("_ACCEPT_USER_TOKEN = True", app)
        # Quota errors are turned into an actionable, token-free hint.
        self.assertIn("_quota_hint", app)
        # The token is not forwarded into the backend predict() args.
        predict_region = app.split("_conn.predict(")[1].split("api_name=")[0]
        self.assertNotIn("_hf_user_token", predict_region)

    def test_user_token_must_be_boolean(self):
        recipe = remote_recipe_from_api_info("owner/backend", self.API_INFO)
        recipe["framework"]["remote"]["user_token"] = "yes"
        with self.assertRaises(RecipeError):
            validate_recipe(recipe)

    def test_requires_api_name_when_multiple_endpoints(self):
        info = {
            "named_endpoints": {"/a": {"parameters": [], "returns": []}, "/b": {}},
            "unnamed_endpoints": {},
        }
        with self.assertRaises(RecipeError):
            remote_recipe_from_api_info("owner/backend", info)

    def test_no_named_endpoints_raises(self):
        with self.assertRaises(RecipeError):
            remote_recipe_from_api_info(
                "owner/backend", {"named_endpoints": {}, "unnamed_endpoints": {}}
            )

    def test_agent_scaffold_uses_endpoint_client(self):
        class _FakeEndpoint:
            def fetch_api_info(self, space):
                return RemoteScaffoldTest.API_INFO

            def resolve_canonical_path(self, space):
                return "owner/backend"

        agent = HarpModelAgent(endpoint_client=_FakeEndpoint())
        recipe = agent.scaffold_remote_recipe("owner/backend")
        validate_recipe(recipe)

    # --- live-config enrichment (DiffRhythm2: real choices + slider bounds) ---

    CONFIG_INFO = {
        "named_endpoints": {
            "/infer": {
                "parameters": [
                    {
                        "label": "Output Format",
                        "parameter_name": "file_type",
                        "component": "Dropdown",
                        "parameter_default": "mp3",
                    },
                    {
                        "label": "Diffusion Steps",
                        "parameter_name": "steps",
                        "component": "Slider",
                        "parameter_default": 16,
                    },
                ],
                "returns": [{"label": "Audio", "component": "Audio"}],
            }
        },
        "unnamed_endpoints": {},
    }

    CONFIG = {
        "components": [
            {"id": 7, "type": "dropdown", "props": {"choices": [["MP3", "mp3"], ["WAV", "wav"]], "value": "mp3"}},
            {"id": 8, "type": "slider", "props": {"minimum": 1, "maximum": 100, "step": 1, "value": 16}},
        ],
        "dependencies": [
            {"api_name": "infer", "inputs": [7, 8], "outputs": [9]},
        ],
    }

    def test_component_props_for_endpoint_maps_inputs_in_order(self):
        props = component_props_for_endpoint(self.CONFIG, "/infer")
        self.assertEqual(len(props), 2)
        self.assertEqual(props[0]["choices"], [["MP3", "mp3"], ["WAV", "wav"]])
        self.assertEqual(props[1]["maximum"], 100)

    def test_component_props_for_endpoint_returns_none_when_unmatched(self):
        self.assertIsNone(component_props_for_endpoint(self.CONFIG, "/nope"))
        self.assertIsNone(component_props_for_endpoint({}, "/infer"))

    def test_scaffold_uses_real_choices_and_slider_bounds_from_config(self):
        props = component_props_for_endpoint(self.CONFIG, "/infer")
        recipe = remote_recipe_from_api_info(
            "owner/backend", self.CONFIG_INFO, component_props=props
        )
        validate_recipe(recipe)
        compile(render_app_from_recipe(recipe), "<config>", "exec")

        by_name = {spec["name"]: spec for spec in recipe["inputs"]}
        # Real option values (unwrapped from [label, value] pairs), default kept.
        self.assertEqual(by_name["file_type"]["choices"], ["mp3", "wav"])
        self.assertEqual(by_name["file_type"]["default"], "mp3")
        # Real slider bounds, not the guessed placeholder range.
        self.assertEqual(by_name["steps"]["min"], 1)
        self.assertEqual(by_name["steps"]["max"], 100)
        self.assertEqual(by_name["steps"]["default"], 16)
        # A resolved control no longer needs a _todo.
        todo = " ".join(recipe.get("_todo", []))
        self.assertNotIn("file_type", todo)
        self.assertNotIn("steps:", todo)

    def test_agent_scaffold_enriches_from_config(self):
        class _FakeEndpoint:
            def fetch_api_info(self, space):
                return RemoteScaffoldTest.CONFIG_INFO

            def fetch_config(self, space):
                return RemoteScaffoldTest.CONFIG

            def resolve_canonical_path(self, space):
                return "owner/backend"

        agent = HarpModelAgent(endpoint_client=_FakeEndpoint())
        recipe = agent.scaffold_remote_recipe("owner/backend", auto_endpoint=True)
        validate_recipe(recipe)
        by_name = {spec["name"]: spec for spec in recipe["inputs"]}
        self.assertEqual(by_name["file_type"]["choices"], ["mp3", "wav"])
        self.assertEqual(by_name["steps"]["max"], 100)


class RefineRemoteRecipeTest(unittest.TestCase):
    SCAFFOLD = {
        "_todo": ["inputs.control.choices: set the real dropdown options"],
        "model": {"id": "owner/backend", "name": "Backend", "description": "TODO"},
        "framework": {
            "gpu": False,
            "pip": [],
            "remote": {
                "space": "owner/backend",
                "api_name": "/synthesis_function",
                "token_env": "HF_TOKEN",
                "args": [
                    {"from": "prompt_audio", "file": True},
                    {"from": "session"},
                    {"from": "control"},
                ],
                "returns": [{"index": 0, "to": "generated"}],
            },
        },
        "inputs": [
            {"name": "prompt_audio", "type": "audio", "label": "Ref"},
            {"name": "session", "type": "textbox", "label": "Session"},
            {"name": "control", "type": "dropdown", "label": "Control",
             "choices": ["TODO_option_1", "TODO_option_2"]},
        ],
        "outputs": [{"name": "generated", "type": "audio", "label": "Out"}],
    }

    ENDPOINT = {
        "parameters": [
            {"label": "Ref", "parameter_name": "prompt_audio", "component": "Audio"},
            {"label": "Session", "parameter_name": "session", "component": "Textbox"},
            {"label": "Control", "parameter_name": "control", "component": "Dropdown"},
        ],
        "returns": [{"label": "Out", "component": "Audio"}],
    }

    def test_llm_refines_but_keeps_call_signature(self):
        # The LLM turns the hidden 'session' arg into a const, fills dropdown
        # choices, and writes a real description -- while keeping args length/order.
        llm_response = {
            "model": {"description": "Zero-shot singing voice synthesis.", "tags": ["svs"]},
            "framework": {
                "gpu": False,
                "pip": [],
                "remote": {
                    "space": "owner/backend",
                    "api_name": "/synthesis_function",
                    "args": [
                        {"from": "prompt_audio", "file": True},
                        {"const": None},
                        {"from": "control"},
                    ],
                    "returns": [{"index": 0, "to": "generated"}],
                },
            },
            "inputs": [
                {"name": "prompt_audio", "type": "audio", "label": "Reference voice"},
                {"name": "control", "type": "dropdown", "label": "Control",
                 "choices": ["auto", "manual"], "default": "auto"},
            ],
            "outputs": [{"name": "generated", "type": "audio", "label": "Generated"}],
        }
        provider = _FakeProvider([llm_response])
        draft = refine_remote_recipe(self.SCAFFOLD, self.ENDPOINT, provider, max_repairs=1)

        remote = draft.recipe["framework"]["remote"]
        self.assertEqual(remote["space"], "owner/backend")
        self.assertEqual(remote["api_name"], "/synthesis_function")
        # args length/order preserved; 'session' is now a const.
        self.assertEqual(len(remote["args"]), 3)
        self.assertEqual(remote["args"][1], {"const": None})
        self.assertEqual(remote["args"][0], {"from": "prompt_audio", "file": True})
        # Dropdown choices refined; description filled.
        control = next(i for i in draft.recipe["inputs"] if i["name"] == "control")
        self.assertEqual(control["choices"], ["auto", "manual"])
        self.assertEqual(draft.recipe["model"]["description"], "Zero-shot singing voice synthesis.")
        compile(draft.app_py, "<refine>", "exec")

    def test_refine_prompt_includes_space_source_grounding(self):
        # When Space UI source is attached, the refine prompt must surface it as
        # ground truth so the LLM can fill real choices and const-out hidden args.
        context = RecipeGenerationContext(
            model_id="owner/backend",
            space_sources={"webui.py": "prompt_lyric_lang = gr.Dropdown(choices=['English'])"},
        )
        prompt = build_remote_refine_prompt(self.SCAFFOLD, self.ENDPOINT, context)
        self.assertIn("Backend Space UI source", prompt)
        self.assertIn("webui.py", prompt)
        self.assertIn("prompt_lyric_lang = gr.Dropdown", prompt)
        self.assertIn("const", prompt.lower())

    def test_refine_prompt_omits_source_section_when_absent(self):
        context = RecipeGenerationContext(model_id="owner/backend")
        prompt = build_remote_refine_prompt(self.SCAFFOLD, self.ENDPOINT, context)
        self.assertNotIn("Backend Space UI source", prompt)

    def test_wrong_args_length_is_repaired(self):
        # First response drops an arg (breaks positional integrity) -> rejected;
        # second response is correct.
        bad = {
            "framework": {"remote": {"space": "owner/backend", "api_name": "/synthesis_function",
                                     "args": [{"from": "prompt_audio", "file": True}],
                                     "returns": [{"index": 0, "to": "generated"}]}},
            "inputs": [{"name": "prompt_audio", "type": "audio", "label": "R"}],
            "outputs": [{"name": "generated", "type": "audio", "label": "O"}],
        }
        good = {
            "framework": {"remote": {"space": "owner/backend", "api_name": "/synthesis_function",
                                     "args": [{"from": "prompt_audio", "file": True},
                                              {"const": None}, {"const": "auto"}],
                                     "returns": [{"index": 0, "to": "generated"}]}},
            "inputs": [{"name": "prompt_audio", "type": "audio", "label": "R"}],
            "outputs": [{"name": "generated", "type": "audio", "label": "O"}],
        }
        provider = _FakeProvider([bad, good])
        draft = refine_remote_recipe(self.SCAFFOLD, self.ENDPOINT, provider, max_repairs=2)
        self.assertEqual(draft.attempts, 2)
        self.assertEqual(len(draft.recipe["framework"]["remote"]["args"]), 3)

    def test_space_api_name_are_pinned_even_if_llm_changes_them(self):
        rogue = {
            "framework": {"remote": {"space": "evil/other", "api_name": "/wrong",
                                     "args": [{"from": "prompt_audio", "file": True},
                                              {"const": None}, {"const": "x"}],
                                     "returns": [{"index": 0, "to": "generated"}]}},
            "inputs": [{"name": "prompt_audio", "type": "audio", "label": "R"}],
            "outputs": [{"name": "generated", "type": "audio", "label": "O"}],
        }
        provider = _FakeProvider([rogue])
        draft = refine_remote_recipe(self.SCAFFOLD, self.ENDPOINT, provider, max_repairs=1)
        remote = draft.recipe["framework"]["remote"]
        self.assertEqual(remote["space"], "owner/backend")
        self.assertEqual(remote["api_name"], "/synthesis_function")


class RemoteExampleRecipeTest(unittest.TestCase):
    def test_committed_remote_examples_render(self):
        examples_dir = Path(__file__).resolve().parent.parent / "examples"
        remote_files = sorted(examples_dir.glob("*_remote_recipe.json"))
        self.assertTrue(remote_files, "expected committed remote example recipes")
        for recipe_file in remote_files:
            recipe = json.loads(recipe_file.read_text(encoding="utf-8"))
            validate_recipe(recipe)
            app = render_app_from_recipe(recipe)
            compile(app, str(recipe_file), "exec")
            self.assertIn("from gradio_client import Client, handle_file", app)
            package = build_package_from_recipe(recipe)
            self.assertIn("gradio_client", package.requirements)
            # None of the backends' heavy/conflicting deps leak into the frontend.
            self.assertNotIn("torch", package.requirements)
            self.assertNotIn("nemo", package.requirements)


class BackendRecipeTest(unittest.TestCase):
    RECIPE = {
        "model": {
            "id": "magenta/ddsp",
            "name": "DDSP Timbre Transfer",
            "description": "DDSP timbre transfer.",
            "license": "apache-2.0",
        },
        "framework": {
            "import": "ddsp",
            "backend": True,
            "gpu": True,
            "pip": ["ddsp==3.7.0", "tensorflow==2.11.0", "gradio==3.50.2", "soundfile"],
            "apt": ["libsndfile1"],
        },
        "inputs": [
            {"name": "audio", "type": "audio", "label": "In", "required": True, "info": "clip"},
            {"name": "instrument", "type": "dropdown", "label": "Instrument",
             "choices": ["Violin", "Flute"], "default": "Violin"},
        ],
        "outputs": [{"name": "out_audio", "type": "audio", "label": "Out"}],
        "inference": {"setup": "MODEL = None", "body": "return audio"},
    }

    def test_renders_plain_gradio_backend_without_pyharp(self):
        app_py = render_app_from_recipe(self.RECIPE)
        compile(app_py, "<backend>", "exec")
        # No pyharp anywhere; a plain gr.Interface publishes the /predict endpoint.
        self.assertNotIn("pyharp", app_py)
        self.assertNotIn("build_endpoint", app_py)
        self.assertNotIn(".harp_required", app_py)
        self.assertIn("gr.Interface(", app_py)
        self.assertIn("def predict(audio, instrument):", app_py)
        # GPU flag still adds the spaces shim + decorator.
        self.assertIn("@spaces.GPU", app_py)

    def test_backend_requirements_exclude_pyharp(self):
        package = build_package_from_recipe(self.RECIPE)
        self.assertEqual(package.framework, "gradio-backend")
        self.assertEqual(package.manifest["deploy_mode"], "backend")
        self.assertNotIn("pyharp", package.requirements)
        # The model's own (legacy) pins are preserved verbatim.
        self.assertIn("ddsp==3.7.0", package.requirements)
        self.assertIn("tensorflow==2.11.0", package.requirements)
        self.assertIn("gradio==3.50.2", package.requirements)
        self.assertIn("libsndfile1", package.packages_txt)

    def test_backend_readme_pins_sdk_version_to_recipe_gradio(self):
        package = build_package_from_recipe(self.RECIPE)
        self.assertIn("sdk: gradio", package.readme)
        # sdk_version follows the recipe's gradio pin so HF doesn't force 5.x.
        self.assertIn("sdk_version: 3.50.2", package.readme)
        self.assertIn("/predict", package.readme)

    def test_backend_readme_defaults_sdk_version_when_unpinned(self):
        recipe = json.loads(json.dumps(self.RECIPE))
        recipe["framework"]["pip"] = ["ddsp", "soundfile"]  # no gradio pin
        package = build_package_from_recipe(recipe)
        self.assertIn("sdk_version: 5.28.0", package.readme)
        # gradio is still installed even when the recipe doesn't list it.
        self.assertIn("gradio>=4.0", package.requirements)

    def test_backend_cannot_combine_with_remote(self):
        recipe = json.loads(json.dumps(self.RECIPE))
        recipe["framework"]["remote"] = {"space": "x/y", "api_name": "/predict"}
        with self.assertRaises(RecipeError):
            validate_recipe(recipe)


class DualRecipeTest(unittest.TestCase):
    RECIPE = {
        "model": {"id": "owner/legacy", "name": "Legacy Model", "license": "mit"},
        "framework": {
            "dual": {
                "backend_python": "3.9",
                "apt": ["libsndfile-dev"],
                "backend_pip": ["oldlib==1.2.3", "numpy==1.23.5"],
                "backend_pip_no_deps": ["madmom"],
                "worker": {
                    "imports": "import os",
                    "body": 'outputs = {"out_audio": inputs["audio"]}',
                },
            }
        },
        "inputs": [{"name": "audio", "type": "audio", "label": "In", "required": True}],
        "outputs": [{"name": "out_audio", "type": "audio", "label": "Out"}],
    }

    def test_validates_and_builds_docker_bundle(self):
        validate_recipe(self.RECIPE)
        package = build_package_from_recipe(self.RECIPE)
        # No top-level app.py/requirements/packages; a Docker bundle instead.
        self.assertEqual(package.app_py, "")
        self.assertEqual(package.requirements, "")
        self.assertEqual(package.framework, "dual-interpreter")
        for name in (
            "Dockerfile",
            "start.sh",
            "frontend_app.py",
            "backend_worker.py",
            "requirements-frontend.txt",
            "requirements-backend.txt",
        ):
            self.assertIn(name, package.extra_files)

    def test_generated_python_compiles_and_isolates_backend_deps(self):
        package = build_package_from_recipe(self.RECIPE)
        compile(package.extra_files["frontend_app.py"], "<dual-frontend>", "exec")
        compile(package.extra_files["backend_worker.py"], "<dual-worker>", "exec")
        # Frontend is pyharp; it must NOT carry the backend's pinned deps.
        frontend_reqs = package.extra_files["requirements-frontend.txt"]
        self.assertIn("gradio", frontend_reqs)
        self.assertNotIn("oldlib", frontend_reqs)
        self.assertNotIn("numpy", frontend_reqs)
        # Backend requirements carry exactly the pins.
        backend_reqs = package.extra_files["requirements-backend.txt"]
        self.assertIn("oldlib==1.2.3", backend_reqs)
        self.assertIn("numpy==1.23.5", backend_reqs)
        # The frontend shells out to the backend worker over subprocess IPC.
        self.assertIn("subprocess.run", package.extra_files["frontend_app.py"])
        self.assertIn("build_endpoint", package.extra_files["frontend_app.py"])

    def test_backend_worker_rejects_missing_output_keys(self):
        recipe = json.loads(json.dumps(self.RECIPE))
        recipe["framework"]["dual"]["worker"]["body"] = (
            'outputs = {"vocals": inputs["audio"]}'
        )
        recipe["outputs"] = [
            {"name": "vocals", "type": "audio", "label": "Vocals"},
            {"name": "drums", "type": "audio", "label": "Drums"},
        ]
        worker = build_package_from_recipe(recipe).extra_files["backend_worker.py"]
        with tempfile.TemporaryDirectory() as tmp:
            worker_path = Path(tmp) / "backend_worker.py"
            worker_path.write_text(worker, encoding="utf-8")
            proc = subprocess.run(
                [sys.executable, str(worker_path)],
                input=json.dumps({"inputs": {"audio": "input.wav"}}),
                capture_output=True,
                text=True,
                timeout=10,
            )

        self.assertNotEqual(proc.returncode, 0)
        payload = json.loads(proc.stdout)
        self.assertFalse(payload["ok"])
        self.assertIn("missing output key(s): drums", payload["error"])
        self.assertIn("returned keys: ['vocals']", payload["error"])

    def test_dockerfile_pins_backend_python_and_no_deps_installs(self):
        package = build_package_from_recipe(self.RECIPE)
        dockerfile = package.extra_files["Dockerfile"]
        self.assertIn("sdk: docker", package.readme)
        self.assertIn("app_port: 7860", package.readme)
        self.assertIn("python3.9", dockerfile)
        self.assertIn("/usr/bin/python3.9 -m venv", dockerfile)
        # --no-deps escape hatch rendered for madmom.
        self.assertIn("--no-build-isolation --no-deps madmom", dockerfile)
        # Legacy-sdist build fix: an older setuptools (with pkg_resources) is pinned
        # in the venv AND constrained into pip's isolated build environments.
        self.assertIn('"setuptools<81"', dockerfile)
        self.assertIn("PIP_CONSTRAINT=/tmp/backend-build-constraints.txt", dockerfile)
        self.assertIn("setuptools<81", dockerfile)

    def test_build_constraints_field_is_rendered(self):
        recipe = json.loads(json.dumps(self.RECIPE))
        recipe["framework"]["dual"]["build_constraints"] = ["Cython<3", "numpy==1.23.5"]
        dockerfile = build_package_from_recipe(recipe).extra_files["Dockerfile"]
        self.assertIn("Cython<3", dockerfile)
        self.assertIn("numpy==1.23.5", dockerfile)

    def test_rejects_both_remote_and_dual(self):
        recipe = json.loads(json.dumps(self.RECIPE))
        recipe["framework"]["remote"] = {"space": "x/y", "api_name": "/predict"}
        with self.assertRaises(RecipeError):
            validate_recipe(recipe)

    def test_requires_worker_body(self):
        recipe = json.loads(json.dumps(self.RECIPE))
        recipe["framework"]["dual"]["worker"]["body"] = ""
        with self.assertRaises(RecipeError):
            validate_recipe(recipe)


class DualExampleRecipeTest(unittest.TestCase):
    def test_committed_dual_examples_build(self):
        examples_dir = Path(__file__).resolve().parent.parent / "examples"
        dual_files = sorted(examples_dir.glob("*_dual_recipe.json"))
        self.assertTrue(dual_files, "expected a committed dual example recipe")
        for recipe_file in dual_files:
            recipe = json.loads(recipe_file.read_text(encoding="utf-8"))
            validate_recipe(recipe)
            package = build_package_from_recipe(recipe)
            compile(package.extra_files["frontend_app.py"], str(recipe_file), "exec")
            compile(package.extra_files["backend_worker.py"], str(recipe_file), "exec")


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


class GeneratedCodeRepairTest(unittest.TestCase):
    """Deterministic fixups applied to LLM-authored inference glue before deploy."""

    def _recipe(self, setup, body):
        return {
            "model": {"id": "x/y", "name": "Y", "description": "d", "author": "x"},
            "framework": {"gpu": False, "pip": ["torch", "torchaudio"]},
            "inputs": [{"name": "text", "type": "textbox", "label": "Text", "required": True}],
            "outputs": [{"name": "audio", "type": "audio", "label": "Audio"}],
            "inference": {"setup": setup, "body": body},
        }

    def test_fixes_torchaudio_cuda_and_adds_torch_import(self):
        # Tacotron2 regression: torchaudio has no .cuda; device checks live on torch.
        recipe = self._recipe(
            'import torchaudio\nDEVICE = "cuda" if torchaudio.cuda.is_available() else "cpu"',
            "return save_audio(text)",
        )
        app = render_app_from_recipe(recipe)
        compile(app, "<repair>", "exec")
        self.assertNotIn("torchaudio.cuda", app)
        self.assertIn("torch.cuda.is_available()", app)
        # torch is referenced after the rewrite but not imported by setup -> injected.
        self.assertIn("\nimport torch\n", app)

    def test_injects_torch_load_weights_only_shim(self):
        # Bark regression: torch>=2.6 defaults weights_only=True and breaks legacy
        # checkpoints; the shim restores the old default for any torch model.
        app = render_app_from_recipe(self._recipe("import bark", "return None"))
        compile(app, "<shim>", "exec")
        self.assertIn('kwargs.setdefault("weights_only", False)', app)
        self.assertIn("_torch.load = _torch_load_compat", app)

    def test_no_duplicate_torch_import_when_setup_imports_it(self):
        recipe = self._recipe("import torch\nx = torch.zeros(1)", "return None")
        app = render_app_from_recipe(recipe)
        compile(app, "<dup>", "exec")
        # setup already imports torch, so the renderer must not add a second one.
        self.assertEqual(app.count("\nimport torch\n"), 1)


class CompanionRequirementTest(unittest.TestCase):
    """Undeclared runtime companions (torchaudio>=2.8 -> torchcodec + ffmpeg)."""

    def _pkg(self, pip):
        recipe = {
            "model": {"id": "x/y", "name": "Y", "description": "d", "author": "x"},
            "framework": {"gpu": False, "pip": pip},
            "inputs": [{"name": "text", "type": "textbox", "label": "T", "required": True}],
            "outputs": [{"name": "audio", "type": "audio", "label": "A"}],
            "inference": {"setup": "", "body": "return None"},
        }
        return build_package_from_recipe(recipe)

    def _has(self, requirements, name):
        return any(r.strip().lower().startswith(name) for r in requirements.splitlines())

    def test_unpinned_torchaudio_adds_torchcodec_and_ffmpeg(self):
        # Tacotron2 regression: torchaudio.load now delegates to torchcodec.
        pkg = self._pkg(["speechbrain", "torch", "torchaudio", "soundfile"])
        self.assertTrue(self._has(pkg.requirements, "torchcodec"))
        self.assertIn("ffmpeg", pkg.packages_txt)

    def test_modern_pinned_torchaudio_adds_torchcodec(self):
        pkg = self._pkg(["torch", "torchaudio==2.9.0"])
        self.assertTrue(self._has(pkg.requirements, "torchcodec"))

    def test_old_torchaudio_does_not_add_torchcodec(self):
        # torchaudio<2.8 still ships the FFmpeg backend, so torchcodec isn't needed.
        pkg = self._pkg(["torch", "torchaudio==2.4.0"])
        self.assertFalse(self._has(pkg.requirements, "torchcodec"))
        self.assertNotIn("ffmpeg", pkg.packages_txt)

    def test_no_duplicate_when_torchcodec_already_present(self):
        pkg = self._pkg(["torchaudio", "torchcodec"])
        count = sum(
            1 for r in pkg.requirements.splitlines() if r.strip().lower().startswith("torchcodec")
        )
        self.assertEqual(count, 1)

    def test_no_torchaudio_no_companion(self):
        pkg = self._pkg(["numpy", "librosa"])
        self.assertFalse(self._has(pkg.requirements, "torchcodec"))

    def test_remote_frontend_never_gets_model_companions(self):
        # Remote frontends install none of the model's deps, so no companion either.
        recipe = remote_recipe_from_api_info(
            "owner/backend",
            {
                "named_endpoints": {
                    "/predict": {
                        "parameters": [{"component": "Textbox"}],
                        "returns": [{"component": "Audio"}],
                    }
                }
            },
        )
        pkg = build_package_from_recipe(recipe)
        self.assertFalse(self._has(pkg.requirements, "torchcodec"))


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
        "requirements.txt": "numpy<1.24\ntorch==2.0.1\n",
        "setup.py": "setup(install_requires=['numpy<1.24', 'librosa<=0.10'])\n",
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

    def test_fetch_github_dependencies_returns_declared_manifests(self):
        manifests = self._agent().fetch_github_dependencies("owner/repo")
        self.assertIn("requirements.txt", manifests)
        self.assertIn("setup.py", manifests)
        self.assertIn("numpy<1.24", manifests["requirements.txt"])
        self.assertIn("install_requires", manifests["setup.py"])
        # A repo without pyproject/setup.cfg simply omits them (no crash).
        self.assertNotIn("pyproject.toml", manifests)

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

    def test_declared_dependencies_are_grounded_in_prompt(self):
        context = RecipeGenerationContext(
            model_id="owner/repo",
            readme="A model.",
            dependency_manifests={
                "setup.py": "setup(install_requires=['numpy<1.24', 'tensorflow<=2.11'])",
                "requirements.txt": "numpy<1.24\n",
            },
        )
        prompt = build_recipe_user_prompt(context)
        self.assertIn("Declared dependencies", prompt)
        self.assertIn("## setup.py", prompt)
        self.assertIn("numpy<1.24", prompt)
        self.assertIn("tensorflow<=2.11", prompt)

    def test_declared_dependencies_section_omitted_when_absent(self):
        context = RecipeGenerationContext(model_id="owner/repo", readme="A model.")
        prompt = build_recipe_user_prompt(context)
        self.assertNotIn("Declared dependencies", prompt)


class RecipeRequirementsLintTest(unittest.TestCase):
    def _recipe(self, pip):
        return {"model": {"id": "a/b", "name": "b"}, "framework": {"pip": pip}}

    def test_flags_bare_numpy_with_source_dep(self):
        warnings = lint_recipe_requirements(
            self._recipe(["git+https://github.com/o/r.git", "numpy", "soundfile"])
        )
        self.assertEqual(len(warnings), 1)
        self.assertIn("numpy", warnings[0])
        self.assertIn("numpy<2", warnings[0])
        # The source-dependency context should be called out.
        self.assertIn("source", warnings[0].lower())

    def test_flags_bare_numpy_without_source_dep(self):
        warnings = lint_recipe_requirements(self._recipe(["numpy", "soundfile"]))
        self.assertEqual(len(warnings), 1)
        self.assertIn("numpy<2", warnings[0])

    def test_pinned_numpy_is_ok(self):
        for entry in ("numpy<2", "numpy==1.26.4", "numpy>=1.24,<2"):
            self.assertEqual(lint_recipe_requirements(self._recipe([entry])), [])

    def test_ignores_numpy_substring_packages(self):
        # 'numpydoc' / 'numpy-foo' must not be mistaken for bare numpy.
        self.assertEqual(lint_recipe_requirements(self._recipe(["numpydoc"])), [])

    def test_flags_other_risk_libs(self):
        warnings = lint_recipe_requirements(self._recipe(["numba"]))
        self.assertEqual(len(warnings), 1)
        self.assertIn("numba", warnings[0])

    def test_no_pip_is_noop(self):
        self.assertEqual(lint_recipe_requirements({"model": {"id": "a/b"}}), [])


class DependencyConflictTest(unittest.TestCase):
    """The pre-deploy checker catches pins that violate a sibling's declared
    constraints (the exact 'librosa==0.10.1 vs ddsp needs librosa<=0.10' bug)."""

    # Fake package metadata so tests never touch the network.
    _REQUIRES_DIST = {
        ("ddsp", "3.7.0"): [
            "librosa (<=0.10)",
            "crepe (<=0.0.12)",
            "numpy (<1.24)",
            "pytest ; extra == 'test'",  # optional extra -> must be ignored
        ],
    }
    _VERSIONS = {
        "librosa": ["0.8.1", "0.9.2", "0.10.0", "0.10.1", "0.11.0"],
        "numpy": ["1.23.5", "1.24.0", "2.0.0"],
    }

    def _requires_dist(self, name, version):
        return self._REQUIRES_DIST.get((name, version))

    def _versions(self, name):
        return self._VERSIONS.get(name, [])

    def _find(self, requirements):
        return find_dependency_conflicts(
            requirements, self._requires_dist, self._versions
        )

    def test_detects_violation_and_suggests_newest_compatible(self):
        conflicts = self._find(["ddsp==3.7.0", "librosa==0.10.1", "numpy==1.23.5"])
        self.assertEqual(len(conflicts), 1)
        conflict = conflicts[0]
        self.assertEqual(conflict["package"], "librosa")
        self.assertEqual(conflict["pinned"], "0.10.1")
        # 0.10.0 is the newest version that satisfies '<=0.10'.
        self.assertEqual(conflict["suggestion"], "0.10.0")

    def test_satisfying_pin_has_no_conflict(self):
        # numpy 1.23.5 satisfies '<1.24'; librosa 0.9.2 satisfies '<=0.10'.
        self.assertEqual(
            self._find(["ddsp==3.7.0", "librosa==0.9.2", "numpy==1.23.5"]), []
        )

    def test_optional_extra_dependencies_are_ignored(self):
        # pytest is only an [test] extra; pinning it must not trigger a conflict.
        self.assertEqual(self._find(["ddsp==3.7.0", "pytest==999.0"]), [])

    def test_version_specifier_semantics(self):
        self.assertFalse(_satisfies("0.10.1", "<=0.10"))
        self.assertTrue(_satisfies("0.10.0", "<=0.10"))
        self.assertTrue(_satisfies("1.23.5", "<1.24"))
        self.assertFalse(_satisfies("1.24.0", "<1.24"))
        self.assertTrue(_satisfies("2.11.0", "<=2.11"))
        self.assertTrue(_satisfies("0.4.2", ">=0.3.0,<=0.10"))

    def test_collect_pip_requirements_by_mode(self):
        dual = {"framework": {"dual": {"backend_pip": ["ddsp==3.7.0", "librosa==0.10.1"]}}}
        self.assertEqual(
            collect_pip_requirements(dual), ["ddsp==3.7.0", "librosa==0.10.1"]
        )
        remote = {"framework": {"remote": {"space": "a/b"}, "pip": ["x==1"]}}
        self.assertEqual(collect_pip_requirements(remote), [])
        plain = {"framework": {"pip": ["torch==2.0"]}}
        self.assertEqual(collect_pip_requirements(plain), ["torch==2.0"])

    def test_apply_dependency_fixes_rewrites_backend_pip(self):
        recipe = {
            "framework": {"dual": {"backend_pip": ["ddsp==3.7.0", "librosa==0.10.1"]}}
        }
        apply_dependency_fixes(recipe, {"librosa": "0.10.0"})
        self.assertEqual(
            recipe["framework"]["dual"]["backend_pip"],
            ["ddsp==3.7.0", "librosa==0.10.0"],
        )

    def test_url_and_option_lines_are_skipped(self):
        # git+/URL and pip option lines can't be checked and must not crash.
        conflicts = self._find(
            [
                "--extra-index-url https://download.pytorch.org/whl/cpu",
                "git+https://github.com/sony/FxNorm-automix",
                "ddsp==3.7.0",
                "librosa==0.10.1",
            ]
        )
        self.assertEqual([c["package"] for c in conflicts], ["librosa"])


class GitHubInstallabilityTest(unittest.TestCase):
    def test_root_pyproject_is_installable(self):
        card = {"files": ["README.md", "pyproject.toml", "src/x.py"]}
        self.assertTrue(_repo_is_pip_installable(card))

    def test_root_setup_py_is_installable(self):
        self.assertTrue(_repo_is_pip_installable({"files": ["setup.py", "pkg/__init__.py"]}))

    def test_only_nested_packaging_is_not_installable(self):
        # A pyproject.toml deep in the tree does NOT make the repo pip-installable.
        card = {"files": ["infer_single.py", "sub/pyproject.toml", "README.md"]}
        self.assertFalse(_repo_is_pip_installable(card))

    def test_no_packaging_files_is_not_installable(self):
        card = {"files": ["infer_single.py", "model.py", "README.md"]}
        self.assertFalse(_repo_is_pip_installable(card))

    def test_unknown_file_list_assumed_installable(self):
        self.assertTrue(_repo_is_pip_installable({}))
        self.assertTrue(_repo_is_pip_installable({"files": []}))

    def test_fetched_manifest_overrides_incomplete_card(self):
        # omnizart regression: the card's partial file listing omits the root
        # pyproject.toml/setup.py, but the dependency grounding fetched them, which
        # authoritatively proves pip-installability.
        card = {"files": ["omnizart/cli.py", "README.md"]}  # no root packaging file
        self.assertFalse(_repo_is_pip_installable(card))
        self.assertTrue(
            _pip_installable_from_signals(card, {"pyproject.toml": "[tool.poetry]"})
        )
        self.assertTrue(
            _pip_installable_from_signals(card, {"setup.py": "from setuptools import setup"})
        )

    def test_only_requirements_manifest_falls_back_to_card(self):
        # requirements.txt alone is not a packaging file -> defer to the card heuristic.
        installable_card = {"files": ["setup.py", "pkg/__init__.py"]}
        noninstallable_card = {"files": ["infer.py", "README.md"]}
        self.assertTrue(
            _pip_installable_from_signals(installable_card, {"requirements.txt": "torch"})
        )
        self.assertFalse(
            _pip_installable_from_signals(noninstallable_card, {"requirements.txt": "torch"})
        )

    def test_strip_repo_pip_removes_matching_ref(self):
        recipe = {
            "framework": {
                "pip": [
                    "git+https://github.com/AMAAI-Lab/SonicMaster.git@main",
                    "torch",
                ]
            }
        }
        _strip_repo_pip(recipe, "git+https://github.com/AMAAI-Lab/SonicMaster.git@v1")
        self.assertEqual(recipe["framework"]["pip"], ["torch"])

    def test_ensure_github_backend_pip_injects_dual_repo_requirement(self):
        recipe = {
            "framework": {
                "dual": {
                    "backend_pip": ["tensorflow==2.9.3"],
                    "worker": {"body": "outputs = {}"},
                }
            }
        }
        _ensure_github_backend_pip(recipe, "git+https://github.com/deezer/spleeter.git@master")
        self.assertEqual(
            recipe["framework"]["dual"]["backend_pip"][0],
            "git+https://github.com/deezer/spleeter.git@master",
        )
        _ensure_github_backend_pip(recipe, "git+https://github.com/deezer/spleeter.git@main")
        self.assertEqual(
            sum(
                1
                for entry in recipe["framework"]["dual"]["backend_pip"]
                if "github.com/deezer/spleeter.git" in entry
            ),
            1,
        )

    def test_strip_repo_backend_pip_removes_matching_ref(self):
        recipe = {
            "framework": {
                "dual": {
                    "backend_pip": [
                        "git+https://github.com/deezer/spleeter.git@master",
                        "tensorflow==2.9.3",
                    ]
                }
            }
        }
        _strip_repo_backend_pip(recipe, "git+https://github.com/deezer/spleeter.git@main")
        self.assertEqual(recipe["framework"]["dual"]["backend_pip"], ["tensorflow==2.9.3"])


class EndpointSelectionTest(unittest.TestCase):
    """Auto-picking the primary endpoint (MelodyFlow: /interrupt,/predict,/toggle_*)."""

    API = {
        "named_endpoints": {
            "/interrupt": {"parameters": [], "returns": []},
            "/predict": {
                "parameters": [{"component": "Textbox"}, {"component": "Audio"}],
                "returns": [{"component": "Audio"}],
            },
            "/toggle_melody": {"parameters": [{"component": "Checkbox"}], "returns": []},
            "/toggle_solver": {"parameters": [{"component": "Dropdown"}], "returns": []},
        }
    }

    class _FakeProvider:
        def __init__(self, choice):
            self._choice = choice

        def complete_json(self, system, user, *, schema=None):
            if isinstance(self._choice, Exception):
                raise self._choice
            return {"api_name": self._choice}

    def test_heuristic_picks_predict_over_controls(self):
        self.assertEqual(guess_primary_endpoint(self.API), "/predict")
        ranked = rank_named_endpoints(self.API)
        self.assertEqual(ranked[0][0], "/predict")
        # control endpoints rank last
        self.assertIn(ranked[-1][0], {"/interrupt", "/toggle_melody", "/toggle_solver"})

    def test_prefers_user_endpoint_over_internal_batched(self):
        # MusicGen regression: /predict_batched is Gradio's internal batched fn and
        # errors on a single-call signature; the user-facing /predict_full must win.
        api = {
            "named_endpoints": {
                "/predict_full": {
                    "parameters": [{"component": "Textbox"}, {"component": "Audio"}],
                    "returns": [{"component": "Audio"}],
                },
                "/predict_batched": {
                    "parameters": [{"component": "Textbox"}, {"component": "Audio"}],
                    "returns": [{"component": "Audio"}],
                },
            }
        }
        self.assertEqual(guess_primary_endpoint(api), "/predict_full")

    def test_single_endpoint_is_returned(self):
        self.assertEqual(
            guess_primary_endpoint({"named_endpoints": {"/run": {"parameters": [], "returns": []}}}),
            "/run",
        )

    def test_no_endpoints_returns_none(self):
        self.assertIsNone(guess_primary_endpoint({"named_endpoints": {}}))

    def test_llm_valid_choice_is_used(self):
        self.assertEqual(
            pick_remote_endpoint(self.API, self._FakeProvider("/predict")), "/predict"
        )

    def test_llm_choice_without_slash_is_normalized(self):
        self.assertEqual(
            pick_remote_endpoint(self.API, self._FakeProvider("predict")), "/predict"
        )

    def test_llm_invalid_choice_falls_back_to_heuristic(self):
        self.assertEqual(
            pick_remote_endpoint(self.API, self._FakeProvider("/nonexistent")), "/predict"
        )

    def test_llm_failure_falls_back_to_heuristic(self):
        self.assertEqual(
            pick_remote_endpoint(self.API, self._FakeProvider(RuntimeError("boom"))), "/predict"
        )


class ResourceHeadsupTest(unittest.TestCase):
    def test_detects_large_weight_size(self):
        # Audio-Omni regression: "~21 GB" checkpoint should trip the heads-up.
        warnings = detect_resource_warnings("Model checkpoint (~21 GB) and a 3.4 GB extra.")
        self.assertEqual(warnings["largest_size_gb"], 21.0)
        self.assertIsNotNone(resource_headsup(warnings))

    def test_terabytes_scale_to_gb(self):
        warnings = detect_resource_warnings("weights are 1.5 TB total")
        self.assertEqual(warnings["largest_size_gb"], 1536.0)

    def test_small_sizes_are_ignored(self):
        warnings = detect_resource_warnings("a tiny 200 MB model and a 1.2 GB file")
        self.assertIsNone(warnings["largest_size_gb"])
        self.assertEqual(warnings["gpu_evidence"], [])
        self.assertIsNone(resource_headsup(warnings))

    def test_memory_specs_are_not_weight_sizes(self):
        # "24 GB VRAM" is a memory requirement, not a download size -> GPU cue only.
        warnings = detect_resource_warnings("Requires 24 GB VRAM on an A100.")
        self.assertIsNone(warnings["largest_size_gb"])
        self.assertTrue(any("VRAM" in e or "A100" in e for e in warnings["gpu_evidence"]))

    def test_docker_gpu_tag_is_not_a_false_positive(self):
        # A base-image tag like "tensorflow:2.5.0-gpu" must not be read as a GPU need.
        warnings = detect_resource_warnings("FROM tensorflow/tensorflow:2.5.0-gpu")
        self.assertEqual(warnings["gpu_evidence"], [])
        self.assertIsNone(resource_headsup(warnings))

    def test_gpu_requirement_phrase_detected(self):
        warnings = detect_resource_warnings("This model requires a GPU to run.")
        self.assertTrue(warnings["gpu_evidence"])
        self.assertIsNotNone(resource_headsup(warnings))


class ApplyCardMetadataTest(unittest.TestCase):
    def test_fills_description_tags_license_from_card(self):
        recipe = {"model": {"name": "X", "description": "TODO: describe this model.", "tags": []}}
        card = {
            "meta": {"tags": ["audio-to-audio"], "license": "mit", "pipeline_tag": "audio"},
            "readme": "# Title\n\n![badge](x)\n\nThis model restores audio from a text prompt.",
        }
        _apply_card_metadata(recipe, card)
        self.assertEqual(recipe["model"]["tags"], ["audio-to-audio"])
        self.assertEqual(recipe["model"]["license"], "mit")
        self.assertIn("restores audio", recipe["model"]["description"])

    def test_does_not_overwrite_existing_description(self):
        recipe = {"model": {"name": "X", "description": "Real desc", "tags": ["t"]}}
        _apply_card_metadata(recipe, {"meta": {"tags": ["other"]}, "readme": "New."})
        self.assertEqual(recipe["model"]["description"], "Real desc")
        self.assertEqual(recipe["model"]["tags"], ["t"])


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
    def test_generated_package_rewrite_removes_stale_layout_files(self):
        base = {
            "model": {"id": "example/layout-switch", "name": "Layout Switch"},
            "inputs": [{"name": "audio", "type": "audio", "label": "Audio"}],
            "outputs": [{"name": "out", "type": "audio", "label": "Out"}],
        }
        single_recipe = {
            **base,
            "framework": {"import": "custom", "pip": []},
            "inference": {"body": "return audio"},
        }
        dual_recipe = {
            **base,
            "framework": {
                "dual": {
                    "backend_python": "3.10",
                    "backend_pip": ["soundfile"],
                    "worker": {"body": "outputs = {'out': inputs.get('audio')}"},
                }
            },
        }
        with tempfile.TemporaryDirectory() as tmp:
            agent = HarpModelAgent()
            folder = agent.write_generated_app_package(
                build_package_from_recipe(single_recipe), Path(tmp)
            )
            self.assertTrue((folder / "app.py").exists())
            self.assertTrue((folder / "requirements.txt").exists())

            folder = agent.write_generated_app_package(
                build_package_from_recipe(dual_recipe), Path(tmp)
            )
            self.assertFalse((folder / "app.py").exists())
            self.assertFalse((folder / "requirements.txt").exists())
            self.assertTrue((folder / "Dockerfile").exists())
            self.assertTrue((folder / "requirements-backend.txt").exists())


class DependencyFingerprintTest(unittest.TestCase):
    def test_is_order_independent_and_drops_comments_and_options(self):
        a = dependency_fingerprint(
            ["numpy==1.23.5", "# a comment", "--extra-index-url https://x", "librosa==0.9.2"]
        )
        b = dependency_fingerprint(["librosa==0.9.2", "numpy==1.23.5"])
        self.assertEqual(a, b)

    def test_is_version_sensitive(self):
        self.assertNotEqual(
            dependency_fingerprint(["numpy==1.23.5"]),
            dependency_fingerprint(["numpy==2.0.0"]),
        )

    def test_git_requirement_is_ref_insensitive(self):
        self.assertEqual(
            dependency_fingerprint(["git+https://github.com/o/r.git@main"]),
            dependency_fingerprint(["git+https://github.com/o/r.git@v2"]),
        )

    def test_fingerprint_for_recipe_uses_installed_pins(self):
        recipe = {
            "framework": {"dual": {"backend_python": "3.9", "backend_pip": ["numpy==1.23.5", "librosa==0.9.2"]}},
        }
        self.assertEqual(
            fingerprint_for_recipe(recipe),
            dependency_fingerprint(["librosa==0.9.2", "numpy==1.23.5"]),
        )

    def test_remote_recipe_has_empty_fingerprint(self):
        recipe = {"framework": {"remote": {"space": "o/s"}, "pip": []}}
        self.assertEqual(fingerprint_for_recipe(recipe), dependency_fingerprint([]))


class RepairRuleTest(unittest.TestCase):
    def test_missing_module_captures_name(self):
        hits = match_repair_rules("ModuleNotFoundError: No module named 'psutil'")
        self.assertTrue(any(h["rule"] == "missing-module" and h.get("match") == "psutil" for h in hits))
        self.assertTrue(any("remove unused imports" in h["hint"] for h in hits))

    def test_imp_and_pkg_resources_rules(self):
        self.assertTrue(
            any(h["rule"] == "imp-removed" for h in match_repair_rules("No module named 'imp'"))
        )
        self.assertTrue(
            any(
                h["rule"] == "pkg-resources-removed"
                for h in match_repair_rules("No module named 'pkg_resources'")
            )
        )

    def test_not_pip_installable_rule(self):
        hits = match_repair_rules("... does not appear to be a Python project ...")
        self.assertTrue(any(h["rule"] == "not-pip-installable" for h in hits))

    def test_dual_backend_missing_output_rule(self):
        self._assert_rule(
            "The backend produced no 'drums' output. Check the Space logs.",
            "dual-backend-missing-output",
        )
        self._assert_rule(
            "RuntimeError: Invalid backend outputs: missing output key(s): drums",
            "dual-backend-missing-output",
        )

    def test_no_match_returns_empty(self):
        self.assertEqual(match_repair_rules("everything is fine"), [])

    def _assert_rule(self, error_text, rule_name):
        hits = match_repair_rules(error_text)
        self.assertTrue(
            any(h["rule"] == rule_name for h in hits),
            f"expected rule {rule_name!r} for {error_text!r}, got {[h['rule'] for h in hits]}",
        )

    def test_zerogpu_spaces_import_order(self):
        self._assert_rule(
            "RuntimeError: CUDA has been initialized before importing the spaces package",
            "zerogpu-spaces-import-order",
        )

    def test_zerogpu_cuda_outside_gpu_scope(self):
        self._assert_rule(
            "RuntimeError: Low-level CUDA init ... did not intercept a CUDA operation",
            "zerogpu-cuda-outside-gpu-scope",
        )

    def test_zerogpu_torch_version(self):
        self._assert_rule(
            "CONFIG_ERROR: torch version 2.4.0 is not compatible with ZeroGPU",
            "zerogpu-torch-version",
        )

    def test_app_starting_timeout(self):
        self._assert_rule("Space stuck on APP_STARTING", "space-app-starting-timeout")

    def test_hf_auth_failed(self):
        self._assert_rule(
            "fatal: Authentication failed for 'https://huggingface.co/spaces/me/x/'",
            "hf-auth-failed",
        )

    def test_hf_push_rejected(self):
        self._assert_rule(
            " ! [rejected] main -> main (fetch first)",
            "hf-push-rejected",
        )

    def test_hf_storage_limit(self):
        self._assert_rule(
            "Repository storage limit reached (Max: 1 GB)",
            "hf-storage-limit",
        )

    def test_hf_hub_artifact_not_found_or_private(self):
        self._assert_rule(
            "Repository Not Found for url: "
            "https://huggingface.co/ZFTurbo/Music-Source-Separation-Training/"
            "resolve/main/vocal_mel_band_roformer_je_1017.ckpt. "
            "Invalid username or password.",
            "hf-hub-artifact-not-found-or-private",
        )

    def test_invalid_space_color(self):
        self._assert_rule(
            '"colorFrom" must be one of [red, yellow, green, ...]',
            "invalid-space-card-color",
        )

    def test_torch_weights_only(self):
        self._assert_rule(
            "_pickle.UnpicklingError: Weights only load failed ... Unsupported global",
            "torch-weights-only",
        )

    def test_lightning_state_dict(self):
        self._assert_rule(
            'RuntimeError: Error(s) in loading state_dict for Model: Missing key(s) in state_dict',
            "lightning-state-dict",
        )

    def test_torchcodec_required(self):
        self._assert_rule(
            "ModuleNotFoundError: No module named 'torchcodec'",
            "torchcodec-required",
        )

    def test_gradio_pin_copied_into_pyharp_frontend(self):
        self._assert_rule(
            "ERROR: Cannot install -r requirements.txt and gradio==6.3.0 because "
            "these package versions have conflicting dependencies.",
            "gradio-pin-copied-into-pyharp-frontend",
        )


class KnowledgeBaseTest(unittest.TestCase):
    def _kb(self, tmp):
        return KnowledgeBase(root=Path(tmp))

    def test_record_and_find_by_repo_and_fingerprint(self):
        with tempfile.TemporaryDirectory() as tmp:
            kb = self._kb(tmp)
            kb.record_deployment(
                {"repo": "o/r", "mode": "dual", "python": "3.9", "deps_fingerprint": "abc123"}
            )
            self.assertEqual(len(kb.find_by_repo("o/r")), 1)
            self.assertEqual(len(kb.find_by_fingerprint("abc123")), 1)
            self.assertEqual(kb.find_by_repo("o/r")[0]["python"], "3.9")

    def test_record_deployment_updates_same_repo_mode(self):
        with tempfile.TemporaryDirectory() as tmp:
            kb = self._kb(tmp)
            kb.record_deployment({"repo": "o/r", "mode": "dual", "outcome": "fail"})
            kb.record_deployment({"repo": "o/r", "mode": "dual", "outcome": "success"})
            deployments = kb.deployments()
            self.assertEqual(len(deployments), 1)
            self.assertEqual(deployments[0]["outcome"], "success")

    def test_different_mode_is_a_separate_record(self):
        with tempfile.TemporaryDirectory() as tmp:
            kb = self._kb(tmp)
            kb.record_deployment({"repo": "o/r", "mode": "dual"})
            kb.record_deployment({"repo": "o/r", "mode": "remote"})
            self.assertEqual(len(kb.deployments()), 2)

    def test_lock_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            kb = self._kb(tmp)
            kb.write_lock("o/r", ["numpy==1.23.5", "librosa==0.9.2"])
            self.assertIn("librosa==0.9.2", kb.lock_text("o/r"))
            self.assertIsNone(kb.lock_text("no/such"))

    def test_failures_retrieval_by_error_signature(self):
        with tempfile.TemporaryDirectory() as tmp:
            kb = self._kb(tmp)
            kb.record_failure(
                {"model": "o/r", "error_signature": "No module named 'psutil'", "fix": "add psutil"}
            )
            matches = kb.find_failures_for_error(
                "Traceback ... ModuleNotFoundError: No module named 'psutil'"
            )
            self.assertEqual(len(matches), 1)
            self.assertEqual(kb.find_failures_for_error("unrelated"), [])

    def test_find_similar_prioritizes_repo_then_fingerprint(self):
        with tempfile.TemporaryDirectory() as tmp:
            kb = self._kb(tmp)
            fp = dependency_fingerprint(["numpy==1.23.5"])
            kb.record_deployment({"repo": "o/r", "mode": "dual", "deps_fingerprint": fp})
            kb.record_deployment({"repo": "other/thing", "mode": "dual", "deps_fingerprint": fp})
            similar = kb.find_similar(repo="o/r", requirements=["numpy==1.23.5"])
            self.assertEqual(similar[0]["repo"], "o/r")
            self.assertEqual(len(similar), 2)


class SeedRegistryTest(unittest.TestCase):
    """The shipped knowledge/ seed must be self-consistent."""

    def test_registry_loads_and_has_the_six_examples(self):
        kb = KnowledgeBase()
        repos = {rec["repo"] for rec in kb.deployments()}
        for expected in (
            "magenta/ddsp",
            "sony/FxNorm-automix",
            "SonyResearch/Woosh",
            "AMAAI-Lab/SonicMaster",
        ):
            self.assertIn(expected, repos)

    def test_dual_recipe_fingerprints_match_registry(self):
        kb = KnowledgeBase()
        base = Path(__file__).resolve().parents[1]
        for rec in kb.deployments():
            recipe_rel = rec.get("recipe")
            if not recipe_rel or rec.get("mode") != "dual":
                continue
            recipe = json.loads((base / recipe_rel).read_text(encoding="utf-8"))
            self.assertEqual(
                fingerprint_for_recipe(recipe),
                rec["deps_fingerprint"],
                msg=f"fingerprint drift for {rec['repo']}",
            )

    def test_lockfiles_exist_where_referenced(self):
        kb = KnowledgeBase()
        base = Path(__file__).resolve().parents[1]
        for rec in kb.deployments():
            lock = rec.get("lock")
            if lock:
                self.assertTrue((base / lock).exists(), msg=f"missing lock {lock}")


class ClassifierSignalTest(unittest.TestCase):
    def test_spec_extraction_and_gradio_conflict(self):
        signals = analyze_signals(
            manifests={"pyproject.toml": "requires-python = '>=3.12'\ndependencies = ['gradio>=6.9.0']"},
        )
        self.assertTrue(signals.gradio_conflict)
        self.assertFalse(signals.python_ok)
        self.assertEqual(signals.python_floor, (3, 12))

    def test_benign_gradio_is_not_a_conflict(self):
        signals = analyze_signals(manifests={"setup.py": "install_requires=['gradio>=4.0']"})
        self.assertFalse(signals.gradio_conflict)

    def test_protobuf_conflict_detected(self):
        # SoulX-Singer regression: NeMo wants protobuf~=5.29.5, but descript-audiotools
        # (a pyharp dep) caps protobuf <3.20 -- they can't co-install.
        signals = analyze_signals(
            manifests={"setup.py": "x", "requirements.txt": "nemo_toolkit==2.6.2\nprotobuf~=5.29.5"}
        )
        packages = [c["package"] for c in signals.dependency_conflicts]
        self.assertIn("protobuf", packages)

    def test_huggingface_hub_1x_conflict_detected(self):
        signals = analyze_signals(
            manifests={"setup.py": "x", "requirements.txt": "huggingface_hub==1.20.1"}
        )
        packages = [c["package"] for c in signals.dependency_conflicts]
        self.assertIn("huggingface_hub", packages)

    def test_old_httpx_conflict_detected(self):
        signals = analyze_signals(
            manifests={
                "setup.py": "x",
                "requirements.txt": "httpx[http2]<0.20.0,>=0.19.0\nspleeter==2.4.0",
            }
        )
        packages = [c["package"] for c in signals.dependency_conflicts]
        self.assertIn("httpx", packages)

    def test_compatible_shared_pins_are_not_conflicts(self):
        # A protobuf/hub the pyharp stack accepts, and a bare mention, don't trip.
        signals = analyze_signals(
            manifests={
                "setup.py": "x",
                "requirements.txt": "protobuf<3.20\nhuggingface_hub>=0.20\ntorch",
            }
        )
        self.assertEqual(signals.dependency_conflicts, [])

    def test_bare_shared_dep_mention_is_not_a_conflict(self):
        signals = analyze_signals(
            manifests={"setup.py": "x", "requirements.txt": "protobuf\nhuggingface_hub\n"}
        )
        self.assertEqual(signals.dependency_conflicts, [])

    def test_native_fragile_detected(self):
        signals = analyze_signals(
            manifests={"requirements.txt": "crepe<=0.0.12\nnumpy==1.23.5", "setup.py": "x"}
        )
        self.assertIn("crepe", signals.native_fragile)

    def test_cuda_hardcoded_without_fallback(self):
        signals = analyze_signals(
            manifests={"setup.py": "x"},
            sources={"infer.py": "model = net.to('cuda')\nout = model(x)"},
        )
        self.assertTrue(signals.cuda_hardcoded)

    def test_cuda_with_fallback_is_not_hardcoded(self):
        signals = analyze_signals(
            manifests={"setup.py": "x"},
            sources={"infer.py": "dev='cuda' if torch.cuda.is_available() else 'cpu'"},
        )
        self.assertFalse(signals.cuda_hardcoded)

    def test_framework_hub_loadable_card_is_pip_installable(self):
        # A bare HF *model* ref (no github manifests) whose card declares a PyPI
        # framework that loads by hub id is single-Space deployable even without a
        # root setup.py (Tacotron2 / SpeechBrain regression).
        card = {
            "meta": {"library_name": "speechbrain", "tags": ["text-to-speech", "speechbrain"]},
            "files": ["hyperparams.yaml", "model.ckpt"],
        }
        signals = analyze_signals(card=card)
        self.assertTrue(signals.pip_installable)

    def test_framework_detected_from_tags_when_library_name_absent(self):
        card = {"meta": {"tags": ["automatic-speech-recognition", "transformers"]}, "files": ["config.json"]}
        signals = analyze_signals(card=card)
        self.assertTrue(signals.pip_installable)

    def test_weights_only_card_without_framework_is_not_pip_installable(self):
        # No packaging file and no recognized framework -> still not installable.
        card = {"meta": {"tags": ["audio"]}, "files": ["model.ckpt", "config.yaml"]}
        signals = analyze_signals(card=card)
        self.assertFalse(signals.pip_installable)


class ClassifierDecisionTest(unittest.TestCase):
    def test_framework_hub_loadable_hf_model_is_single(self):
        # `deploy speechbrain/tts-tacotron2-ljspeech` must route to a one-command
        # single Space, not two-space (post-refactor SpeechBrain path).
        decision = recommend_mode(
            card={
                "meta": {"library_name": "speechbrain", "tags": ["text-to-speech"]},
                "files": ["hyperparams.yaml", "model.ckpt"],
            }
        )
        self.assertEqual(decision.mode, "single")

    def test_weights_only_hf_model_without_framework_is_two_space(self):
        decision = recommend_mode(
            card={"meta": {"tags": ["audio"]}, "files": ["model.ckpt"]}
        )
        self.assertEqual(decision.mode, "two-space")


    def test_no_blockers_is_single(self):
        decision = recommend_mode(manifests={"setup.py": "install_requires=['numpy']"})
        self.assertEqual(decision.mode, "single")

    def test_gradio_conflict_without_space_is_dual(self):
        decision = recommend_mode(
            manifests={"setup.py": "x", "requirements.txt": "gradio>=6.9.0\ntorch"}
        )
        self.assertEqual(decision.mode, "dual")

    def test_gradio_conflict_with_existing_space_is_remote(self):
        decision = recommend_mode(
            manifests={"setup.py": "x", "requirements.txt": "gradio>=6.9.0"},
            has_existing_space=True,
        )
        self.assertEqual(decision.mode, "remote")

    def test_protobuf_conflict_without_space_is_dual(self):
        decision = recommend_mode(
            manifests={"setup.py": "x", "requirements.txt": "protobuf~=5.29.5\ntorch"}
        )
        self.assertEqual(decision.mode, "dual")
        self.assertTrue(any("protobuf-conflict" in b for b in decision.blockers))

    def test_protobuf_conflict_with_existing_space_is_remote(self):
        decision = recommend_mode(
            manifests={"setup.py": "x", "requirements.txt": "protobuf~=5.29.5"},
            has_existing_space=True,
        )
        self.assertEqual(decision.mode, "remote")

    def test_httpx_conflict_without_space_is_dual(self):
        decision = recommend_mode(
            manifests={
                "setup.py": "x",
                "requirements.txt": "httpx[http2]<0.20.0,>=0.19.0\nspleeter==2.4.0",
            }
        )
        self.assertEqual(decision.mode, "dual")
        self.assertTrue(any("httpx-conflict" in b for b in decision.blockers))

    def test_python_above_dual_ceiling_is_two_space(self):
        decision = recommend_mode(
            manifests={"pyproject.toml": "requires-python = '>=3.12'\ndependencies=['gradio>=6.9.0']"}
        )
        self.assertEqual(decision.mode, "two-space")

    def test_python_311_is_two_space_not_dual(self):
        # ACE-Step github regression: the dual base image (bullseye) can't apt-get
        # python3.11, so >=3.11 must route to two-space rather than a dual build
        # that dies with "Unable to locate package python3.11".
        decision = recommend_mode(
            manifests={
                "pyproject.toml": "requires-python = '>=3.11,<3.13'\ndependencies=['gradio==6.2.0']"
            }
        )
        self.assertEqual(decision.mode, "two-space")

    def test_python_310_can_still_be_dual(self):
        # 3.10 is the base image's own interpreter, so it stays dual-buildable.
        decision = recommend_mode(
            manifests={
                "pyproject.toml": "requires-python = '>=3.10'\ndependencies=['gradio>=6.9.0']"
            }
        )
        self.assertEqual(decision.mode, "dual")

    def test_native_fragile_without_space_is_dual(self):
        decision = recommend_mode(
            manifests={"setup.py": "x", "requirements.txt": "madmom\nnumpy"}
        )
        self.assertEqual(decision.mode, "dual")

    def test_cuda_hardcoded_adds_recommendation_but_stays_single(self):
        decision = recommend_mode(
            manifests={"setup.py": "install_requires=['torch']"},
            sources={"infer.py": "x = m.cuda()"},
        )
        self.assertEqual(decision.mode, "single")
        self.assertTrue(any("CUDA" in r for r in decision.recommendations))


class OrchestratorRefDetectionTest(unittest.TestCase):
    def test_detects_github_url(self):
        target = detect_ref("https://github.com/haoheliu/voicefixer")
        self.assertEqual(target.kind, "github")
        self.assertEqual(target.slug, "haoheliu/voicefixer")

    def test_detects_github_url_with_git_suffix(self):
        target = detect_ref("https://github.com/AMAAI-Lab/SonicMaster.git")
        self.assertEqual(target.kind, "github")
        self.assertEqual(target.name, "SonicMaster")

    def test_detects_hf_space_url(self):
        target = detect_ref("https://huggingface.co/spaces/facebook/MusicGen")
        self.assertEqual(target.kind, "hf_space")
        self.assertEqual(target.slug, "facebook/MusicGen")

    def test_detects_hf_model_url(self):
        target = detect_ref("https://huggingface.co/Soul-AILab/SoulX-Singer")
        self.assertEqual(target.kind, "hf_model")
        self.assertEqual(target.slug, "Soul-AILab/SoulX-Singer")

    def test_bare_ref_defaults_to_hf_model(self):
        self.assertEqual(detect_ref("facebook/MusicGen").kind, "hf_model")

    def test_source_forces_github_for_bare_ref(self):
        target = detect_ref("AMAAI-Lab/SonicMaster", source="github")
        self.assertEqual(target.kind, "github")

    def test_source_forces_space_for_bare_ref(self):
        self.assertEqual(detect_ref("facebook/MusicGen", source="hf-space").kind, "hf_space")

    def test_invalid_ref_raises(self):
        with self.assertRaises(ValueError):
            detect_ref("not a ref!!")

    def test_extract_space_links_dedupes_in_order(self):
        blob = (
            "See https://huggingface.co/spaces/amaai-lab/SonicMaster and "
            "https://huggingface.co/spaces/amaai-lab/SonicMaster again, plus "
            "https://huggingface.co/spaces/other/Thing"
        )
        self.assertEqual(
            extract_space_links(blob),
            ["amaai-lab/SonicMaster", "other/Thing"],
        )


class OrchestratorPlanTest(unittest.TestCase):
    def test_space_ref_proxies_to_itself(self):
        plan = decide_plan(kind="hf_space", slug="facebook/MusicGen")
        self.assertEqual(plan.mode, "remote")
        self.assertEqual(plan.backend_space, "facebook/MusicGen")
        self.assertTrue(plan.can_execute)

    def test_discovered_space_is_preferred(self):
        plan = decide_plan(
            kind="github",
            slug="AMAAI-Lab/SonicMaster",
            backend_space="amaai-lab/SonicMaster",
            decision_mode="single",
        )
        self.assertEqual(plan.mode, "remote")
        self.assertEqual(plan.backend_space, "amaai-lab/SonicMaster")
        self.assertTrue(plan.can_execute)

    def test_clean_github_is_single_space(self):
        plan = decide_plan(kind="github", slug="haoheliu/voicefixer", decision_mode="single")
        self.assertEqual(plan.mode, "single")
        self.assertIsNone(plan.backend_space)
        self.assertTrue(plan.can_execute)

    def test_remote_without_backend_is_not_executable(self):
        plan = decide_plan(
            kind="github",
            slug="Soul-AILab/SoulX-Singer",
            decision_mode="remote",
            decision_blockers=["protobuf conflict"],
        )
        self.assertEqual(plan.mode, "remote-missing-backend")
        self.assertFalse(plan.can_execute)
        self.assertIn("protobuf conflict", plan.blockers)

    def test_dual_is_executable(self):
        plan = decide_plan(kind="github", slug="x/y", decision_mode="dual")
        self.assertEqual(plan.mode, "dual")
        self.assertTrue(plan.can_execute)
        self.assertTrue(plan.guidance)

    def test_two_space_is_guidance_only(self):
        plan = decide_plan(kind="github", slug="x/y", decision_mode="two-space")
        self.assertEqual(plan.mode, "two-space")
        self.assertFalse(plan.can_execute)
        self.assertTrue(plan.guidance)


class OrchestratorStepsTest(unittest.TestCase):
    def _steps(self, plan, target, **kw):
        return build_steps(
            plan,
            target=target,
            target_repo="me/target",
            recipe_path="r.json",
            package_parent="gen",
            package_dir="gen/pkg",
            **kw,
        )

    def test_space_ref_uses_deterministic_scaffold(self):
        target = detect_ref("facebook/MusicGen", source="hf-space")
        plan = decide_plan(kind="hf_space", slug=target.slug)
        steps = self._steps(plan, target, user_token=True)
        self.assertEqual(steps[0][0], "scaffold-remote-recipe")
        self.assertIn("--auto-endpoint", steps[0])
        self.assertIn("--user-token", steps[0])
        self.assertEqual(steps[-1][:1], ["deploy-space"])
        self.assertIn("me/target", steps[-1])

    def test_remote_from_github_grounds_on_source(self):
        target = detect_ref("AMAAI-Lab/SonicMaster", source="github")
        plan = decide_plan(
            kind="github", slug=target.slug, backend_space="amaai-lab/SonicMaster"
        )
        steps = self._steps(plan, target, ground_source=True)
        recipe_step = steps[0]
        self.assertEqual(recipe_step[0], "generate-recipe-from-llm")
        self.assertIn("--remote-space", recipe_step)
        self.assertIn("amaai-lab/SonicMaster", recipe_step)
        self.assertIn("--remote-llm", recipe_step)
        self.assertIn("--github", recipe_step)

    def test_single_space_passes_io_hints(self):
        target = detect_ref("haoheliu/voicefixer", source="github")
        plan = decide_plan(kind="github", slug=target.slug, decision_mode="single")
        steps = self._steps(plan, target, inputs="audio", outputs="audio")
        recipe_step = steps[0]
        self.assertEqual(recipe_step[0], "generate-recipe-from-llm")
        self.assertIn("--github", recipe_step)
        self.assertIn("--inputs", recipe_step)
        self.assertIn("audio", recipe_step)

    def test_dual_space_uses_llm_dual_and_docker_deploy(self):
        target = detect_ref("deezer/spleeter", source="github")
        plan = decide_plan(kind="github", slug=target.slug, decision_mode="dual")
        steps = self._steps(plan, target, inputs="audio", outputs="audio")
        recipe_step = steps[0]
        self.assertEqual(recipe_step[0], "generate-recipe-from-llm")
        self.assertIn("--github", recipe_step)
        self.assertIn("--dual", recipe_step)
        self.assertIn("--inputs", recipe_step)
        self.assertEqual(steps[-1][:1], ["deploy-space"])
        self.assertIn("--sdk", steps[-1])
        self.assertIn("docker", steps[-1])

    def test_non_executable_plan_has_no_steps(self):
        plan = decide_plan(kind="github", slug="x/y", decision_mode="two-space")
        target = detect_ref("x/y", source="github")
        self.assertEqual(self._steps(plan, target), [])


if __name__ == "__main__":
    unittest.main()
