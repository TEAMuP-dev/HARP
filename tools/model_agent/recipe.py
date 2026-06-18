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
from typing import Any, Dict, List, Mapping

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

    inference = recipe.get("inference")
    if not isinstance(inference, Mapping) or not str(inference.get("body") or "").strip():
        errors.append("inference.body (the process function body) is required")

    if errors:
        raise RecipeError("Invalid recipe: " + "; ".join(errors))


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

    model = recipe["model"]
    framework = recipe.get("framework") or {}
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


def _requirements_from_recipe(framework: Mapping[str, Any]) -> str:
    requirements = list(_BASE_REQUIREMENTS)
    for package in framework.get("pip", []) or []:
        if package and package not in requirements:
            requirements.append(str(package))
    return "\n".join(requirements + [""])


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


def build_package_from_recipe(recipe: Mapping[str, Any]) -> GeneratedAppPackage:
    """Build the in-memory files for a pyharp wrapper from a recipe."""

    validate_recipe(recipe)

    model = recipe["model"]
    framework = recipe.get("framework") or {}
    repo_id = str(model.get("id"))
    tags = list(model.get("tags") or [])
    task = str(model.get("task") or (tags[0] if tags else "custom"))

    io = {
        "inputs": [str(spec.get("type")) for spec in recipe["inputs"]],
        "outputs": [str(spec.get("type")) for spec in recipe["outputs"]],
    }
    manifest = {
        "repo_id": repo_id,
        "task": task,
        "framework": str(framework.get("import") or "custom"),
        "io": io,
        "entry": "app.py",
        "space_layout": "huggingface-gradio",
        "generated": True,
        "source": "recipe",
    }

    return GeneratedAppPackage(
        repo_id=repo_id,
        task=task,
        framework=str(framework.get("import") or "custom"),
        score={"score": None, "blockers": [], "rationale": "generated from recipe", "task": task},
        io=io,
        app_py=render_app_from_recipe(recipe),
        requirements=_requirements_from_recipe(framework),
        readme=_readme_from_recipe(recipe),
        packages_txt=_packages_from_recipe(framework),
        manifest=manifest,
    )
