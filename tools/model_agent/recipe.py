"""Recipe-driven generation of HARP pyharp wrappers.

A *recipe* is a declarative JSON spec, derived from the shapes real HARP
wrappers use (see ``analyze``), that fully describes a wrapper: the model card,
the inference framework/dependencies, the ordered input/output components, and
the model-specific inference glue. The generator renders a runnable ``app.py``
(plus ``requirements.txt`` / ``packages.txt`` / ``README.md`` / manifest) from
it, so adding a new model is "write a recipe", not "hand-write a wrapper".

Schema (all string code fields are inlined into the generated module)::

    {
      "model":    {"id", "name", "description", "author", "tags": [...]},
      "framework":{"import", "pip": [...], "apt": [...], "gpu": bool},
      "inputs":   [{"name","type","label","required","info",  ...type-specific}],
      "outputs":  [{"name","type","label","info","file_types": [...]}],
      "inference":{"setup": "<module-level code>", "body": "<process body>"}
    }

Input types:  audio, file, dropdown, slider, textbox, number, checkbox
Output types: audio, file, labels

Remote-backend (proxy) mode
---------------------------
When ``framework.remote`` is present the wrapper does **not** run the model in
its own process. Instead it renders a thin frontend whose ``process_fn`` forwards
the call to a backend Gradio Space via ``gradio_client`` and returns the result.
This keeps the frontend's dependencies to just ``pyharp + gradio + gradio_client``
-- so models whose dependencies conflict with pyharp (e.g. protobuf/nemo vs.
descript-audiotools) become deployable by construction, with the backend Space
left completely unmodified. In this mode ``inference`` is ignored (and optional)::

    "framework": {
      "gpu": false,
      "remote": {
        "space":     "<owner/space or URL of the backend Gradio Space>",
        "api_name":  "/predict",          # backend named endpoint to call
        "token_env": "HF_TOKEN",          # env var with an HF token (private backends)
        "args": [                          # the backend's POSITIONAL call signature
          {"from": "<input name>", "file": true},   # forward a HARP input (files uploaded)
          {"const": null},                          # fixed arg the UI should not expose
          {"from": "steps", "cast": "int"}          # optional int/float/str/bool coercion
        ],
        "returns": [ {"index": 0, "to": "<output name>"} ]  # backend return pos -> HARP output
      }
    }

The optional ``info`` field is a tooltip. It is rendered the way HARP's own
reference wrappers do: as a native ``info=`` kwarg on standard Gradio components
and as a chained pyharp ``.set_info(...)`` on media components (audio/file). A
``file`` component may also carry ``file_types`` (e.g. ``[".mid", ".midi"]``).
The generated module uses ``from pyharp import *`` so inference glue can freely
call pyharp helpers (``load_audio``, ``save_audio``, ``AudioLabel``, ...).
"""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any, Callable, Dict, List, Mapping, MutableMapping, Optional, Sequence, Tuple

from .agent import GeneratedAppPackage

JSON = Dict[str, Any]

INPUT_TYPES = {"audio", "file", "dropdown", "slider", "textbox", "number", "checkbox"}
OUTPUT_TYPES = {"audio", "file", "labels"}

# The `spaces` package only exists on Hugging Face Spaces (ZeroGPU). Import it
# defensively with a no-op `@spaces.GPU` fallback so generated GPU wrappers also
# run locally (e.g. for smoke-tests) and on non-ZeroGPU hosts.
_SPACES_IMPORT_BLOCK = """\
try:
    import spaces
except ImportError:  # 'spaces' is only provided by Hugging Face Spaces
    import types as _types

    def _gpu(*args, **kwargs):
        if len(args) == 1 and callable(args[0]) and not kwargs:
            return args[0]

        def _decorator(func):
            return func

        return _decorator

    spaces = _types.SimpleNamespace(GPU=_gpu)"""

_BASE_REQUIREMENTS = [
    "git+https://github.com/TEAMuP-dev/pyharp.git@develop",
    "gradio>=4.0",
]

_IDENTIFIER_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


class RecipeError(ValueError):
    """Raised when a recipe is structurally invalid."""


