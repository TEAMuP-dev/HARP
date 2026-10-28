"""
Regression checks for the lint tier (src/lint.py).

Each rule is exercised against a small app or requirements file written for
it, so a change to a rule can be checked without reading any Space:

    python -m unittest discover -s model_validation/tests -v

Resolution with uv is left out, since it needs the network.
"""

from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from lint import apply_exceptions, check_lint_exceptions, lint_files
from lint_app import LocalModules, lint_app, lint_containers, lint_revisions
from lint_base import WARNING, Finding, SpaceFiles
from lint_requirements import (find_python_file, matching_torchaudio, read_dockerfile,
                               url_reference_pinned)


# An app laid out as docs/DEPLOYMENT.md asks, which every rule accepts
GOOD_APP = '''
import spaces  # imported first, as ZeroGPU requires

import os

import gradio as gr
import torch
from huggingface_hub import hf_hub_download
from pyharp import ModelCard, build_endpoint

model_card = ModelCard(name="Model", description="", author="", tags=[])

if __name__ != "__main__":
    model = torch.load(hf_hub_download("owner/model", "model.pt", revision="abc1234"))


def process_fn(path):
    if not path:
        raise gr.Error("Provide the audio.")
    return separate(path)


@spaces.GPU(duration=60)
def separate(path):
    return path


if __name__ == "__main__":
    with gr.Blocks() as demo:
        input_components = [gr.Audio(type="filepath", label="Input").harp_required(True)]
        output_components = [gr.Audio(type="filepath", format="wav", label="Output")]
        build_endpoint(model_card=model_card, input_components=input_components,
                       output_components=output_components, process_fn=process_fn)
    demo.queue().launch(show_error=True)
'''


def rules(findings):
    """The rules a list of findings names, without repeats."""
    return sorted({finding.rule for finding in findings})


class AppRulesTest(unittest.TestCase):

    def check(self, source):
        return rules(lint_app(source, "app.py"))

    def test_a_conforming_app_has_no_findings(self):
        self.assertEqual(self.check(GOOD_APP), [])

    def test_spaces_after_a_third_party_import(self):
        source = GOOD_APP.replace("import spaces  # imported first, as ZeroGPU requires\n", "")
        source = source.replace("import torch\n", "import torch\nimport spaces\n")
        self.assertIn("spaces-first", self.check(source))

    def test_standard_library_before_spaces_is_allowed(self):
        source = "import os\nimport sys\n" + GOOD_APP
        self.assertNotIn("spaces-first", self.check(source))

    def test_a_fallback_for_spaces_missing(self):
        source = GOOD_APP.replace(
            "import spaces  # imported first, as ZeroGPU requires\n",
            "try:\n    import spaces\nexcept ImportError:\n    spaces = None\n")
        self.assertIn("spaces-fallback", self.check(source))

    def test_the_gpu_decorator_on_process_fn(self):
        source = GOOD_APP.replace("def process_fn(path):",
                                  "@spaces.GPU(duration=60)\ndef process_fn(path):")
        self.assertIn("gpu-on-process-fn", self.check(source))

    def test_process_fn_is_found_by_the_name_build_endpoint_is_given(self):
        source = GOOD_APP.replace("def process_fn(path):", "@spaces.GPU\ndef run(path):")
        source = source.replace("process_fn=process_fn", "process_fn=run")
        self.assertIn("gpu-on-process-fn", self.check(source))

    def test_loading_at_module_scope(self):
        source = GOOD_APP.replace('if __name__ != "__main__":\n    model', 'if True:\n    model')
        self.assertIn("load-outside-worker", self.check(source))

    def test_loading_behind_a_compound_worker_guard_is_allowed(self):
        source = GOOD_APP.replace('if __name__ != "__main__":',
                                  'if __name__ != "__main__" and os.environ.get("X") is None:')
        self.assertEqual(self.check(source), [])

    def test_loading_through_a_function_the_module_defines(self):
        source = GOOD_APP.replace(
            'if __name__ != "__main__":\n    model = torch.load(hf_hub_download("owner/model", '
            '"model.pt", revision="abc1234"))',
            'def load():\n    return torch.load("model.pt")\n\n\nmodel = load()')
        self.assertIn("load-outside-worker", self.check(source))

    def test_loading_in_a_thread_started_at_module_scope(self):
        source = GOOD_APP.replace(
            'if __name__ != "__main__":\n    model = torch.load(hf_hub_download("owner/model", '
            '"model.pt", revision="abc1234"))',
            'import threading\n\ndef load():\n    global model\n    model = torch.load("m.pt")\n\n'
            'threading.Thread(target=load).start()')
        self.assertIn("load-outside-worker", self.check(source))

    def test_loading_in_the_main_block(self):
        source = GOOD_APP.replace("    with gr.Blocks() as demo:",
                                  '    model = torch.load("m.pt")\n    with gr.Blocks() as demo:')
        self.assertIn("load-outside-worker", self.check(source))

    def test_the_interface_at_module_scope(self):
        source = GOOD_APP.replace('if __name__ == "__main__":\n    with gr.Blocks',
                                  'if True:\n    with gr.Blocks')
        self.assertIn("interface-outside-main", self.check(source))

    def test_the_card_after_process_fn(self):
        card = 'model_card = ModelCard(name="Model", description="", author="", tags=[])\n'
        source = GOOD_APP.replace(card, "").replace("\n@spaces.GPU", card + "\n@spaces.GPU")
        self.assertIn("card-order", self.check(source))

    def test_the_card_inside_the_main_block(self):
        card = 'model_card = ModelCard(name="Model", description="", author="", tags=[])\n'
        source = GOOD_APP.replace(card, "").replace(
            "    with gr.Blocks() as demo:", "    " + card + "    with gr.Blocks() as demo:")
        self.assertIn("card-order", self.check(source))

    def test_launch_without_show_error(self):
        source = GOOD_APP.replace("launch(show_error=True)", "launch()")
        self.assertIn("show-error", self.check(source))

    def test_launch_without_a_queue(self):
        source = GOOD_APP.replace("demo.queue().launch", "demo.launch")
        self.assertIn("queue", self.check(source))

    def test_warnings_never_reach_harp(self):
        source = GOOD_APP.replace('raise gr.Error("Provide the audio.")',
                                  'gr.Warning("Provide the audio.")')
        self.assertIn("info-warning", self.check(source))

    def test_an_input_that_converts_uploads(self):
        source = GOOD_APP.replace('gr.Audio(type="filepath", label="Input")',
                                  'gr.Audio(type="filepath", format="wav", label="Input")')
        self.assertIn("input-format", self.check(source))

    def test_an_input_named_in_the_list_is_found(self):
        source = GOOD_APP.replace(
            'input_components = [gr.Audio(type="filepath", label="Input").harp_required(True)]',
            'audio_in = gr.Audio(type="filepath", format="wav")\n'
            '        input_components = [audio_in]')
        self.assertIn("input-format", self.check(source))

    def test_an_output_format_is_allowed(self):
        self.assertNotIn("input-format", self.check(GOOD_APP))

    def test_a_file_that_does_not_parse(self):
        self.assertEqual(self.check("def broken(:\n"), ["app-file"])


