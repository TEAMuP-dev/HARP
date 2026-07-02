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
from typing import Any, Dict, List, Mapping, Optional, Tuple

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
    "git+https://github.com/TEAMuP-dev/pyharp.git@v0.3.0",
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

    if is_remote:
        # Remote-backend recipes proxy to a Space, so the inference glue is not
        # required; instead the remote block itself must be well-formed.
        errors.extend(_validate_remote(remote, input_names, output_names))
    else:
        inference = recipe.get("inference")
        if not isinstance(inference, Mapping) or not str(inference.get("body") or "").strip():
            errors.append("inference.body (the process function body) is required")

    if errors:
        raise RecipeError("Invalid recipe: " + "; ".join(errors))


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


def _component_code(spec: Mapping[str, Any], *, is_input: bool) -> str:
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
        types = (
            f", file_types={json.dumps([str(item) for item in file_types])}"
            if isinstance(file_types, list) and file_types
            else ""
        )
        code = f'gr.File(type="filepath", label={label}{types})'
    elif comp_type == "dropdown":
        choices = json.dumps(list(spec.get("choices", [])))
        default = spec.get("default")
        value = f", value={json.dumps(default)}" if default is not None else ""
        code = f"gr.Dropdown(choices={choices}{value}, label={label}{info_kwarg})"
    elif comp_type == "slider":
        minimum = json.dumps(spec.get("min"))
        maximum = json.dumps(spec.get("max"))
        step = f", step={json.dumps(spec.get('step'))}" if spec.get("step") is not None else ""
        default = (
            f", value={json.dumps(spec.get('default'))}" if spec.get("default") is not None else ""
        )
        code = f"gr.Slider(minimum={minimum}, maximum={maximum}{step}{default}, label={label}{info_kwarg})"
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

    if is_input and spec.get("required"):
        code += ".harp_required(True)"
    if info and comp_type in _MEDIA_TYPES:
        code += f".set_info({json.dumps(info)})"
    return code


def _indent(code: str, spaces: int = 4) -> str:
    pad = " " * spaces
    return "\n".join((pad + line if line.strip() else line) for line in code.splitlines())