def _slug(value: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", value.strip()).strip("-").lower()


def validate_recipe(recipe: Mapping[str, Any]) -> None:
    """Validate a recipe, raising :class:`RecipeError` with all problems found."""

    errors: List[str] = []

    model = recipe.get("model")
    if not isinstance(model, Mapping):
        errors.append("'model' object is required")
        model = {}
    else:
        if not str(model.get("id") or "").strip():
            errors.append("model.id is required")
        if not str(model.get("name") or "").strip():
            errors.append("model.name is required")

    framework = recipe.get("framework")
    if framework is not None and not isinstance(framework, Mapping):
        errors.append("'framework' must be an object when present")
        framework = {}
    framework = framework if isinstance(framework, Mapping) else {}
    remote = framework.get("remote")
    is_remote = remote is not None
    dual = framework.get("dual")
    is_dual = dual is not None
    is_backend = bool(framework.get("backend"))

    inputs = recipe.get("inputs")
    outputs = recipe.get("outputs")
    if not isinstance(inputs, list) or not inputs:
        errors.append("'inputs' must be a non-empty list")
        inputs = []
    if not isinstance(outputs, list) or not outputs:
        errors.append("'outputs' must be a non-empty list")
        outputs = []

    seen_names = set()
    for index, spec in enumerate(inputs):
        prefix = f"inputs[{index}]"
        errors.extend(_validate_component(spec, prefix, INPUT_TYPES, seen_names, is_input=True))
    for index, spec in enumerate(outputs):
        prefix = f"outputs[{index}]"
        errors.extend(_validate_component(spec, prefix, OUTPUT_TYPES, seen_names, is_input=False))

    input_names = {str(spec.get("name")) for spec in inputs if isinstance(spec, Mapping)}
    output_names = {str(spec.get("name")) for spec in outputs if isinstance(spec, Mapping)}

    if is_remote and is_dual:
        errors.append("framework may not set both 'remote' and 'dual'")
    if is_backend and (is_remote or is_dual):
        errors.append(
            "framework.backend (a plain-Gradio backend Space) cannot be combined "
            "with 'remote' or 'dual'"
        )
    if is_backend:
        py_ver = framework.get("python_version")
        if py_ver is not None and not _PY_VERSION_RE.match(str(py_ver).strip()):
            errors.append(
                "framework.python_version must be a Python 3.x like '3.10' "
                f"(got {py_ver!r})"
            )
        vendor = framework.get("vendor_files")
        if vendor is not None:
            if not isinstance(vendor, Mapping):
                errors.append("framework.vendor_files must be an object of {filename: source}")
            else:
                for name, text in vendor.items():
                    if not str(name).strip() or not isinstance(text, str):
                        errors.append(
                            "framework.vendor_files entries must be filename -> source string"
                        )
                        break

    if is_remote:
        # Remote-backend recipes proxy to a Space, so the inference glue is not
        # required; instead the remote block itself must be well-formed.
        errors.extend(_validate_remote(remote, input_names, output_names))
    elif is_dual:
        # Dual-interpreter recipes run the model in an isolated backend venv, so
        # there is no in-process inference block; the dual block must be sound.
        errors.extend(_validate_dual(dual))
    else:
        inference = recipe.get("inference")
        if not isinstance(inference, Mapping) or not str(inference.get("body") or "").strip():
            errors.append("inference.body (the process function body) is required")

    if errors:
        raise RecipeError("Invalid recipe: " + "; ".join(errors))


_PY_VERSION_RE = re.compile(r"^3\.(8|9|10|11|12|13)$")


def _validate_dual(dual: Any) -> List[str]:
    """Validate a ``framework.dual`` block (isolated backend + pyharp frontend)."""

    if not isinstance(dual, Mapping):
        return ["framework.dual must be an object"]

    errors: List[str] = []

    backend_python = str(dual.get("backend_python") or "3.9").strip()
    if not _PY_VERSION_RE.match(backend_python):
        errors.append(
            "framework.dual.backend_python must be a Python version like '3.9' or '3.10'"
        )

    backend_pip = dual.get("backend_pip")
    if not isinstance(backend_pip, list) or not backend_pip:
        errors.append(
            "framework.dual.backend_pip must be a non-empty list (the backend's pinned deps)"
        )
    elif not all(isinstance(item, str) and item.strip() for item in backend_pip):
        errors.append("framework.dual.backend_pip entries must be non-empty strings")

    for optional_list in ("apt", "backend_pip_no_deps", "build_constraints"):
        value = dual.get(optional_list)
        if value is not None and (
            not isinstance(value, list)
            or not all(isinstance(item, str) and item.strip() for item in value)
        ):
            errors.append(f"framework.dual.{optional_list} must be a list of strings")

    worker = dual.get("worker")
    if not isinstance(worker, Mapping):
        errors.append("framework.dual.worker must be an object with a 'body'")
    else:
        if not str(worker.get("body") or "").strip():
            errors.append(
                "framework.dual.worker.body is required (it reads `inputs` and must "
                "set an `outputs` dict)"
            )
        if worker.get("imports") is not None and not isinstance(worker.get("imports"), str):
            errors.append("framework.dual.worker.imports must be a string when present")

    return errors


_REMOTE_CASTS = {"int", "float", "str", "bool"}


def _validate_remote(remote: Any, input_names: set, output_names: set) -> List[str]:
    """Validate a ``framework.remote`` proxy block against declared I/O names."""

    if not isinstance(remote, Mapping):
        return ["framework.remote must be an object"]

    errors: List[str] = []

    if not str(remote.get("space") or "").strip():
        errors.append("framework.remote.space is required (backend Space id or URL)")

    api_name = remote.get("api_name")
    if not isinstance(api_name, str) or not api_name.strip():
        errors.append("framework.remote.api_name is required (e.g. '/predict')")

    token_env = remote.get("token_env")
    if token_env is not None and (not isinstance(token_env, str) or not token_env.strip()):
        errors.append("framework.remote.token_env must be a non-empty string when present")

    user_token = remote.get("user_token")
    if user_token is not None and not isinstance(user_token, bool):
        errors.append("framework.remote.user_token must be a boolean when present")

    args = remote.get("args")
    if not isinstance(args, list):
        errors.append(
            "framework.remote.args must be a list (the backend's positional call signature)"
        )
        args = []
    for index, entry in enumerate(args):
        prefix = f"framework.remote.args[{index}]"
        if not isinstance(entry, Mapping):
            errors.append(f"{prefix} must be an object")
            continue
        has_from = "from" in entry
        has_const = "const" in entry
        if has_from == has_const:
            errors.append(f"{prefix} must have exactly one of 'from' or 'const'")
        if has_from and str(entry.get("from")) not in input_names:
            errors.append(
                f"{prefix}.from '{entry.get('from')}' does not match any declared input"
            )
        cast = entry.get("cast")
        if cast is not None and cast not in _REMOTE_CASTS:
            errors.append(f"{prefix}.cast must be one of {sorted(_REMOTE_CASTS)}")
        if "file" in entry and not isinstance(entry.get("file"), bool):
            errors.append(f"{prefix}.file must be a boolean")

    returns = remote.get("returns")
    if not isinstance(returns, list) or not returns:
        errors.append(
            "framework.remote.returns must be a non-empty list mapping backend "
            "return positions to HARP outputs"
        )
        returns = []
    mapped_outputs: set = set()
    for index, entry in enumerate(returns):
        prefix = f"framework.remote.returns[{index}]"
        if not isinstance(entry, Mapping):
            errors.append(f"{prefix} must be an object")
            continue
        idx = entry.get("index")
        if not isinstance(idx, int) or isinstance(idx, bool) or idx < 0:
            errors.append(f"{prefix}.index must be a non-negative integer")
        to = str(entry.get("to") or "")
        if to not in output_names:
            errors.append(f"{prefix}.to '{to}' does not match any declared output")
        else:
            mapped_outputs.add(to)
    missing = output_names - mapped_outputs
    if missing:
        errors.append(
            "framework.remote.returns must map every output; missing: "
            + ", ".join(sorted(missing))
        )

    return errors


def _validate_component(
    spec: Any,
    prefix: str,
    allowed_types: set,
    seen_names: set,
    *,
    is_input: bool,
) -> List[str]:
    errors: List[str] = []
    if not isinstance(spec, Mapping):
        return [f"{prefix} must be an object"]

    name = str(spec.get("name") or "")
    if not _IDENTIFIER_RE.match(name):
        errors.append(f"{prefix}.name '{name}' must be a valid Python identifier")
    elif name in seen_names:
        errors.append(f"{prefix}.name '{name}' is duplicated")
    else:
        seen_names.add(name)

    comp_type = str(spec.get("type") or "")
    if comp_type not in allowed_types:
        errors.append(
            f"{prefix}.type '{comp_type}' is not one of {sorted(allowed_types)}"
        )

    if comp_type == "dropdown" and not isinstance(spec.get("choices"), list):
        errors.append(f"{prefix} (dropdown) requires a 'choices' list")
    if comp_type == "slider":
        if spec.get("min") is None or spec.get("max") is None:
            errors.append(f"{prefix} (slider) requires 'min' and 'max'")

    if "info" in spec and not isinstance(spec.get("info"), str):
        errors.append(f"{prefix}.info must be a string")
    if "file_types" in spec and not isinstance(spec.get("file_types"), list):
        errors.append(f"{prefix}.file_types must be a list")

    return errors


_MEDIA_TYPES = {"audio", "file"}


def _component_code(spec: Mapping[str, Any], *, is_input: bool, pyharp: bool = True) -> str:
    comp_type = str(spec.get("type"))
    label = json.dumps(str(spec.get("label") or spec.get("name")))

    info = str(spec.get("info") or "").strip()
    # Standard Gradio components take a tooltip via the info= kwarg; media
    # components (audio/file) use the chained pyharp .set_info(...) instead.
    info_kwarg = f", info={json.dumps(info)}" if info and comp_type not in _MEDIA_TYPES else ""

    if comp_type == "audio":
        code = f'gr.Audio(type="filepath", label={label})'
    elif comp_type == "file":
        file_types = spec.get("file_types")
        if not (isinstance(file_types, list) and file_types):
            # pyharp only supports gr.File as a MIDI track, and get_harp_component
            # does ``'.mid' in gr_cmp.file_types`` -- which raises TypeError when
            # file_types is None (Gradio's default). Default to MIDI so the wrapper
            # is valid pyharp and never crashes on an unset file_types.
            file_types = [".mid", ".midi"]
        types = f", file_types={json.dumps([str(item) for item in file_types])}"
        code = f'gr.File(type="filepath", label={label}{types})'
    elif comp_type == "dropdown":
        choice_list = list(spec.get("choices", []))
        default = spec.get("default")
        # A default that isn't among the choices makes Gradio raise at launch;
        # keep the app renderable by surfacing it as a selectable choice.
        if default is not None and default not in choice_list:
            choice_list = [default, *choice_list]
        choices = json.dumps(choice_list)
        value = f", value={json.dumps(default)}" if default is not None else ""
        code = f"gr.Dropdown(choices={choices}{value}, label={label}{info_kwarg})"
    elif comp_type == "slider":
        lo = spec.get("min")
        hi = spec.get("max")
        default = spec.get("default")
        # Widen the range to include an out-of-range default (older Gradio raises
        # instead of clamping) so the control always renders.
        if isinstance(default, (int, float)) and not isinstance(default, bool):
            if isinstance(lo, (int, float)) and default < lo:
                lo = default
            if isinstance(hi, (int, float)) and default > hi:
                hi = default
        minimum = json.dumps(lo)
        maximum = json.dumps(hi)
        step = f", step={json.dumps(spec.get('step'))}" if spec.get("step") is not None else ""
        default_kwarg = f", value={json.dumps(default)}" if default is not None else ""
        code = f"gr.Slider(minimum={minimum}, maximum={maximum}{step}{default_kwarg}, label={label}{info_kwarg})"
    elif comp_type == "textbox":
        default = (
            f", value={json.dumps(spec.get('default'))}" if spec.get("default") is not None else ""
        )
        code = f"gr.Textbox(label={label}{default}{info_kwarg})"
    elif comp_type == "number":
        default = (
            f"value={json.dumps(spec.get('default'))}, " if spec.get("default") is not None else ""
        )
        code = f"gr.Number({default}label={label}{info_kwarg})"
    elif comp_type == "checkbox":
        default = "True" if bool(spec.get("default", False)) else "False"
        code = f"gr.Checkbox(value={default}, label={label}{info_kwarg})"
    elif comp_type == "labels":
        code = f"gr.JSON(label={label})"
    else:  # pragma: no cover - guarded by validation
        raise RecipeError(f"Unsupported component type: {comp_type}")

    # .harp_required(...) and .set_info(...) are pyharp component extensions; a
    # plain-Gradio backend (pyharp=False) has no pyharp installed, so skip them.
    if pyharp and is_input and spec.get("required"):
        code += ".harp_required(True)"
    if pyharp and info and comp_type in _MEDIA_TYPES:
        code += f".set_info({json.dumps(info)})"
    return code


def _indent(code: str, spaces: int = 4) -> str:
    pad = " " * spaces
    return "\n".join((pad + line if line.strip() else line) for line in code.splitlines())


# Restores torch.load's pre-2.6 default (weights_only=False) so legacy model
# checkpoints -- which pickle non-tensor globals (e.g. numpy scalars) -- load
# without the "Weights only load failed / WeightsUnpickler error" that PyTorch
# >=2.6 raises. Guarded so it is a no-op when torch is not installed. This is the
# single most common runtime failure for torch models packaged for a fresh Space.
_TORCH_LOAD_COMPAT_PREAMBLE = """\
try:  # torch>=2.6 flipped torch.load(weights_only) to True; legacy ckpts need False
    import torch as _torch

    if getattr(_torch.load, "__harp_compat__", False) is False:
        _torch_load_orig = _torch.load

        def _torch_load_compat(*args, **kwargs):
            kwargs.setdefault("weights_only", False)
            return _torch_load_orig(*args, **kwargs)

        _torch_load_compat.__harp_compat__ = True
        _torch.load = _torch_load_compat
except Exception:  # torch not installed / unexpected API -- nothing to patch
    pass"""

# LLM code-generation slips that a deterministic pass can correct before deploy.
# torch{audio,vision} have no `.cuda` submodule; device checks live on `torch`.
_CODE_REPAIRS = (
    (re.compile(r"\btorchaudio\.cuda\b"), "torch.cuda"),
    (re.compile(r"\btorchvision\.cuda\b"), "torch.cuda"),
)
_TORCH_REF_RE = re.compile(r"\btorch\.")
_TORCH_IMPORT_RE = re.compile(r"^\s*import\s+torch\b", re.MULTILINE)


def _repair_inference_code(setup: str, body: str) -> "tuple[str, str, List[str]]":
    """Deterministically fix common LLM/compat mistakes in generated glue.

    Returns ``(setup, body, extra_imports)``. ``extra_imports`` adds ``import
    torch`` when the (repaired) code references ``torch.`` but never imports it --
    e.g. after rewriting a bogus ``torchaudio.cuda.is_available()`` call.
    """

    for pattern, replacement in _CODE_REPAIRS:
        setup = pattern.sub(replacement, setup)
        body = pattern.sub(replacement, body)

    extra_imports: List[str] = []
    combined = f"{setup}\n{body}"
    if _TORCH_REF_RE.search(combined) and not _TORCH_IMPORT_RE.search(setup):
        extra_imports.append("import torch")
    return setup, body, extra_imports


def render_app_from_recipe(recipe: Mapping[str, Any]) -> str:
    """Render a runnable pyharp ``app.py`` from a validated recipe."""

    validate_recipe(recipe)

    framework = recipe.get("framework") or {}
    if isinstance(framework, Mapping) and framework.get("remote"):
        return _render_remote_app(recipe)
    if isinstance(framework, Mapping) and framework.get("dual"):
        # Dual mode's runnable frontend lives in frontend_app.py; render it here
        # too so `render-recipe` shows the pyharp surface users will see.
        return _render_dual_frontend(recipe)
    if isinstance(framework, Mapping) and framework.get("backend"):
        # Plain-Gradio backend Space (no pyharp): the model-running half of the
        # two-Space workflow. A remote-backend frontend proxies to it later.
        return _render_backend_app(recipe)

    model = recipe["model"]
    inputs = recipe["inputs"]
    outputs = recipe["outputs"]
    inference = recipe["inference"]

    uses_gpu = bool(framework.get("gpu"))

    setup = str(inference.get("setup") or "").strip()
    raw_body = str(inference["body"]).strip() or "pass"
    setup, raw_body, extra_imports = _repair_inference_code(setup, raw_body)

    import_lines = ["import gradio as gr"]
    for extra in extra_imports:
        import_lines.append(extra)
    if uses_gpu:
        import_lines.append(_SPACES_IMPORT_BLOCK)
    import_lines.append("")
    # Match HARP's reference wrappers: a star import exposes every pyharp
    # helper (ModelCard, build_endpoint, LabelList, AudioLabel, MidiLabel,
    # load_audio, save_audio, ...) that inference glue may reach for.
    import_lines.append("from pyharp import *")

    arg_names = ", ".join(str(spec["name"]) for spec in inputs)
    body = _indent(raw_body)
    decorator = "@spaces.GPU\n" if uses_gpu else ""

    input_lines = ",\n".join(
        "        " + _component_code(spec, is_input=True) for spec in inputs
    )
    output_lines = ",\n".join(
        "        " + _component_code(spec, is_input=False) for spec in outputs
    )

    parts = [
        "from __future__ import annotations",
        "",
        "\n".join(import_lines),
        "",
        "",
        _TORCH_LOAD_COMPAT_PREAMBLE,
        "",
        "",
    ]
    if setup:
        parts += [setup, "", ""]
    parts += [
        "model_card = ModelCard(",
        f"    name={json.dumps(str(model.get('name')))},",
        f"    description={json.dumps(str(model.get('description') or ''))},",
        f"    author={json.dumps(str(model.get('author') or ''))},",
        f"    tags={json.dumps(list(model.get('tags') or []))},",
        ")",
        "",
        "",
        f"{decorator}def process_fn({arg_names}):",
        body,
        "",
        "",
        "with gr.Blocks() as demo:",
        "    input_components = [",
        input_lines + ("," if input_lines else ""),
        "    ]",
        "    output_components = [",
        output_lines + ("," if output_lines else ""),
        "    ]",
        "    build_endpoint(",
        "        model_card=model_card,",
        "        input_components=input_components,",
        "        output_components=output_components,",
        "        process_fn=process_fn,",
        "    )",
        "",
        "demo.queue().launch(share=True, show_error=False, pwa=True)",
        "",
    ]
    return "\n".join(parts)


def _render_backend_app(recipe: Mapping[str, Any]) -> str:
    """Render a plain-Gradio backend ``app.py`` (no pyharp) exposing ``/predict``.

    This is the model-running half of the two-Space workflow: it installs the
    model's own (often legacy, pyharp-incompatible) dependencies and publishes
    the inference function as a Gradio API endpoint. A separate remote-backend
    frontend then proxies to it via ``gradio_client``. ``gr.Interface`` names its
    endpoint ``/predict`` -- exactly what ``scaffold-remote-recipe`` probes for.

    Unlike the pyharp renderer, the module does NOT ``from pyharp import *``: the
    backend must not depend on pyharp (isolating pyharp's deps from the model's is
    the whole point). Inference glue therefore uses plain libraries (soundfile,
    librosa, ...) for audio I/O rather than pyharp helpers.
    """

    model = recipe["model"]
    inputs = recipe["inputs"]
    outputs = recipe["outputs"]
    inference = recipe["inference"]
    framework = recipe.get("framework") or {}
    uses_gpu = bool(framework.get("gpu"))

    setup = str(inference.get("setup") or "").strip()
    raw_body = str(inference["body"]).strip() or "pass"
    setup, raw_body, extra_imports = _repair_inference_code(setup, raw_body)

    import_lines = ["import gradio as gr"]
    for extra in extra_imports:
        import_lines.append(extra)
    if uses_gpu:
        import_lines.append(_SPACES_IMPORT_BLOCK)

    arg_names = ", ".join(str(spec["name"]) for spec in inputs)
    body = _indent(raw_body)
    decorator = "@spaces.GPU\n" if uses_gpu else ""

    input_lines = ",\n".join(
        "        " + _component_code(spec, is_input=True, pyharp=False) for spec in inputs
    )
    output_lines = ",\n".join(
        "        " + _component_code(spec, is_input=False, pyharp=False) for spec in outputs
    )

    parts = [
        "from __future__ import annotations",
        "",
        "\n".join(import_lines),
        "",
        "",
        _TORCH_LOAD_COMPAT_PREAMBLE,
        "",
        "",
    ]
    if setup:
        parts += [setup, "", ""]
    parts += [
        f"{decorator}def predict({arg_names}):",
        body,
        "",
        "",
        "demo = gr.Interface(",
        "    fn=predict,",
        "    inputs=[",
        input_lines + ("," if input_lines else ""),
        "    ],",
        "    outputs=[",
        output_lines + ("," if output_lines else ""),
        "    ],",
        f"    title={json.dumps(str(model.get('name')))},",
        f"    description={json.dumps(str(model.get('description') or ''))},",
        ")",
        "",
        'if __name__ == "__main__":',
        '    demo.queue().launch(server_name="0.0.0.0", server_port=7860, show_error=True)',
        "",
    ]
    return "\n".join(parts)


def _remote_arg_expr(entry: Mapping[str, Any]) -> str:
    """Render one positional argument of the backend ``predict`` call."""

    if "const" in entry:
        # entry["const"] is already a Python value from the parsed JSON, so its
        # repr is a valid Python literal (None -> "None", "x" -> "'x'", ...).
        return repr(entry.get("const"))

    name = str(entry.get("from"))
    cast = entry.get("cast")
    expr = f"{cast}({name})" if cast in _REMOTE_CASTS else name
    # Files/audio are local paths on the frontend; handle_file uploads them to
    # the backend. (File wrapping supersedes a cast, which wouldn't apply.)
    # Guard None: optional file inputs (e.g. an unused melody prompt) arrive as
    # None, and handle_file(None) raises "File None does not exist"; forward None
    # so the backend applies its own default for the omitted argument.
    if entry.get("file"):
        return f"(handle_file({name}) if {name} else None)"
    return expr


def _render_remote_app(recipe: Mapping[str, Any]) -> str:
    """Render a thin pyharp frontend that proxies to a backend Gradio Space.

    The frontend imports only ``pyharp``/``gradio``/``gradio_client`` -- never the
    model's dependencies -- so backends with dependencies that conflict with
    pyharp become deployable by construction, and the backend Space stays
    unmodified.
    """

    model = recipe["model"]
    framework = recipe.get("framework") or {}
    remote = framework["remote"]
    inputs = recipe["inputs"]
    outputs = recipe["outputs"]

    space = str(remote.get("space"))
    api_name = str(remote.get("api_name"))
    token_env = str(remote.get("token_env") or "HF_TOKEN")
    # When true, the frontend exposes an optional masked "Hugging Face token"
    # control; if a user provides one, ZeroGPU usage on the backend is attributed
    # to THAT user instead of falling back to the Space's own HF_TOKEN secret
    # (which otherwise funds/bottlenecks everyone, and is anonymous -- ~0 ZeroGPU
    # quota -- if the secret is unset).
    accept_user_token = bool(remote.get("user_token"))

    # The token control is appended AFTER the model inputs so it is never part of
    # the backend's positional args; it only selects which identity makes the call.
    _token_param = "_hf_user_token"
    arg_names = ", ".join(str(spec["name"]) for spec in inputs)
    if accept_user_token:
        arg_names = f"{arg_names}, {_token_param}=''" if arg_names else f"{_token_param}=''"

    call_arg_exprs = [_remote_arg_expr(entry) for entry in remote.get("args", [])]

    index_by_output = {
        str(entry["to"]): int(entry["index"]) for entry in remote.get("returns", [])
    }

    body_lines: List[str] = []
    # _tok selects the calling identity (a user token gets a fresh per-call client;
    # otherwise the Space's cached client). Always defined so _make_conn is uniform.
    if accept_user_token:
        body_lines.append(f"    _tok = ({_token_param} or '').strip()")
    else:
        body_lines.append("    _tok = ''")
    body_lines += [
        "    # Call the backend, waking it and retrying if it was asleep (a cold",
        "    # start otherwise fails the first hit with 'read operation timed out').",
        "    _raw = None",
        "    for _attempt in range(_CALL_RETRIES + 1):",
        "        try:",
        "            _conn = _make_conn(_tok)",
        "            _raw = _conn.predict(",
    ]
    for expr in call_arg_exprs:
        body_lines.append("                " + expr + ",")
    body_lines += [
        f"                api_name={json.dumps(api_name)},",
        "            )",
        "            break",
        "        except Exception as _exc:  # never surfaces the token",
        "            if _attempt < _CALL_RETRIES and _is_cold_start(str(_exc)):",
        "                _reset_client()",
        "                _wake_backend()",
        "                continue",
        "            raise gr.Error(_quota_hint(str(_exc)))",
        "    _values = list(_raw) if isinstance(_raw, (list, tuple)) else [_raw]",
    ]
    has_media_output = any(str(spec.get("type")) in _MEDIA_TYPES for spec in outputs)
    if has_media_output:
        # Many backends return their error/status text as a sibling string output
        # (e.g. SonicMaster returns (audio, status)); surface it so a failed call
        # shows the backend's real reason instead of a generic "no output".
        body_lines.append(
            '    _detail = " | ".join(str(_v) for _v in _values '
            "if isinstance(_v, str) and _v.strip())"
        )
    for spec in outputs:
        name = str(spec["name"])
        idx = index_by_output[name]
        body_lines.append(
            f"    _out_{name} = _values[{idx}] if len(_values) > {idx} else None"
        )
        if str(spec.get("type")) in _MEDIA_TYPES:
            fallback = (
                f"The backend Space returned no '{name}' output. Check the backend "
                "Space's logs; if it uses ZeroGPU it may need a moment to warm up."
            )
            body_lines.append(f"    if not _out_{name}:")
            body_lines.append(
                f"        raise gr.Error(_detail or {json.dumps(fallback)})"
            )
    return_names = ", ".join(f"_out_{spec['name']}" for spec in outputs)
    body_lines.append(f"    return {return_names}")

    input_component_lines = ["        " + _component_code(spec, is_input=True) for spec in inputs]
    if accept_user_token:
        # Masked, optional token control. Appended last so it is never forwarded
        # to the backend's positional call -- it only picks the calling identity.
        token_info = (
            "Optional. Paste a Hugging Face token (Settings -> Access Tokens, read "
            "scope) so ZeroGPU usage on the backend is charged to YOUR account. "
            "Used only for this call; not stored. Leave blank to use this Space's "
            "own token."
        )
        input_component_lines.append(
            "        gr.Textbox(label=\"Hugging Face token (optional)\", "
            f"type=\"password\", info={json.dumps(token_info)})"
        )
    input_lines = ",\n".join(input_component_lines)
    output_lines = ",\n".join(
        "        " + _component_code(spec, is_input=False) for spec in outputs
    )

    quota_hint_lines = [
        "def _quota_hint(message):",
        "    # Turn a backend error into an actionable message.",
        "    # NOTE: 'message' is the backend's error text; it never contains our token.",
        "    _low = (message or \"\").lower()",
        "    if \"quota\" in _low or \"zerogpu\" in _low:",
        "        if _ACCEPT_USER_TOKEN:",
        "            return (",
        "                \"The backend's ZeroGPU quota is exhausted for the identity making \"",
        "                \"this call. Paste your own Hugging Face token in the token field \"",
        "                \"(read scope) so usage is attributed to your account.\"",
        "            )",
        "        return (",
        "            \"The backend's ZeroGPU quota is exhausted. This Space's calls are \"",
        "            \"anonymous unless an HF_TOKEN secret is set (Settings -> Variables \"",
        "            \"and secrets); use a token from a PRO account or a ZeroGPU-enabled org.\"",
        "        )",
        "    # Opaque backend failure: the Space raised an exception it refuses to",
        "    # expose (it runs with show_error=False), so all we get is a generic",
        "    # 'Internal Gradio error'. The frontend can't fix a server-side crash --",
        "    # point the user at where the real cause lives.",
        "    if (",
        "        \"internal gradio error\" in _low",
        "        or \"internal server error\" in _low",
        "        or \"apperror\" in _low",
        "        or _low.strip() in (\"\", \"none\")",
        "    ):",
        "        _hint = (",
        "            \"The backend Space raised an error it did not expose (it runs with \"",
        "            \"show_error disabled), so the real cause is only in the backend \"",
        f"            \"Space's Logs tab (\" + _BACKEND_SPACE + \").\"",
        "        )",
        "        if _ACCEPT_USER_TOKEN:",
        "            _hint += (",
        "                \" If it is a ZeroGPU Space, an anonymous call can fail this way -- \"",
        "                \"paste a Hugging Face token in the token field and retry.\"",
        "            )",
        "        return _hint",
        "    return message or \"Backend call failed.\"",
    ]

    parts = [
        "from __future__ import annotations",
        "",
        "import os",
        "import time",
        "import urllib.request",
        "",
        "import gradio as gr",
        "",
        "from pyharp import *",
        "from gradio_client import Client, handle_file",
        "",
        "",
        f"_BACKEND_SPACE = {json.dumps(space)}",
        f"_BACKEND_API_NAME = {json.dumps(api_name)}",
        f"_BACKEND_TOKEN_ENV = {json.dumps(token_env)}",
        f"_ACCEPT_USER_TOKEN = {accept_user_token!r}",
        "# How many times to wake+retry a sleeping backend, and how long to wait for",
        "# it to boot (a free Space cold start can take a few minutes).",
        '_CALL_RETRIES = int(os.environ.get("BACKEND_CALL_RETRIES", "4"))',
        '_WAKE_TIMEOUT = float(os.environ.get("BACKEND_WAKE_TIMEOUT", "420"))',
        "_client = None",
        "",
        "",
        "def _backend_client():",
        "    # Lazily create and cache one warm connection using this Space's own",
        "    # token (from the HF_TOKEN secret) or anonymous if none is set. User",
        "    # tokens are NOT cached here -- they get a fresh per-call connection.",
        "    global _client",
        "    if _client is None:",
        "        _token = os.environ.get(_BACKEND_TOKEN_ENV) or None",
        "        _client = Client(_BACKEND_SPACE, hf_token=_token)",
        "    return _client",
        "",
        "",
        "def _reset_client():",
        "    # Drop the cached connection so the next attempt reconnects to a Space",
        "    # that has since finished waking.",
        "    global _client",
        "    _client = None",
        "",
        "",
        "def _make_conn(tok):",
        "    tok = (tok or '').strip()",
        "    if tok:",
        "        return Client(_BACKEND_SPACE, hf_token=tok)",
        "    return _backend_client()",
        "",
        "",
        "def _space_url(space):",
        "    slug = space.strip().lower().replace('/', '-').replace('_', '-')",
        "    return f'https://{slug}.hf.space/'",
        "",
        "",
        "def _is_cold_start(message):",
        "    # Errors that mean 'the backend was asleep/booting', worth waking+retrying",
        "    # (vs. a real application error, which we surface immediately).",
        "    _low = (message or '').lower()",
        "    return any(s in _low for s in (",
        "        'read operation timed out', 'timed out', 'timeout', 'starting',",
        "        'building', 'not ready', 'no application', 'connection', '503', '502',",
        "    ))",
        "",
        "",
        "def _wake_backend():",
        "    # A sleeping Space boots when its URL is hit; poll until it answers (or",
        "    # the budget expires) so the retried call lands on a running backend.",
        "    _url = _space_url(_BACKEND_SPACE)",
        "    _deadline = time.time() + _WAKE_TIMEOUT",
        "    _delay = 5.0",
        "    while time.time() < _deadline:",
        "        try:",
        "            _req = urllib.request.Request(_url, headers={'User-Agent': 'harp-frontend'})",
        "            with urllib.request.urlopen(_req, timeout=30) as _resp:",
        "                if getattr(_resp, 'status', 200) < 500:",
        "                    return True",
        "        except Exception:",
        "            pass",
        "        time.sleep(_delay)",
        "        _delay = min(_delay * 1.5, 30.0)",
        "    return False",
        "",
        "",
        "\n".join(quota_hint_lines),
        "",
        "",
        "model_card = ModelCard(",
        f"    name={json.dumps(str(model.get('name')))},",
        f"    description={json.dumps(str(model.get('description') or ''))},",
        f"    author={json.dumps(str(model.get('author') or ''))},",
        f"    tags={json.dumps(list(model.get('tags') or []))},",
        ")",
        "",
        "",
        f"def process_fn({arg_names}):",
        "\n".join(body_lines),
        "",
        "",
        "with gr.Blocks() as demo:",
        "    input_components = [",
        input_lines + ("," if input_lines else ""),
        "    ]",
        "    output_components = [",
        output_lines + ("," if output_lines else ""),
        "    ]",
        "    build_endpoint(",
        "        model_card=model_card,",
        "        input_components=input_components,",
        "        output_components=output_components,",
        "        process_fn=process_fn,",
        "    )",
        "",
        "demo.queue().launch(share=True, show_error=False, pwa=True)",
        "",
    ]
    return "\n".join(parts)


# --------------------------------------------------------------------------- #
# Dual-interpreter mode: one Docker Space with a modern pyharp/Gradio frontend
# (Python 3.10) that shells out to an isolated backend venv (pinned old deps,
# possibly an older Python) via a one-shot subprocess. IPC is JSON over
# stdin/stdout; media are passed by file path on the shared container fs. This
# makes "code that hasn't been touched in years" deployable *and* HARP-native in
# a single Space, without the backend's deps ever touching pyharp/Gradio.
# --------------------------------------------------------------------------- #
def _render_dual_frontend(recipe: Mapping[str, Any]) -> str:
    """Render the pyharp frontend that proxies to the isolated backend worker."""

    model = recipe["model"]
    inputs = recipe["inputs"]
    outputs = recipe["outputs"]

    arg_names = ", ".join(str(spec["name"]) for spec in inputs)
    input_name_list = json.dumps([str(spec["name"]) for spec in inputs])
    output_name_list = json.dumps([str(spec["name"]) for spec in outputs])

    input_lines = ",\n".join(
        "        " + _component_code(spec, is_input=True) for spec in inputs
    )
    output_lines = ",\n".join(
        "        " + _component_code(spec, is_input=False) for spec in outputs
    )

    body_lines = [
        f"    _inputs = dict(zip({input_name_list}, [{arg_names}]))",
        "    _outputs = _call_backend({\"inputs\": _inputs})",
    ]
    for spec in outputs:
        name = str(spec["name"])
        body_lines.append(f"    _out_{name} = _outputs.get({json.dumps(name)})")
        if str(spec.get("type")) in _MEDIA_TYPES:
            fallback = (
                f"The backend produced no '{name}' output. Check the Space logs "
                "(the backend worker's stderr is captured there)."
            )
            body_lines.append(f"    if not _out_{name}:")
            body_lines.append(f"        raise gr.Error({json.dumps(fallback)})")
    return_names = ", ".join(f"_out_{spec['name']}" for spec in outputs)
    body_lines.append(f"    return {return_names}")

    parts = [
        "from __future__ import annotations",
        "",
        "import json",
        "import os",
        "import subprocess",
        "",
        "import gradio as gr",
        "",
        "from pyharp import *",
        "",
        "",
        '_BACKEND_PYTHON = os.environ.get("BACKEND_PYTHON", "/opt/backend/bin/python")',
        '_BACKEND_SCRIPT = os.environ.get("BACKEND_SCRIPT", "/app/backend_worker.py")',
        '_BACKEND_TIMEOUT = float(os.environ.get("BACKEND_TIMEOUT", "900"))',
        "",
        "",
        "def _call_backend(payload):",
        "    # One-shot subprocess into the isolated backend venv. The worker prints",
        "    # exactly one JSON object as its final stdout line; all model/library",
        "    # chatter is redirected to stderr (captured in the Space logs).",
        "    completed = subprocess.run(",
        "        [_BACKEND_PYTHON, _BACKEND_SCRIPT],",
        "        input=json.dumps(payload),",
        "        capture_output=True,",
        "        text=True,",
        "        timeout=_BACKEND_TIMEOUT,",
        "    )",
        "    _lines = [ln for ln in completed.stdout.splitlines() if ln.strip()]",
        "    try:",
        "        response = json.loads(_lines[-1]) if _lines else {}",
        "    except json.JSONDecodeError as exc:",
        "        raise gr.Error(",
        '            "The backend returned no valid result. Last stderr: "',
        "            + (completed.stderr[-1500:] or \"(empty)\")",
        "        ) from exc",
        "    if not response.get(\"ok\"):",
        "        raise gr.Error(",
        '            response.get("error") or completed.stderr[-1500:] or "Backend worker failed"',
        "        )",
        "    return response.get(\"outputs\") or {}",
        "",
        "",
        "model_card = ModelCard(",
        f"    name={json.dumps(str(model.get('name')))},",
        f"    description={json.dumps(str(model.get('description') or ''))},",
        f"    author={json.dumps(str(model.get('author') or ''))},",
        f"    tags={json.dumps(list(model.get('tags') or []))},",
        ")",
        "",
        "",
        f"def process_fn({arg_names}):",
        "\n".join(body_lines),
        "",
        "",
        "with gr.Blocks() as demo:",
        "    input_components = [",
        input_lines + ("," if input_lines else ""),
        "    ]",
        "    output_components = [",
        output_lines + ("," if output_lines else ""),
        "    ]",
        "    build_endpoint(",
        "        model_card=model_card,",
        "        input_components=input_components,",
        "        output_components=output_components,",
        "        process_fn=process_fn,",
        "    )",
        "",
        'demo.queue().launch(server_name="0.0.0.0", server_port=int(os.environ.get("PORT", "7860")), show_error=True)',
        "",
    ]
    _ = output_name_list  # names are consumed positionally; kept for clarity
    return "\n".join(parts)


def _render_dual_backend_worker(recipe: Mapping[str, Any]) -> str:
    """Render the one-shot backend worker that runs in the isolated venv."""

    dual = recipe["framework"]["dual"]
    worker = dual.get("worker") or {}
    imports = str(worker.get("imports") or "").strip()
    body = _indent(str(worker.get("body")).strip() or "outputs = {}")
    expected_outputs = [str(spec["name"]) for spec in recipe["outputs"]]
    required_media_outputs = [
        str(spec["name"])
        for spec in recipe["outputs"]
        if str(spec.get("type")) in _MEDIA_TYPES
    ]

    parts = [
        "#!/usr/bin/env python3",
        '"""One-shot backend worker for the isolated interpreter.',
        "",
        "Reads a JSON request {\"inputs\": {...}} on stdin and prints a JSON response",
        "{\"ok\": bool, \"outputs\": {...}} on stdout. Media are exchanged by file path.",
        "All library stdout noise is redirected to stderr so stdout carries only the",
        "JSON protocol.",
        '"""',
        "from __future__ import annotations",
        "",
        "import contextlib",
        "import json",
        "import sys",
        "import traceback",
        "",
        f"_EXPECTED_OUTPUTS = {json.dumps(expected_outputs)}",
        f"_REQUIRED_MEDIA_OUTPUTS = {json.dumps(required_media_outputs)}",
    ]
    if imports:
        parts += ["", imports]
    parts += [
        "",
        "",
        "def _run(inputs):",
        body,
        "    return outputs",
        "",
        "",
        "def _validate_outputs(outputs):",
        "    if not isinstance(outputs, dict):",
        "        raise RuntimeError(",
        '            "framework.dual.worker.body must set outputs to a dict; "',
        '            f"got {type(outputs).__name__}"',
        "        )",
        "    missing = [name for name in _EXPECTED_OUTPUTS if name not in outputs]",
        "    empty_media = [name for name in _REQUIRED_MEDIA_OUTPUTS if not outputs.get(name)]",
        "    if missing or empty_media:",
        "        details = []",
        "        if missing:",
        '            details.append("missing output key(s): " + ", ".join(missing))',
        "        if empty_media:",
        '            details.append("empty media output(s): " + ", ".join(empty_media))',
        "        returned = sorted(str(key) for key in outputs.keys())",
        "        raise RuntimeError(",
        '            "Invalid backend outputs: "',
        '            + "; ".join(details)',
        '            + f". Expected keys: {_EXPECTED_OUTPUTS}; returned keys: {returned}. "',
        '            + "Ensure framework.dual.worker.body sets outputs with every recipe output name."',
        "        )",
        "    return outputs",
        "",
        "",
        "def main():",
        "    try:",
        "        request = json.load(sys.stdin)",
        "    except Exception as exc:",
        '        print(json.dumps({"ok": False, "error": f"invalid request: {exc!r}"}), flush=True)',
        "        return 2",
        '    inputs = request.get("inputs") or {}',
        "    try:",
        "        with contextlib.redirect_stdout(sys.stderr):",
        "            outputs = _run(inputs)",
        "            outputs = _validate_outputs(outputs)",
        '        payload = {"ok": True, "outputs": outputs}',
        "    except Exception:",
        '        payload = {"ok": False, "error": traceback.format_exc()[-3000:]}',
        "    print(json.dumps(payload), flush=True)",
        '    return 0 if payload["ok"] else 1',
        "",
        "",
        'if __name__ == "__main__":',
        "    raise SystemExit(main())",
        "",
    ]
    return "\n".join(parts)


def _render_dual_dockerfile(recipe: Mapping[str, Any]) -> str:
    dual = recipe["framework"]["dual"]
    backend_python = str(dual.get("backend_python") or "3.9").strip()
    apt = [str(pkg) for pkg in (dual.get("apt") or []) if str(pkg).strip()]
    no_deps = [str(pkg) for pkg in (dual.get("backend_pip_no_deps") or []) if str(pkg).strip()]

    # Frontend is always the base image's Python 3.10. The backend uses its own
    # interpreter: the base 3.10 if requested, otherwise Debian's pythonX.Y.
    if backend_python == "3.10":
        backend_apt_pkgs: List[str] = []
        backend_py_bin = "/usr/local/bin/python3.10"
    else:
        backend_apt_pkgs = [
            f"python{backend_python}",
            f"python{backend_python}-dev",
            f"python{backend_python}-venv",
            f"python{backend_python}-distutils",
        ]
        backend_py_bin = f"/usr/bin/python{backend_python}"

    apt_line = " \\\n    ".join(
        ["build-essential", "curl", "git"] + backend_apt_pkgs + apt
    )

    # Legacy sdists (e.g. crepe) import pkg_resources at build time, which modern
    # setuptools has removed -- breaking their PEP517 build. Pin an older setuptools
    # both IN the venv (so pkg_resources exists at runtime too) and, via
    # PIP_CONSTRAINT, in the *isolated build environments* pip creates (so the build
    # backend itself gets pkg_resources). Extra pins from the recipe let a specific
    # tricky sdist get a compatible Cython/numpy at build time.
    build_constraints = ["setuptools<81", "wheel"] + [
        str(item) for item in (dual.get("build_constraints") or []) if str(item).strip()
    ]
    constraints_printf = "\\n".join(build_constraints) + "\\n"

    backend_install = [
        f'RUN {backend_py_bin} -m venv "$BACKEND_VENV" \\',
        f"    && printf '{constraints_printf}' > /tmp/backend-build-constraints.txt \\",
        '    && "$BACKEND_VENV/bin/pip" install --no-cache-dir -U pip wheel "setuptools<81" \\',
        '    && PIP_CONSTRAINT=/tmp/backend-build-constraints.txt "$BACKEND_VENV/bin/pip" install --no-cache-dir -r /tmp/requirements-backend.txt',
    ]
    for pkg in no_deps:
        backend_install[-1] += " \\"
        backend_install.append(
            "    && PIP_CONSTRAINT=/tmp/backend-build-constraints.txt "
            f'"$BACKEND_VENV/bin/pip" install --no-cache-dir --no-build-isolation --no-deps {pkg}'
        )

    lines = [
        "# HARP dual-interpreter Space: pyharp/Gradio frontend (Python 3.10) +",
        f"# isolated model backend (Python {backend_python}) via one-shot subprocess IPC.",
        "FROM python:3.10-slim-bullseye",
        "",
        "ENV DEBIAN_FRONTEND=noninteractive \\",
        "    PYTHONUNBUFFERED=1 \\",
        "    PYTHONIOENCODING=UTF-8 \\",
        "    FRONTEND_VENV=/opt/frontend \\",
        "    BACKEND_VENV=/opt/backend \\",
        "    BACKEND_PYTHON=/opt/backend/bin/python \\",
        "    BACKEND_SCRIPT=/app/backend_worker.py \\",
        "    PORT=7860",
        "",
        f"RUN apt-get update && apt-get install -y --no-install-recommends \\\n    {apt_line} \\",
        "    && rm -rf /var/lib/apt/lists/*",
        "",
        "WORKDIR /app",
        "",
        "COPY requirements-backend.txt /tmp/requirements-backend.txt",
        "\n".join(backend_install),
        "",
        "COPY requirements-frontend.txt /tmp/requirements-frontend.txt",
        'RUN /usr/local/bin/python3.10 -m venv "$FRONTEND_VENV" \\',
        '    && "$FRONTEND_VENV/bin/pip" install --no-cache-dir -U pip wheel \\',
        '    && "$FRONTEND_VENV/bin/pip" install --no-cache-dir -r /tmp/requirements-frontend.txt',
        "",
        "COPY backend_worker.py frontend_app.py start.sh ./",
        "RUN chmod +x /app/start.sh",
        "",
        "EXPOSE 7860",
        'CMD ["/app/start.sh"]',
        "",
    ]
    return "\n".join(lines)


def _render_dual_start_sh() -> str:
    return "\n".join(
        [
            "#!/usr/bin/env bash",
            "set -euo pipefail",
            "",
            'exec "${FRONTEND_VENV:-/opt/frontend}/bin/python" -u /app/frontend_app.py',
            "",
        ]
    )


def _dual_frontend_requirements() -> str:
    return "\n".join(list(_BASE_REQUIREMENTS) + ["gradio==5.28.0", ""])


def _dual_backend_requirements(dual: Mapping[str, Any]) -> str:
    pkgs = [str(pkg) for pkg in (dual.get("backend_pip") or []) if str(pkg).strip()]
    return "\n".join(pkgs + [""])


_GRADIO_PIN_RE = re.compile(r"^gradio\s*==\s*([0-9][0-9A-Za-z.\-]*)", re.IGNORECASE)


def _recipe_gradio_pin(framework: Mapping[str, Any]) -> Optional[str]:
    """Return the exact gradio version pinned in ``framework.pip`` (or None)."""

    for entry in framework.get("pip") or []:
        match = _GRADIO_PIN_RE.match(str(entry).strip())
        if match:
            return match.group(1)
    return None


def _backend_needs_legacy_python(framework: Mapping[str, Any]) -> bool:
    """True when the backend's pip pins are unlikely to resolve on bleeding-edge Python.

    Hugging Face Gradio Spaces have been drifting toward newer CPythons on which
    older TensorFlow / TF1-compat research stacks (REMI, DDSP, …) have no wheels.
    Pinning ``python_version: 3.10`` in the Space README keeps those pins installable.
    """

    for entry in framework.get("pip") or []:
        name = str(entry).strip().split("==")[0].split(">=")[0].split("<")[0].split("[")[0]
        name = name.strip().lower().replace("_", "-")
        if name in {"tensorflow", "tensorflow-cpu", "tensorflow-gpu", "tf-nightly"}:
            return True
        if name in {"ddsp", "crepe", "magenta"}:
            return True
    return False


def ensure_backend_runtime_defaults(recipe: MutableMapping[str, Any]) -> None:
    """Fill backend-only runtime defaults (python_version) when missing.

    Mutates ``recipe`` in place. Safe to call repeatedly.
    """

    framework = recipe.get("framework")
    if not isinstance(framework, MutableMapping) or not framework.get("backend"):
        return
    if str(framework.get("python_version") or "").strip():
        return
    if _backend_needs_legacy_python(framework):
        framework["python_version"] = "3.10"


def attach_vendor_files(
    recipe: MutableMapping[str, Any], sources: Mapping[str, str]
) -> int:
    """Copy top-level ``.py`` sources into ``framework.vendor_files`` for the Space.

    Used for script-style GitHub repos (no setup.py): the LLM may still emit
    runtime ``urllib`` downloads, but vendored files make ``os.path.exists`` skip
    those downloads and keep imports working offline. Returns the number of files
    attached.
    """

    framework = recipe.get("framework")
    if not isinstance(framework, MutableMapping) or not framework.get("backend"):
        return 0
    vendor: Dict[str, str] = {}
    existing = framework.get("vendor_files")
    if isinstance(existing, Mapping):
        vendor.update({str(k): str(v) for k, v in existing.items() if isinstance(v, str)})
    for path, text in (sources or {}).items():
        if not isinstance(text, str) or not text.strip():
            continue
        name = Path(str(path).replace("\\", "/")).name
        if not name.endswith(".py"):
            continue
        # Don't overwrite an explicit vendor entry the LLM/recipe already set.
        vendor.setdefault(name, text)
    if not vendor:
        return 0
    framework["vendor_files"] = vendor
    return len(vendor)


# Some packages need an UNDECLARED companion at runtime: pip won't pull it, it
# appears in no manifest, and it only surfaces as a ModuleNotFoundError on the
# Space. torchaudio>=2.8 moved load/save/info to TorchCodec (and >=2.9 removed the
# old FFmpeg backend), so any code calling torchaudio.load/save (e.g. speechbrain,
# many TTS models) then needs `torchcodec` + system FFmpeg. Keyed by canonical
# (hyphenated) trigger name.
_COMPANION_REQUIREMENTS: Dict[str, Dict[str, Any]] = {
    "torchaudio": {
        "min_version": "2.8.0",
        "pip": ["torchcodec"],
        "apt": ["ffmpeg"],
    },
}


def _spec_admits_at_least(spec: str, exact: Optional[str], min_version: str) -> bool:
    """True if the requirement could resolve to a version >= ``min_version``.

    Unpinned -> resolves to the latest (so yes). An exact pin is compared
    directly. A range admits it if either the floor or an arbitrarily high
    version satisfies it (catches both ``>=X`` and bounded ``>=2.7,<3`` ranges).
    """

    if exact is not None:
        try:
            return _cmp_release(_release_tuple(exact), _release_tuple(min_version)) >= 0
        except Exception:
            return True
    if not spec:
        return True
    try:
        return _satisfies(min_version, spec) or _satisfies("1000.0", spec)
    except Exception:
        return True


def _companion_requirements(entries: List[str]) -> Tuple[List[str], List[str]]:
    """(pip_companions, apt_companions) implied by trigger packages in ``entries``."""

    present: Dict[str, Tuple[str, Optional[str]]] = {}
    for entry in entries:
        parsed = _parse_install_line(entry)
        if parsed is None:
            continue
        name, spec, exact = parsed
        present[name] = (spec, exact)

    pip_add: List[str] = []
    apt_add: List[str] = []
    for trigger, info in _COMPANION_REQUIREMENTS.items():
        canon = trigger.lower().replace("_", "-")
        if canon not in present:
            continue
        spec, exact = present[canon]
        if not _spec_admits_at_least(spec, exact, str(info["min_version"])):
            continue
        for pkg in info.get("pip", []):
            if pkg.lower().replace("_", "-") not in present and pkg not in pip_add:
                pip_add.append(pkg)
        for pkg in info.get("apt", []):
            if pkg not in apt_add:
                apt_add.append(pkg)
    return pip_add, apt_add


def _backend_requirements_from_recipe(framework: Mapping[str, Any]) -> str:
    """Requirements for a plain-Gradio backend Space.

    A backend must NOT install pyharp -- keeping the model's (often legacy) deps
    isolated from pyharp is the entire reason for the two-Space split. So this
    installs gradio + the model's own ``framework.pip`` and nothing else.
    """

    pip = [str(pkg) for pkg in (framework.get("pip") or []) if str(pkg).strip()]
    has_gradio = any(_requirement_name_and_pinned(pkg)[0] == "gradio" for pkg in pip)
    requirements: List[str] = [] if has_gradio else ["gradio>=4.0"]
    for package in pip:
        if package not in requirements:
            requirements.append(package)
    for pkg in _companion_requirements(requirements)[0]:
        if pkg not in requirements:
            requirements.append(pkg)
    return "\n".join(requirements + [""])


def _requirements_from_recipe(framework: Mapping[str, Any]) -> str:
    requirements = list(_BASE_REQUIREMENTS)
    # Remote-backend frontends call the model over the network via gradio_client
    # and deliberately install NONE of the model's own deps (that's the whole
    # point -- conflict-free by construction).
    if isinstance(framework, Mapping) and framework.get("remote"):
        requirements.append("gradio_client")
    for package in framework.get("pip", []) or []:
        if package and package not in requirements:
            requirements.append(str(package))
    for pkg in _companion_requirements(requirements)[0]:
        if pkg not in requirements:
            requirements.append(pkg)
    return "\n".join(requirements + [""])


# VCS / URL requirement prefixes. Packages installed from source (rather than a
# prebuilt wheel) are the most fragile part of a build: their build backends and
# transitive pins get resolved fresh, so an unbounded numpy that drifts to 2.x
# routinely breaks them (the classic "Cannot import 'setuptools.build_meta'" /
# numba build failure). We flag that combination.
_VCS_REQ_PREFIXES = ("git+", "hg+", "svn+", "bzr+")

# Libraries that most often break when numpy drifts to 2.x. Left unpinned in a
# recipe with a source dependency present, they are a deployment landmine.
_UNPINNED_RISK_LIBS = frozenset({"numpy", "numba", "llvmlite"})

_REQ_LEADING_NAME_RE = re.compile(r"^([A-Za-z0-9_.\-]+)")


def _requirement_name_and_pinned(entry: str) -> Tuple[Optional[str], bool]:
    """Return ``(canonical_name, has_version_constraint)`` for a pip line.

    Source/URL requirements (``git+...``, ``https://...``) are treated as
    explicit (``name=None, pinned=True``) since they are not a bare PyPI name.
    """

    text = str(entry).strip()
    if not text or text.startswith("#"):
        return None, True
    if text.startswith(_VCS_REQ_PREFIXES) or "://" in text:
        return None, True

    match = _REQ_LEADING_NAME_RE.match(text)
    if not match:
        return None, True

    name = match.group(1).lower().replace("_", "-")
    rest = text[match.end():].strip()
    if rest.startswith("["):  # drop an extras group like [asr]
        close = rest.find("]")
        rest = rest[close + 1:].strip() if close != -1 else ""
    # Anything left (==, <, >, ~=, @, ;, etc.) counts as a constraint/marker.
    return name, bool(rest)


def lint_recipe_requirements(recipe: Mapping[str, Any]) -> List[str]:
    """Warn about fragile ``framework.pip`` patterns (esp. unpinned numpy).

    Returns a list of human-readable warnings; empty when nothing looks risky.
    """

    framework = (recipe or {}).get("framework") or {}
    if not isinstance(framework, Mapping):
        return []
    pip = framework.get("pip")
    if not isinstance(pip, list):
        return []

    has_source_dep = any(
        isinstance(entry, str)
        and (entry.strip().startswith(_VCS_REQ_PREFIXES) or "://" in entry.strip())
        for entry in pip
    )

    warnings: List[str] = []
    for entry in pip:
        name, pinned = _requirement_name_and_pinned(entry)
        if name in _UNPINNED_RISK_LIBS and not pinned:
            if name == "numpy":
                message = (
                    "framework.pip lists 'numpy' with no version bound. numpy 2.x "
                    "frequently breaks source builds and older audio/ML stacks"
                )
                if has_source_dep:
                    message += (
                        " -- and you have a git+/URL source dependency, which builds "
                        "from source and is especially fragile here"
                    )
                message += ". Pin 'numpy<2' unless you specifically need numpy 2.x."
            else:
                message = (
                    f"framework.pip lists '{name}' with no version bound; it commonly "
                    "breaks when numpy drifts to 2.x. Consider pinning it."
                )
            warnings.append(message)
    return warnings


# --- Dependency conflict detection -------------------------------------------
# A recipe can pin a package to a version that a *sibling* package forbids (e.g.
# `librosa==0.10.1` while `ddsp==3.7.0` declares `librosa<=0.10`). pip only
# discovers this at install time -- after a long Docker build. These helpers
# detect such pin-vs-declared-constraint conflicts up front (from package
# metadata) and can auto-repair them, so the agent doesn't need a human to
# hand-feed the right version. Version logic is a small PEP 440 subset (enough
# for the numeric release pins ML recipes use); no third-party 'packaging' dep.

_VERSION_OP_RE = re.compile(r"^(===|~=|==|!=|<=|>=|<|>)\s*(.+)$")
_CORE_VERSION_RE = re.compile(r"^\s*v?(\d+(?:\.\d+)*)")


def _release_tuple(version: str) -> Tuple[int, ...]:
    """Numeric release part of a version (epoch/pre/post/dev/local stripped)."""

    core = _CORE_VERSION_RE.match(str(version).split("+", 1)[0])
    if not core:
        return (0,)
    return tuple(int(part) for part in core.group(1).split("."))


def _cmp_release(a: Tuple[int, ...], b: Tuple[int, ...]) -> int:
    width = max(len(a), len(b))
    a = a + (0,) * (width - len(a))
    b = b + (0,) * (width - len(b))
    return (a > b) - (a < b)


def _satisfies_one(version: str, op: str, ref: str) -> bool:
    va = _release_tuple(version)
    if op in ("==", "==="):
        if ref.endswith(".*"):
            base = _release_tuple(ref[:-2])
            return _cmp_release(va[: len(base)], base) == 0
        return _cmp_release(va, _release_tuple(ref)) == 0
    if op == "~=":  # compatible release: >= ref and same leading components
        vb = _release_tuple(ref)
        if _cmp_release(va, vb) < 0:
            return False
        base = vb[:-1] if len(vb) > 1 else vb
        return _cmp_release(va[: len(base)], base) == 0
    cmp = _cmp_release(va, _release_tuple(ref))
    if op == "!=":
        return cmp != 0
    if op == "<=":
        return cmp <= 0
    if op == ">=":
        return cmp >= 0
    if op == "<":
        return cmp < 0
    if op == ">":
        return cmp > 0
    return True


def _parse_specifier(spec: str) -> List[Tuple[str, str]]:
    clauses: List[Tuple[str, str]] = []
    for part in str(spec).split(","):
        part = part.strip().strip("()")
        if not part:
            continue
        match = _VERSION_OP_RE.match(part)
        if match:
            clauses.append((match.group(1), match.group(2).strip()))
    return clauses


def _satisfies(version: str, spec: str) -> bool:
    return all(_satisfies_one(version, op, ref) for op, ref in _parse_specifier(spec))


_INSTALL_LINE_RE = re.compile(r"^([A-Za-z0-9_.\-]+)\s*(?:\[[^\]]*\])?\s*(.*)$")
_REQUIRES_DIST_RE = re.compile(
    r"^([A-Za-z0-9_.\-]+)\s*(?:\[[^\]]*\])?\s*(\([^)]*\)|[<>=!~][^;]*)?\s*(;.*)?$"
)


def _parse_install_line(line: str) -> Optional[Tuple[str, str, Optional[str]]]:
    """Return ``(canonical_name, specifier, exact_version_or_None)`` for a pip
    line, or None for options/comments/URL requirements (which we can't check)."""

    text = str(line).strip()
    if not text or text.startswith("#") or text.startswith("-"):
        return None
    if text.startswith(_VCS_REQ_PREFIXES) or "://" in text:
        return None
    text = text.split(";", 1)[0].strip()
    match = _INSTALL_LINE_RE.match(text)
    if not match:
        return None
    name = match.group(1).lower().replace("_", "-")
    spec = match.group(2).strip()
    exact = None
    for op, ref in _parse_specifier(spec):
        if op in ("==", "==="):
            exact = ref
    return name, spec, exact


def _parse_requires_dist(entry: str) -> Optional[Tuple[str, str, str]]:
    """Parse a PyPI ``requires_dist`` line into ``(name, specifier, marker)``."""

    match = _REQUIRES_DIST_RE.match(str(entry).strip())
    if not match:
        return None
    name = match.group(1).lower().replace("_", "-")
    spec = (match.group(2) or "").strip()
    marker = (match.group(3) or "").strip().lstrip(";").strip()
    return name, spec, marker


def collect_pip_requirements(recipe: Mapping[str, Any]) -> List[str]:
    """The pip lines that will actually be installed for the model, by mode.

    Dual → the isolated backend's ``backend_pip``; remote → nothing (the
    frontend installs none of the model's deps); otherwise ``framework.pip``.
    """

    framework = (recipe or {}).get("framework") or {}
    if not isinstance(framework, Mapping):
        return []
    dual = framework.get("dual")
    if isinstance(dual, Mapping):
        return [str(x) for x in (dual.get("backend_pip") or []) if str(x).strip()]
    if framework.get("remote"):
        return []
    return [str(x) for x in (framework.get("pip") or []) if str(x).strip()]


def find_dependency_conflicts(
    requirements: List[str],
    requires_dist_of: Callable[[str, str], Optional[List[str]]],
    available_versions: Optional[Callable[[str], Optional[List[str]]]] = None,
) -> List[Dict[str, Any]]:
    """Find pins that violate a sibling package's declared constraints.

    ``requires_dist_of(name, version)`` returns the declared dependency strings
    for a pinned package (e.g. from PyPI metadata), or None if unknown.
    ``available_versions(name)`` (optional) lets us suggest the newest version
    that satisfies every declared constraint. Returns one entry per conflicting
    package: ``{package, pinned, violations:[(source, specifier)], combined,
    suggestion}``.
    """

    parsed = [p for p in (_parse_install_line(line) for line in requirements) if p]
    pinned = {name: exact for (name, _spec, exact) in parsed if exact}
    explicit_spec = {name: spec for (name, spec, _exact) in parsed if spec}

    declared: Dict[str, List[Tuple[str, str]]] = {}
    for name, _spec, exact in parsed:
        if not exact:
            continue
        for entry in requires_dist_of(name, exact) or []:
            dep = _parse_requires_dist(entry)
            if not dep:
                continue
            dep_name, dep_spec, marker = dep
            if "extra" in marker:  # optional-extra deps aren't installed here
                continue
            declared.setdefault(dep_name, []).append((name, dep_spec))

    conflicts: List[Dict[str, Any]] = []
    for dep_name, sources in declared.items():
        if dep_name not in pinned:
            continue
        pin = pinned[dep_name]
        violations = [(src, spec) for (src, spec) in sources if spec and not _satisfies(pin, spec)]
        if not violations:
            continue
        specs = [spec for (_src, spec) in sources if spec]
        # Keep only NON-exact clauses of the package's own line (e.g. a '>=0.8'
        # range) -- never its '==' pin, which is exactly what we're replacing.
        self_spec = explicit_spec.get(dep_name)
        if self_spec:
            kept = [
                f"{op}{ref}"
                for op, ref in _parse_specifier(self_spec)
                if op not in ("==", "===")
            ]
            specs.extend(kept)
        suggestion = None
        if available_versions is not None:
            candidates = available_versions(dep_name) or []
            ok = [v for v in candidates if all(_satisfies(v, s) for s in specs)]
            if ok:
                suggestion = max(ok, key=_release_tuple)
        conflicts.append(
            {
                "package": dep_name,
                "pinned": pin,
                "violations": violations,
                "combined": ",".join(specs),
                "suggestion": suggestion,
            }
        )
    return conflicts


def _rewrite_pins(lines: List[str], fixes: Mapping[str, str]) -> List[str]:
    out: List[str] = []
    for line in lines:
        parsed = _parse_install_line(line)
        if parsed and parsed[0] in fixes and parsed[2] is not None:
            out.append(f"{parsed[0]}=={fixes[parsed[0]]}")
        else:
            out.append(line)
    return out


def apply_dependency_fixes(recipe: MutableMapping[str, Any], fixes: Mapping[str, str]) -> None:
    """Rewrite exact pins in ``framework.pip``/``framework.dual.backend_pip`` to
    the versions in ``fixes`` (``{package: version}``). Mutates ``recipe``."""

    if not fixes:
        return
    framework = recipe.get("framework")
    if not isinstance(framework, MutableMapping):
        return
    dual = framework.get("dual")
    if isinstance(dual, MutableMapping) and isinstance(dual.get("backend_pip"), list):
        dual["backend_pip"] = _rewrite_pins(dual["backend_pip"], fixes)
    if isinstance(framework.get("pip"), list):
        framework["pip"] = _rewrite_pins(framework["pip"], fixes)


def _packages_from_recipe(framework: Mapping[str, Any]) -> str:
    apt = [str(package) for package in (framework.get("apt") or []) if package]
    # Add system libs implied by a pip companion (e.g. torchcodec needs ffmpeg).
    pip_entries = [str(p) for p in (framework.get("pip") or []) if str(p).strip()]
    for pkg in _companion_requirements(pip_entries)[1]:
        if pkg not in apt:
            apt.append(pkg)
    return "\n".join(apt + [""]) if apt else ""


def _readme_from_recipe(recipe: Mapping[str, Any]) -> str:
    model = recipe["model"]
    name = str(model.get("name"))
    description = str(model.get("description") or "Generated HARP wrapper.")
    license_name = str(model.get("license") or "other").strip().lower()
    inputs = ", ".join(f"{spec['type']}" for spec in recipe["inputs"])
    outputs = ", ".join(f"{spec['type']}" for spec in recipe["outputs"])

    framework = recipe.get("framework") or {}
    if isinstance(framework, Mapping) and framework.get("backend"):
        # Plain-Gradio backend Space: pin the Space runtime's gradio to whatever
        # the recipe pins (so HF doesn't force a version that fights the model's
        # deps); fall back to the standard version when the recipe leaves it open.
        sdk_version = _recipe_gradio_pin(framework) or "5.28.0"
        # Legacy TF / research stacks need an older CPython: without an explicit
        # python_version, current Gradio Spaces may pick 3.12+ where tensorflow
        # 2.15 (and similar) have no wheels ("No matching distribution found").
        python_version = str(framework.get("python_version") or "").strip()
        if not python_version and _backend_needs_legacy_python(framework):
            python_version = "3.10"
        front_matter = [
            "---",
            f"title: {json.dumps(name)}",
            "colorFrom: indigo",
            "colorTo: gray",
            "sdk: gradio",
            f"sdk_version: {sdk_version}",
            "app_file: app.py",
            "pinned: false",
            f"license: {json.dumps(license_name)}",
        ]
        if python_version:
            front_matter.append(f'python_version: "{python_version}"')
        front_matter.append("---")
        return "\n".join(
            front_matter
            + [
                "",
                f"# {name} (backend)",
                "",
                description,
                "",
                "Plain-Gradio **backend** Space for the two-Space HARP workflow: it runs "
                "the model and exposes inference at the `/predict` API endpoint. It does "
                "NOT depend on pyharp. Deploy a thin HARP remote-backend frontend "
                "(`scaffold-remote-recipe <this-space>`) that proxies to it.",
                "",
                f"- Inputs: {inputs}",
                f"- Outputs: {outputs}",
                "",
                "Generated by the HARP model agent from a recipe.",
                "",
            ]
        )

    if isinstance(framework, Mapping) and framework.get("dual"):
        backend_python = str(framework["dual"].get("backend_python") or "3.9")
        return "\n".join(
            [
                "---",
                f"title: {json.dumps(name)}",
                "colorFrom: indigo",
                "colorTo: gray",
                "sdk: docker",
                "app_port: 7860",
                "pinned: false",
                f"license: {json.dumps(license_name)}",
                "---",
                "",
                f"# {name}",
                "",
                description,
                "",
                "Dual-interpreter HARP Space: a pyharp/Gradio frontend (Python 3.10) "
                f"drives an isolated model backend (Python {backend_python}) via a "
                "one-shot subprocess, so the backend's pinned dependencies never touch "
                "pyharp/Gradio.",
                "",
                f"- Inputs: {inputs}",
                f"- Outputs: {outputs}",
                "",
                "Generated by the HARP model agent from a recipe.",
                "",
            ]
        )

    return "\n".join(
        [
            "---",
            # Quote free-text values: a title/license containing a colon (e.g.
            # "SoulX-Singer: SVS") would otherwise break the YAML front matter.
            f"title: {json.dumps(name)}",
            "colorFrom: indigo",
            "colorTo: gray",
            "sdk: gradio",
            "sdk_version: 5.28.0",
            "app_file: app.py",
            "pinned: false",
            f"license: {json.dumps(license_name)}",
            "---",
            "",
            f"# {name}",
            "",
            description,
            "",
            f"- Inputs: {inputs}",
            f"- Outputs: {outputs}",
            "",
            "Generated by the HARP model agent from a recipe.",
            "",
        ]
    )


_GRADIO_TO_RECIPE_INPUT = {
    "Audio": "audio",
    "File": "file",
    "Dropdown": "dropdown",
    "Slider": "slider",
    "Textbox": "textbox",
    "Number": "number",
    "Checkbox": "checkbox",
}

_GRADIO_TO_RECIPE_OUTPUT = {
    "Audio": "audio",
    "File": "file",
    "JSON": "labels",
}

def _identifier(label: Any, fallback: str, used: set) -> str:
    base = re.sub(r"[^0-9A-Za-z]+", "_", str(label or "")).strip("_").lower()
    if not _IDENTIFIER_RE.match(base):
        base = fallback
    name = base
    counter = 2
    while name in used:
        name = f"{base}_{counter}"
        counter += 1
    used.add(name)
    return name


def _remote_param_name(param: Mapping[str, Any], index: int, used: set) -> str:
    """Best identifier for a backend parameter (prefer its ``parameter_name``)."""

    pname = str(param.get("parameter_name") or "").strip()
    if _IDENTIFIER_RE.match(pname) and pname not in used:
        used.add(pname)
        return pname
    return _identifier(param.get("label") or pname, f"arg_{index}", used)


# Endpoint names that usually ARE the model's main inference call.
_PRIMARY_ENDPOINT_NAMES = {
    "predict", "run", "generate", "infer", "inference", "process", "synthesize",
    "synthesis", "synthesis_function", "transcribe", "forward", "call", "api", "submit",
}
# Substrings that mark a UI control / housekeeping endpoint (never the inference).
_CONTROL_ENDPOINT_HINTS = (
    "interrupt", "toggle", "cancel", "stop", "reset", "clear", "login", "logout",
    "load", "change", "update", "select", "lambda", "refresh", "preview", "_run_",
)
# Substrings that mark an INTERNAL variant of the inference call (Gradio's batched
# fn, streaming/background helpers). These share a verb with the real endpoint
# (e.g. /predict_batched vs /predict_full) but take list-shaped args a single HARP
# call can't satisfy -- calling one yields an opaque "Internal Gradio error".
_INTERNAL_ENDPOINT_HINTS = (
    "batch", "batched", "internal", "stream", "background", "_fn", "warmup",
)
_MEDIA_RETURN_COMPONENTS = {"audio", "image", "video", "file", "gallery", "model3d"}


def rank_named_endpoints(api_info: Mapping[str, Any]) -> List["tuple[str, int]"]:
    """Rank a Space's named endpoints by how likely each is the inference call.

    Deterministic heuristic (no network/LLM): reward canonical names
    (``/predict`` etc.), returning media, and having parameters; penalize obvious
    UI-control endpoints (``/interrupt``, ``/toggle_*``, ...). Returns
    ``[(api_name, score), ...]`` sorted best-first, ties broken by name.
    """

    named = api_info.get("named_endpoints") if isinstance(api_info, Mapping) else None
    if not isinstance(named, Mapping):
        return []
    scored: List["tuple[str, int]"] = []
    for name, ep in named.items():
        ep = ep if isinstance(ep, Mapping) else {}
        short = str(name).lstrip("/").lower()
        score = 0
        if short in _PRIMARY_ENDPOINT_NAMES:
            score += 100
        elif any(verb in short for verb in _PRIMARY_ENDPOINT_NAMES):
            # Partial match rewards the user-facing variant, e.g. "predict_full"
            # or "generate_music", without over-scoring an exact control name.
            score += 40
        if any(hint in short for hint in _CONTROL_ENDPOINT_HINTS):
            score -= 100
        if any(hint in short for hint in _INTERNAL_ENDPOINT_HINTS):
            # e.g. /predict_batched, /predict_stream: internal siblings of the call.
            score -= 60
        returns = ep.get("returns") or []
        params = ep.get("parameters") or []
        if returns:
            score += 10
        if any(
            str(r.get("component", "")).lower() in _MEDIA_RETURN_COMPONENTS
            for r in returns
            if isinstance(r, Mapping)
        ):
            score += 20
        score += min(len(params), 10)
        scored.append((str(name), score))
    scored.sort(key=lambda item: (-item[1], item[0]))
    return scored


def guess_primary_endpoint(api_info: Mapping[str, Any]) -> Optional[str]:
    """Best-guess the primary inference endpoint, or None if there are none."""

    ranked = rank_named_endpoints(api_info)
    return ranked[0][0] if ranked else None


def _placeholder_slider_range(default: float) -> JSON:
    """Guess slider min/max/step that safely bracket a known default.

    Gradio's ``/info`` schema exposes a parameter's default but not its slider
    bounds. A default that falls outside ``[min, max]`` is a launch-time error in
    older Gradio, so we derive a range that always contains it (with head-room)
    and let the user tighten it via the recipe's ``_todo`` note. Integer-valued
    defaults get an integer step.
    """

    value = float(default)
    low = min(0.0, value)
    if value.is_integer():
        high = max(value * 2, value + 10)
        return {"min": low, "max": float(int(high)), "step": 1.0}
    high = max(value * 2, value + 1.0)
    return {"min": low, "max": high, "step": 0.1}


def _config_choice_values(props: Any) -> List[Any]:
    """Real dropdown/radio choice *values* from a Gradio config component.

    Gradio stores choices as ``[[label, value], ...]`` (older builds use a flat
    list); we return the callable values the backend expects.
    """

    if not isinstance(props, Mapping):
        return []
    choices = props.get("choices")
    if not isinstance(choices, (list, tuple)):
        return []
    values: List[Any] = []
    for choice in choices:
        if isinstance(choice, (list, tuple)) and len(choice) == 2:
            values.append(choice[1])
        else:
            values.append(choice)
    return [value for value in values if value is not None]


def _config_slider_range(props: Any) -> Optional[JSON]:
    """Real slider min/max/step from a Gradio config component, if present."""

    if not isinstance(props, Mapping):
        return None
    low = props.get("minimum")
    high = props.get("maximum")
    if not isinstance(low, (int, float)) or isinstance(low, bool):
        return None
    if not isinstance(high, (int, float)) or isinstance(high, bool):
        return None
    step = props.get("step")
    if not isinstance(step, (int, float)) or isinstance(step, bool) or not step:
        step = 1.0 if float(low).is_integer() and float(high).is_integer() else 0.1
    return {"min": low, "max": high, "step": step}


def component_props_for_endpoint(
    config: Any, api_name: Optional[str]
) -> Optional[List[Optional[JSON]]]:
    """Map a Gradio config's components onto a named endpoint's input parameters.

    Returns a list of ``props`` dicts aligned (by position) with the endpoint's
    ``/info`` parameters, so the scaffold can read each input's *real* dropdown
    choices / slider bounds (which ``/info`` omits). ``None`` when the config is
    unusable or the endpoint can't be matched -- the caller then falls back to the
    placeholder behavior.
    """

    if not isinstance(config, Mapping):
        return None
    components = config.get("components")
    dependencies = config.get("dependencies")
    if not isinstance(components, list) or not isinstance(dependencies, list):
        return None
    by_id = {
        comp.get("id"): comp
        for comp in components
        if isinstance(comp, Mapping) and comp.get("id") is not None
    }
    want = (api_name or "").lstrip("/")
    for dep in dependencies:
        if not isinstance(dep, Mapping):
            continue
        if str(dep.get("api_name") or "").lstrip("/") != want:
            continue
        inputs = dep.get("inputs")
        if not isinstance(inputs, list):
            return None
        props: List[Optional[JSON]] = []
        for cid in inputs:
            comp = by_id.get(cid)
            comp_props = comp.get("props") if isinstance(comp, Mapping) else None
            props.append(comp_props if isinstance(comp_props, Mapping) else None)
        return props
    return None


def remote_recipe_from_api_info(
    space: str,
    api_info: Mapping[str, Any],
    *,
    api_name: Optional[str] = None,
    model_name: str = "",
    user_token: bool = False,
    component_props: Optional[Sequence[Optional[Mapping[str, Any]]]] = None,
) -> JSON:
    """Scaffold a remote-backend recipe from a Gradio ``/info`` API schema.

    Maps a backend named endpoint's positional parameters to HARP inputs and its
    returns to HARP outputs, producing a ``framework.remote`` block whose ``args``
    line up with the backend's call signature. Parameters whose component type has
    no HARP equivalent are emitted as ``{"const": <default>}`` so the positional
    signature stays aligned.

    ``component_props`` (from the Space's live Gradio config, aligned by position
    with the endpoint's parameters) supplies the real dropdown choices and slider
    bounds that ``/info`` omits. When it isn't available the scaffold falls back to
    safe placeholders (choices seeded with the known default, slider range guessed
    to bracket the default) that the user tightens via the ``_todo`` notes.
    """

    named = api_info.get("named_endpoints") if isinstance(api_info, Mapping) else None
    if not isinstance(named, Mapping) or not named:
        raise RecipeError(
            "The backend Space exposes no named API endpoints, so gradio_client "
            "cannot call it (the author likely set api_name=False / show_api=False). "
            "This model must be duplicated rather than proxied."
        )

    if api_name:
        if api_name not in named:
            raise RecipeError(
                f"api_name '{api_name}' not found. Available endpoints: "
                + ", ".join(sorted(named))
            )
        chosen = api_name
    elif len(named) == 1:
        chosen = next(iter(named))
    else:
        best = guess_primary_endpoint(api_info)
        hint = f" (best guess: {best})" if best else ""
        raise RecipeError(
            "The backend exposes multiple named endpoints; pass --api-name to pick "
            "one" + hint + ". Available: " + ", ".join(sorted(named))
            + ". Or let the agent choose: scaffold-remote-recipe --auto-endpoint, or "
            "generate-recipe-from-llm --remote-space <space> --remote-llm (no "
            "--remote-api-name) to have the LLM pick."
        )

    endpoint = named[chosen] if isinstance(named[chosen], Mapping) else {}
    parameters = endpoint.get("parameters") or []
    returns = endpoint.get("returns") or []

    used_names: set = set()
    todos: List[str] = []

    inputs: List[JSON] = []
    args: List[JSON] = []
    for index, param in enumerate(parameters):
        param = param if isinstance(param, Mapping) else {}
        component = str(param.get("component") or "")
        recipe_type = _GRADIO_TO_RECIPE_INPUT.get(component)
        default = param.get("parameter_default")
        if recipe_type is None:
            # No HARP equivalent (State/Image/Video/...): keep the slot aligned by
            # sending the backend's own default as a constant.
            args.append({"const": default})
            todos.append(
                f"framework.remote.args[{index}]: backend arg (component "
                f"'{component or 'unknown'}') is sent as a constant {default!r}; "
                "adjust if it should be user-controlled"
            )
            continue

        name = _remote_param_name(param, index, used_names)
        label = str(param.get("label") or name)
        spec: JSON = {"name": name, "type": recipe_type, "label": label}

        props = (
            component_props[index]
            if component_props is not None and index < len(component_props)
            else None
        )

        if recipe_type == "dropdown":
            real_choices = _config_choice_values(props)
            if real_choices:
                # The live config exposes the true option list.
                spec["choices"] = real_choices
                spec["default"] = default if default in real_choices else real_choices[0]
            elif default is not None:
                # /info gives only the default; seed choices with it so the app is
                # valid (a default outside the choice list makes Gradio raise at
                # launch). The user adds the rest via the _todo note.
                spec["choices"] = [default]
                spec["default"] = default
                todos.append(
                    f"inputs.{name}.choices: only the default is known; "
                    "add the other real dropdown options"
                )
            else:
                spec["choices"] = ["TODO_option_1"]
                spec["default"] = "TODO_option_1"
                todos.append(f"inputs.{name}.choices: set the real dropdown options")
        elif recipe_type == "slider":
            real_range = _config_slider_range(props)
            if real_range:
                # The live config exposes the true min/max/step.
                spec.update(real_range)
                if isinstance(default, (int, float)) and not isinstance(default, bool):
                    spec["default"] = default
            elif isinstance(default, (int, float)) and not isinstance(default, bool):
                # /info gives only the default; guess a range that brackets it so
                # the control renders (out-of-range is a launch-time error in
                # older Gradio). User fixes it via the _todo.
                spec.update(_placeholder_slider_range(default))
                spec["default"] = default
                todos.append(
                    f"inputs.{name}: set the real slider min/max/step "
                    "(guessed to bracket the default)"
                )
            else:
                spec["min"] = 0.0
                spec["max"] = 1.0
                spec["step"] = 0.1
                todos.append(f"inputs.{name}: set the real slider min/max/step")
        elif recipe_type == "checkbox":
            spec["default"] = bool(default)
        elif recipe_type in {"textbox", "number"} and default is not None:
            spec["default"] = default

        inputs.append(spec)
        arg: JSON = {"from": name}
        if recipe_type in _MEDIA_TYPES:
            arg["file"] = True
        args.append(arg)

    outputs: List[JSON] = []
    ret_map: List[JSON] = []
    for index, ret in enumerate(returns):
        ret = ret if isinstance(ret, Mapping) else {}
        component = str(ret.get("component") or "")
        recipe_type = _GRADIO_TO_RECIPE_OUTPUT.get(component)
        if recipe_type is None:
            continue
        name = _identifier(ret.get("label"), f"output_{index}", used_names)
        outputs.append(
            {"name": name, "type": recipe_type, "label": str(ret.get("label") or name)}
        )
        ret_map.append({"index": index, "to": name})

    if not inputs:
        raise RecipeError(
            f"Could not derive any user inputs from backend endpoint '{chosen}'."
        )
    if not outputs:
        raise RecipeError(
            f"Backend endpoint '{chosen}' returns no recognized outputs "
            "(audio/file/labels); cannot scaffold a HARP wrapper."
        )

    parts = str(space).split("/")
    author = parts[0] if len(parts) == 2 else "TODO-author"
    display_name = model_name or parts[-1].replace("-", " ").replace("_", " ").title()

    todos.extend(
        [
            "model.description: describe what the model does",
            "model.tags: add Hugging Face/HARP tags",
            "framework.remote.args: mark any fixed args as {\"const\": ...} and drop "
            "ones the UI should not expose (e.g. hidden metadata)",
            "Reliability: pointing at someone else's live Space is an unpinned runtime "
            "dependency -- duplicate it under your org for anything you depend on.",
        ]
    )

    remote: JSON = {
        "space": str(space),
        "api_name": chosen,
        "token_env": "HF_TOKEN",
        "args": args,
        "returns": ret_map,
    }
    if user_token:
        # Expose a masked per-user token control so ZeroGPU usage is charged to the
        # calling user instead of this Space's (shared/anonymous) identity.
        remote["user_token"] = True
        todos.append(
            "framework.remote.user_token is on: the UI shows an optional HF token "
            "field; usage is attributed to the user's account when they provide one."
        )

    return {
        "_source": f"{space} :: {chosen}",
        "_todo": todos,
        "model": {
            "id": str(space),
            "name": display_name,
            "description": "TODO: describe this model.",
            "author": author,
            "tags": [],
        },
        "framework": {
            "gpu": False,
            "pip": [],
            "remote": remote,
        },
        "inputs": inputs,
        "outputs": outputs,
    }


def build_package_from_recipe(recipe: Mapping[str, Any]) -> GeneratedAppPackage:
    """Build the in-memory files for a pyharp wrapper from a recipe."""

    # Backend recipes may omit python_version; fill it before validate/render so
    # the README front matter always carries a pin when legacy TF deps need it.
    if isinstance(recipe, MutableMapping):
        ensure_backend_runtime_defaults(recipe)

    validate_recipe(recipe)

    model = recipe["model"]
    framework = recipe.get("framework") or {}
    remote = framework.get("remote") if isinstance(framework, Mapping) else None
    dual = framework.get("dual") if isinstance(framework, Mapping) else None
    is_backend = bool(framework.get("backend")) if isinstance(framework, Mapping) else False
    repo_id = str(model.get("id"))
    tags = list(model.get("tags") or [])
    task = str(model.get("task") or (tags[0] if tags else "custom"))
    if dual:
        framework_name = "dual-interpreter"
    elif remote:
        framework_name = "gradio_client"
    elif is_backend:
        framework_name = "gradio-backend"
    else:
        framework_name = str(framework.get("import") or "custom")

    io = {
        "inputs": [str(spec.get("type")) for spec in recipe["inputs"]],
        "outputs": [str(spec.get("type")) for spec in recipe["outputs"]],
    }
    manifest = {
        "repo_id": repo_id,
        "task": task,
        "framework": framework_name,
        "io": io,
        "entry": "frontend_app.py" if dual else "app.py",
        "space_layout": "huggingface-docker" if dual else "huggingface-gradio",
        "generated": True,
        "source": "recipe",
    }
    if remote:
        manifest["deploy_mode"] = "remote-backend"
        manifest["backend_space"] = str(remote.get("space") or "")
    if is_backend:
        manifest["deploy_mode"] = "backend"

    if dual:
        # A dual package is a Docker Space bundle: the modern pyharp frontend, the
        # isolated backend worker, the two split requirement sets, the Dockerfile
        # that builds both venvs, and start.sh. There is no top-level app.py.
        manifest["deploy_mode"] = "dual-interpreter"
        manifest["space_sdk"] = "docker"
        extra_files = {
            "frontend_app.py": _render_dual_frontend(recipe),
            "backend_worker.py": _render_dual_backend_worker(recipe),
            "Dockerfile": _render_dual_dockerfile(recipe),
            "start.sh": _render_dual_start_sh(),
            "requirements-frontend.txt": _dual_frontend_requirements(),
            "requirements-backend.txt": _dual_backend_requirements(dual),
        }
        return GeneratedAppPackage(
            repo_id=repo_id,
            task=task,
            framework=framework_name,
            score={
                "score": None,
                "blockers": [],
                "rationale": "generated from recipe",
                "task": task,
            },
            io=io,
            app_py="",
            requirements="",
            readme=_readme_from_recipe(recipe),
            packages_txt="",
            manifest=manifest,
            extra_files=extra_files,
        )

    requirements = (
        _backend_requirements_from_recipe(framework)
        if is_backend
        else _requirements_from_recipe(framework)
    )
    extra_files: JSON = {}
    if is_backend and isinstance(framework, Mapping):
        vendor = framework.get("vendor_files")
        if isinstance(vendor, Mapping):
            for name, text in vendor.items():
                if isinstance(text, str) and str(name).strip():
                    # Only allow simple relative filenames (no path traversal).
                    safe = Path(str(name).replace("\\", "/")).name
                    if safe.endswith(".py"):
                        extra_files[safe] = text
    return GeneratedAppPackage(
        repo_id=repo_id,
        task=task,
        framework=framework_name,
        score={"score": None, "blockers": [], "rationale": "generated from recipe", "task": task},
        io=io,
        app_py=render_app_from_recipe(recipe),
        requirements=requirements,
        readme=_readme_from_recipe(recipe),
        packages_txt=_packages_from_recipe(framework),
        manifest=manifest,
        extra_files=extra_files,
    )