# A module beside the app that keeps the loading, as many Spaces do
RUNTIME = '''
import torch
from transformers import AutoModel

DEVICE = "cuda" if torch.cuda.is_available() else "cpu"
_model = None


def load():
    global _model
    if _model is None:
        _model = AutoModel.from_pretrained("owner/model", revision="abc1234")
    return _model


def generate(x):
    model = load()
    return model(x)
'''

# A module that loads as soon as it is imported
EAGER_RUNTIME = '''
from transformers import AutoModel

MODEL = AutoModel.from_pretrained("owner/model", revision="abc1234")
'''

WORKER_BLOCK = ('if __name__ != "__main__":\n    model = torch.load(hf_hub_download("owner/model", '
                '"model.pt", revision="abc1234"))')

# The generate() of the runtime, moving the model it loads
MOVING = "    model = load()\n    model.to(DEVICE)\n"


class CallFollowingTest(unittest.TestCase):
    """Loading reached through the Space's own modules, and loading inside a GPU call."""

    def check(self, worker, gpu_body="return path", runtime=RUNTIME, zerogpu=True,
              imports="from runtime import generate, load"):
        source = GOOD_APP.replace("from pyharp import ModelCard, build_endpoint\n",
                                  f"from pyharp import ModelCard, build_endpoint\n{imports}\n")
        source = source.replace(WORKER_BLOCK, worker)
        source = source.replace("def separate(path):\n    return path",
                                f"def separate(path):\n    {gpu_body}")
        files = {"app.py": source, "runtime.py": runtime, "eager.py": EAGER_RUNTIME}
        return rules(lint_app(source, "app.py", LocalModules(files), zerogpu))

    def test_a_runtime_loader_called_at_module_scope(self):
        self.assertIn("load-outside-worker", self.check("load()"))

    def test_a_runtime_loader_called_in_the_worker_is_allowed(self):
        self.assertEqual(self.check('if __name__ != "__main__":\n    load()'), [])

    def test_importing_a_module_that_loads_when_imported(self):
        self.assertIn("load-outside-worker", self.check('if __name__ != "__main__":\n    pass',
                                                        imports="import eager"))

    def test_importing_it_in_the_worker_is_allowed(self):
        self.assertEqual(self.check('if __name__ != "__main__":\n    import eager',
                                    imports=""), [])

    def test_a_model_built_inside_the_gpu_call(self):
        found = self.check('if __name__ != "__main__":\n    pass', "return generate(path)")
        self.assertIn("load-in-gpu-call", found)

    def test_a_cache_the_worker_filled_at_startup_is_allowed(self):
        found = self.check('if __name__ != "__main__":\n    load()', "return generate(path)")
        self.assertEqual(found, [])

    def test_a_model_moved_inside_the_gpu_call(self):
        runtime = RUNTIME.replace("    model = load()\n", MOVING)
        found = self.check('if __name__ != "__main__":\n    load()', "return generate(path)",
                           runtime=runtime)
        self.assertIn("load-in-gpu-call", found)

    def test_moving_a_model_the_loader_placed_is_allowed(self):
        runtime = RUNTIME.replace('revision="abc1234")', 'revision="abc1234").to(DEVICE)')
        runtime = runtime.replace("    model = load()\n", MOVING)
        found = self.check('if __name__ != "__main__":\n    load()', "return generate(path)",
                           runtime=runtime)
        self.assertEqual(found, [])

    def test_gpu_calls_are_not_checked_off_zerogpu(self):
        found = self.check('if __name__ != "__main__":\n    pass', "return generate(path)",
                           zerogpu=False)
        self.assertNotIn("load-in-gpu-call", found)