def render_app_from_recipe(recipe: Mapping[str, Any]) -> str:
    """Render a runnable pyharp ``app.py`` from a validated recipe."""

    validate_recipe(recipe)

    framework = recipe.get("framework") or {}
    if isinstance(framework, Mapping) and framework.get("remote"):
        return _render_remote_app(recipe)

    model = recipe["model"]
    inputs = recipe["inputs"]
    outputs = recipe["outputs"]
    inference = recipe["inference"]

    uses_gpu = bool(framework.get("gpu"))

    import_lines = ["import gradio as gr"]
    if uses_gpu:
        import_lines.append(_SPACES_IMPORT_BLOCK)
    import_lines.append("")
    # Match HARP's reference wrappers: a star import exposes every pyharp
    # helper (ModelCard, build_endpoint, LabelList, AudioLabel, MidiLabel,
    # load_audio, save_audio, ...) that inference glue may reach for.
    import_lines.append("from pyharp import *")

    setup = str(inference.get("setup") or "").strip()
    arg_names = ", ".join(str(spec["name"]) for spec in inputs)
    body = _indent(str(inference["body"]).strip() or "pass")
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
    if entry.get("file"):
        return f"handle_file({name})"
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

    arg_names = ", ".join(str(spec["name"]) for spec in inputs)

    call_args = ",\n".join(
        "        " + _remote_arg_expr(entry) for entry in remote.get("args", [])
    )

    index_by_output = {
        str(entry["to"]): int(entry["index"]) for entry in remote.get("returns", [])
    }

    body_lines = [
        "    _raw = _backend_client().predict(",
    ]
    if call_args:
        body_lines.append(call_args + ",")
    body_lines += [
        f"        api_name={json.dumps(api_name)},",
        "    )",
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

    input_lines = ",\n".join(
        "        " + _component_code(spec, is_input=True) for spec in inputs
    )
    output_lines = ",\n".join(
        "        " + _component_code(spec, is_input=False) for spec in outputs
    )

    parts = [
        "from __future__ import annotations",
        "",
        "import os",
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
        "_client = None",
        "",
        "",
        "def _backend_client():",
        "    # Lazily create and cache one warm connection to the backend Space.",
        "    global _client",
        "    if _client is None:",
        "        _token = os.environ.get(_BACKEND_TOKEN_ENV) or None",
        "        _client = Client(_BACKEND_SPACE, hf_token=_token)",
        "    return _client",
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


def _packages_from_recipe(framework: Mapping[str, Any]) -> str:
    apt = [str(package) for package in (framework.get("apt") or []) if package]
    return "\n".join(apt + [""]) if apt else ""


def _readme_from_recipe(recipe: Mapping[str, Any]) -> str:
    model = recipe["model"]
    name = str(model.get("name"))
    description = str(model.get("description") or "Generated HARP wrapper.")
    license_name = str(model.get("license") or "other").strip().lower()
    inputs = ", ".join(f"{spec['type']}" for spec in recipe["inputs"])
    outputs = ", ".join(f"{spec['type']}" for spec in recipe["outputs"])

    return "\n".join(
        [
            "---",
            f"title: {name}",
            "colorFrom: indigo",
            "colorTo: gray",
            "sdk: gradio",
            "sdk_version: 5.28.0",
            "app_file: app.py",
            "pinned: false",
            f"license: {license_name}",
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

_STUB_SETUP = "# TODO: import the model package and load the model here."
_STUB_BODY = (
    "# TODO: run inference on the inputs above and return the outputs in order.\n"
    'raise NotImplementedError("Fill in the inference body for this recipe.")'
)


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


def recipe_skeleton_from_analysis(record: Mapping[str, Any], *, model_id: str = "") -> JSON:
    """Build a recipe *skeleton* from a (recipe-eligible) ``analyze`` record.

    Input/output components are filled in from the statically-resolved shapes;
    the parts that cannot be derived from a wrapper's surface (the model's
    dependencies and the inference glue, plus dropdown choices and slider
    ranges) are left as clearly-marked TODO placeholders. The result is itself a
    valid recipe that renders to a stub wrapper which raises ``NotImplementedError``.
    """

    used_names: set = set()
    todos: List[str] = []

    inputs: List[JSON] = []
    for index, detail in enumerate(record.get("input_details", [])):
        gradio_type = str(detail.get("type"))
        recipe_type = _GRADIO_TO_RECIPE_INPUT.get(gradio_type)
        if recipe_type is None:
            raise RecipeError(
                f"input component type '{gradio_type}' has no recipe mapping"
            )
        label = detail.get("label")
        spec: JSON = {
            "name": _identifier(label, f"input_{index}", used_names),
            "type": recipe_type,
            "label": str(label) if label else recipe_type.title(),
        }
        if detail.get("harp_required"):
            spec["required"] = True
        if detail.get("info"):
            spec["info"] = str(detail["info"])
        if recipe_type == "dropdown":
            choices = detail.get("choices")
            if isinstance(choices, list) and choices:
                spec["choices"] = list(choices)
                if detail.get("default") is not None:
                    spec["default"] = detail["default"]
            else:
                spec["choices"] = ["TODO_option_1", "TODO_option_2"]
                todos.append(f"inputs.{spec['name']}.choices: set the real dropdown options")
        if recipe_type == "slider":
            has_range = detail.get("min") is not None and detail.get("max") is not None
            spec["min"] = detail["min"] if detail.get("min") is not None else 0.0
            spec["max"] = detail["max"] if detail.get("max") is not None else 1.0
            if detail.get("step") is not None:
                spec["step"] = detail["step"]
            elif not has_range:
                spec["step"] = 0.1
            if detail.get("default") is not None:
                spec["default"] = detail["default"]
            if not has_range:
                todos.append(f"inputs.{spec['name']}: set the real slider min/max/step/default")
        if recipe_type in {"textbox", "number", "checkbox"} and detail.get("default") is not None:
            spec["default"] = detail["default"]
        if recipe_type == "file" and isinstance(detail.get("file_types"), list):
            spec["file_types"] = list(detail["file_types"])
        inputs.append(spec)

    outputs: List[JSON] = []
    for index, detail in enumerate(record.get("output_details", [])):
        gradio_type = str(detail.get("type"))
        recipe_type = _GRADIO_TO_RECIPE_OUTPUT.get(gradio_type)
        if recipe_type is None:
            raise RecipeError(
                f"output component type '{gradio_type}' has no recipe mapping"
            )
        label = detail.get("label")
        out_spec: JSON = {
            "name": _identifier(label, f"output_{index}", used_names),
            "type": recipe_type,
            "label": str(label) if label else recipe_type.title(),
        }
        if detail.get("info"):
            out_spec["info"] = str(detail["info"])
        if recipe_type == "file" and isinstance(detail.get("file_types"), list):
            out_spec["file_types"] = list(detail["file_types"])
        outputs.append(out_spec)

    identifier = model_id or "TODO-author/TODO-model"
    parts = identifier.split("/")
    author = parts[0] if len(parts) == 2 else "TODO-author"
    display_name = parts[-1].replace("-", " ").replace("_", " ").title()

    todos.extend(
        [
            "model.description: describe what the model does",
            "model.tags: add Hugging Face/HARP tags",
            "framework.import / framework.pip: set the real package(s)",
            "inference.setup / inference.body: fill in model loading and inference",
        ]
    )

    return {
        "_source": str(record.get("path") or ""),
        "_todo": todos,
        "model": {
            "id": identifier,
            "name": display_name,
            "description": "TODO: describe this model.",
            "author": author,
            "tags": [],
        },
        "framework": {
            "import": "TODO_package",
            "pip": ["TODO-package"],
            "apt": [],
            "gpu": bool(record.get("uses_spaces_gpu")),
        },
        "inputs": inputs,
        "outputs": outputs,
        "inference": {"setup": _STUB_SETUP, "body": _STUB_BODY},
    }


def _remote_param_name(param: Mapping[str, Any], index: int, used: set) -> str:
    """Best identifier for a backend parameter (prefer its ``parameter_name``)."""

    pname = str(param.get("parameter_name") or "").strip()
    if _IDENTIFIER_RE.match(pname) and pname not in used:
        used.add(pname)
        return pname
    return _identifier(param.get("label") or pname, f"arg_{index}", used)


def remote_recipe_from_api_info(
    space: str,
    api_info: Mapping[str, Any],
    *,
    api_name: Optional[str] = None,
    model_name: str = "",
) -> JSON:
    """Scaffold a remote-backend recipe from a Gradio ``/info`` API schema.

    Maps a backend named endpoint's positional parameters to HARP inputs and its
    returns to HARP outputs, producing a ``framework.remote`` block whose ``args``
    line up with the backend's call signature. Parameters whose component type has
    no HARP equivalent are emitted as ``{"const": <default>}`` so the positional
    signature stays aligned; the user reviews/trims these (and any slider ranges /
    dropdown choices, which ``/info`` does not expose) before deploying.
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
        raise RecipeError(
            "The backend exposes multiple named endpoints; pass --api-name to pick "
            "one. Available: " + ", ".join(sorted(named))
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

        if recipe_type == "dropdown":
            spec["choices"] = ["TODO_option_1", "TODO_option_2"]
            if default is not None:
                spec["default"] = default
            todos.append(f"inputs.{name}.choices: set the real dropdown options")
        elif recipe_type == "slider":
            spec["min"] = 0.0
            spec["max"] = 1.0
            spec["step"] = 0.1
            if isinstance(default, (int, float)) and not isinstance(default, bool):
                spec["default"] = default
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
            "remote": {
                "space": str(space),
                "api_name": chosen,
                "token_env": "HF_TOKEN",
                "args": args,
                "returns": ret_map,
            },
        },
        "inputs": inputs,
        "outputs": outputs,
    }


def build_package_from_recipe(recipe: Mapping[str, Any]) -> GeneratedAppPackage:
    """Build the in-memory files for a pyharp wrapper from a recipe."""

    validate_recipe(recipe)

    model = recipe["model"]
    framework = recipe.get("framework") or {}
    remote = framework.get("remote") if isinstance(framework, Mapping) else None
    repo_id = str(model.get("id"))
    tags = list(model.get("tags") or [])
    task = str(model.get("task") or (tags[0] if tags else "custom"))
    framework_name = "gradio_client" if remote else str(framework.get("import") or "custom")

    io = {
        "inputs": [str(spec.get("type")) for spec in recipe["inputs"]],
        "outputs": [str(spec.get("type")) for spec in recipe["outputs"]],
    }
    manifest = {
        "repo_id": repo_id,
        "task": task,
        "framework": framework_name,
        "io": io,
        "entry": "app.py",
        "space_layout": "huggingface-gradio",
        "generated": True,
        "source": "recipe",
    }
    if remote:
        manifest["deploy_mode"] = "remote-backend"
        manifest["backend_space"] = str(remote.get("space") or "")

    return GeneratedAppPackage(
        repo_id=repo_id,
        task=task,
        framework=framework_name,
        score={"score": None, "blockers": [], "rationale": "generated from recipe", "task": task},
        io=io,
        app_py=render_app_from_recipe(recipe),
        requirements=_requirements_from_recipe(framework),
        readme=_readme_from_recipe(recipe),
        packages_txt=_packages_from_recipe(framework),
        manifest=manifest,
    )