class ContainerTest(unittest.TestCase):

    def test_aiff_among_the_containers_kept(self):
        source = 'LOSSLESS = {".wav", ".flac", ".aiff"}\n'
        self.assertEqual(rules(lint_containers(source, "app.py")), ["aiff-container"])

    def test_wav_and_flac_alone_are_allowed(self):
        source = 'LOSSLESS = {".wav", ".flac"}\n'
        self.assertEqual(lint_containers(source, "app.py"), [])


class RevisionTest(unittest.TestCase):

    def test_a_repository_without_a_revision(self):
        findings = lint_revisions('hf_hub_download("owner/model", "m.pt")\n', "app.py", [])
        self.assertEqual(rules(findings), ["unpinned-revision"])
        self.assertEqual(findings[0].severity, WARNING)

    def test_a_pinned_revision(self):
        source = 'snapshot_download("owner/model", revision="0123abc")\n'
        self.assertEqual(lint_revisions(source, "app.py", []), [])

    def test_a_directory_of_the_space_is_a_path(self):
        source = 'Model.from_pretrained("checkpoints/model")\n'
        self.assertEqual(lint_revisions(source, "app.py", ["checkpoints/model/config.json"]), [])


def space(requirements="", hardware="cpu-basic", app="import spaces\n", **files):
    """A Gradio Space holding the given requirements and app."""
    return SpaceFiles("teamup-tech/test", sdk="gradio", sdk_version="6.24.0", hardware=hardware,
                      files={"app.py": app, "requirements.txt": requirements, **files})


PYHARP = "git+https://github.com/TEAMuP-dev/pyharp.git@v0.4.2\n"


class RequirementRulesTest(unittest.TestCase):

    def check(self, requirements, **kwargs):
        return rules(lint_files(space(requirements, **kwargs)))

    def test_conforming_requirements(self):
        self.assertEqual(self.check(PYHARP + "torch==2.13.0\ntorchaudio==2.11.0\n"
                                             "spaces>=0.51.1\n"), [])

    def test_pyharp_on_a_branch(self):
        self.assertIn("pyharp-pin", self.check(
            "git+https://github.com/TEAMuP-dev/pyharp.git@develop\nspaces>=0.51.1\n"))

    def test_pyharp_from_a_fork(self):
        self.assertIn("pyharp-pin", self.check(
            "git+https://github.com/someone/pyharp.git@v0.4.2\nspaces>=0.51.1\n"))

    def test_pyharp_missing(self):
        self.assertIn("pyharp-pin", self.check("spaces>=0.51.1\n"))

    def test_gradio_listed(self):
        self.assertIn("gradio-listed", self.check(PYHARP + "gradio==6.24.0\nspaces>=0.51.1\n"))

    def test_spaces_imported_but_not_installed(self):
        self.assertIn("spaces-listed", self.check(PYHARP))

    def test_spaces_pinned_exactly(self):
        self.assertIn("spaces-exact", self.check(PYHARP + "spaces==0.51.3\n"))

    def test_a_torch_zerogpu_rejects(self):
        self.assertIn("zerogpu-torch", self.check(PYHARP + "torch==2.9.0\ntorchaudio==2.9.0\n"
                                                           "spaces>=0.51.1\n",
                                                  hardware="zero-a10g"))

    def test_any_torch_off_zerogpu(self):
        self.assertNotIn("zerogpu-torch", self.check(PYHARP + "torch==2.9.0\ntorchaudio==2.9.0\n"
                                                              "spaces>=0.51.1\n"))

    def test_torch_pinned_without_torchaudio(self):
        found = lint_files(space(PYHARP + "torch==2.8.0\nspaces>=0.51.1\n"))
        match = [f for f in found if f.rule == "torchaudio-match"]
        self.assertEqual(len(match), 1)
        self.assertIn("torchaudio==2.8.0", match[0].message)

    def test_url_without_a_reference(self):
        self.assertIn("url-unpinned", self.check(
            PYHARP + "pkg @ git+https://github.com/o/pkg.git\nspaces>=0.51.1\n"))

    def test_url_left_unpinned_with_a_reason(self):
        self.assertNotIn("url-unpinned", self.check(
            PYHARP + "# Another dependency names it by this URL\n"
                     "pkg @ git+https://github.com/o/pkg.git\nspaces>=0.51.1\n"))

    def test_unpinned_packages_are_a_warning(self):
        found = [f for f in lint_files(space(PYHARP + "numpy\nscipy>=1.10\nspaces>=0.51.1\n"))
                 if f.rule == "unpinned"]
        self.assertEqual(len(found), 1)
        self.assertEqual(found[0].severity, WARNING)
        self.assertIn("numpy", found[0].message)
        self.assertIn("scipy>=1.10", found[0].message)

    def test_build_tools_are_not_counted_as_unpinned(self):
        found = [f for f in lint_files(space(PYHARP + "pip\nwheel\nsetuptools\nspaces>=0.51.1\n"))
                 if f.rule == "unpinned"]
        self.assertEqual(found, [])


class TorchaudioTest(unittest.TestCase):

    def test_the_same_release_loads(self):
        self.assertEqual(matching_torchaudio("2.10.0", "2.10.0"), "")

    def test_the_final_release_loads_beside_a_later_torch(self):
        self.assertEqual(matching_torchaudio("2.13.0", "2.11.0"), "")

    def test_the_final_release_does_not_load_beside_an_earlier_torch(self):
        self.assertEqual(matching_torchaudio("2.8.0", "2.11.0"), "2.8.0")


class ReferenceTest(unittest.TestCase):

    def test_references(self):
        for url, pinned in [("git+https://github.com/o/r.git@v1.2.0", True),
                            ("git+https://github.com/o/r@0123abcd", True),
                            ("git+https://github.com/o/r.git@main", False),
                            ("git+https://github.com/o/r.git", False),
                            ("https://github.com/o/r/archive/0123abcdef0123abcdef.zip", True),
                            ("https://github.com/o/r/archive/main.zip", False),
                            ("https://huggingface.co/spaces/o/s/resolve/main/x-1.0-py3.whl",
                             False)]:
            with self.subTest(url=url):
                self.assertEqual(url_reference_pinned(url), pinned)


class DockerfileTest(unittest.TestCase):

    DOCKERFILE = '''FROM python:3.11-slim
COPY requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir gradio==6.24.0 && \\
    pip install --no-cache-dir -r /app/requirements.txt
RUN /usr/bin/python3.9 -m venv "$VENV" && "$VENV/bin/pip" install -r /tmp/backend.txt numpy
CMD ["python", "app.py"]
'''

    def test_what_it_installs(self):
        build = read_dockerfile(self.DOCKERFILE)
        self.assertEqual(build.python, "3.11")
        self.assertEqual(build.requirement_files, ["requirements.txt", "backend.txt"])
        self.assertEqual(build.base_requirement_files, ["requirements.txt"])
        self.assertEqual(sorted(spec.name for spec in build.specs), ["gradio", "numpy"])
        self.assertEqual(find_python_file(build.command), "app.py")


class ExceptionsTest(unittest.TestCase):

    def test_an_excused_rule_is_dropped(self):
        findings = [Finding("card-order", "the card comes late")]
        self.assertEqual(apply_exceptions(findings, {"card-order": "mirrors an example"}), [])

    def test_an_exception_no_longer_needed_is_reported(self):
        kept = apply_exceptions([], {"card-order": "mirrors an example"})
        self.assertEqual(rules(kept), ["stale-exception"])

    def test_an_exception_needs_a_known_rule_and_a_reason(self):
        for exceptions in ({"no-such-rule": "reason"}, {"card-order": ""}, ["card-order"]):
            config = {"overrides": {"teamup-tech/x": {"lint_exceptions": exceptions}}}
            with self.subTest(exceptions=exceptions), self.assertRaises(ValueError):
                check_lint_exceptions(config)


if __name__ == "__main__":
    unittest.main()
