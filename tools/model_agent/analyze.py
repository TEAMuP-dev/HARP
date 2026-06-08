"""Static analysis of harvested pyharp ``app.py`` wrappers.

This reads downloaded Hugging Face Space ``app.py`` files and reports the
input/output component shapes, GPU usage, and pyharp import styles across the
corpus. It parses with :mod:`ast` and never executes the files, so it is safe to
run on arbitrary downloaded code.

The goal is to validate the wrapper "recipe" schema against real wrappers: how
many inputs/outputs do real HARP models use, which Gradio component types, do
they rely on ``@spaces.GPU``, and which pyharp import paths appear.
"""

from __future__ import annotations

import ast
from collections import Counter
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

JSON = Dict[str, Any]

GRADIO_MODULES = {"gr", "gradio"}


def _kw_str(call: ast.Call, name: str) -> Optional[str]:
    for keyword in call.keywords:
        if (
            keyword.arg == name
            and isinstance(keyword.value, ast.Constant)
            and isinstance(keyword.value.value, str)
        ):
            return keyword.value.value
    return None


def _unwrap_chain(call: ast.Call) -> Tuple[ast.AST, set]:
    """Descend a method chain like ``gr.Audio(...).harp_required(True)``.

    Returns the base call (``gr.Audio(...)``) and the set of chained method
    names (e.g. ``{"harp_required"}``).
    """

    flags: set = set()
    node: ast.AST = call
    while (
        isinstance(node, ast.Call)
        and isinstance(node.func, ast.Attribute)
        and isinstance(node.func.value, ast.Call)
    ):
        flags.add(node.func.attr)
        node = node.func.value
    return node, flags


def _gradio_component(node: ast.AST) -> Optional[JSON]:
    """Resolve an AST node to a Gradio component descriptor, if it is one."""

    if not isinstance(node, ast.Call):
        return None

    base, flags = _unwrap_chain(node)
    if (
        isinstance(base, ast.Call)
        and isinstance(base.func, ast.Attribute)
        and isinstance(base.func.value, ast.Name)
        and base.func.value.id in GRADIO_MODULES
    ):
        kwargs = {kw.arg for kw in base.keywords if kw.arg}
        return {
            "type": base.func.attr,
            "label": _kw_str(base, "label"),
            "harp_required": "harp_required" in flags,
            "has_choices": "choices" in kwargs,
        }
    return None


def _is_spaces_gpu(decorator: ast.AST) -> bool:
    target = decorator.func if isinstance(decorator, ast.Call) else decorator
    return (
        isinstance(target, ast.Attribute)
        and target.attr == "GPU"
        and isinstance(target.value, ast.Name)
        and target.value.id == "spaces"
    )


def _is_build_endpoint(call: ast.Call) -> bool:
    func = call.func
    if isinstance(func, ast.Name):
        return func.id == "build_endpoint"
    if isinstance(func, ast.Attribute):
        return func.attr == "build_endpoint"
    return False


def _resolve_components(
    list_node: ast.AST,
    var_components: Dict[str, JSON],
) -> List[JSON]:
    if not isinstance(list_node, ast.List):
        return [{"type": "dynamic", "label": None, "harp_required": False, "has_choices": False}]

    resolved: List[JSON] = []
    for element in list_node.elts:
        if isinstance(element, ast.Name) and element.id in var_components:
            resolved.append(var_components[element.id])
        else:
            component = _gradio_component(element)
            resolved.append(
                component
                or {"type": "unknown", "label": None, "harp_required": False, "has_choices": False}
            )
    return resolved


UNRESOLVED_TYPES = {"dynamic", "unknown"}


def _annotate_eligibility(record: JSON) -> JSON:
    """Mark whether a record is usable as recipe-design input.

    A wrapper is "recipe eligible" only when its input/output component shapes
    are fully resolved statically. Wrappers whose components are passed as a
    variable (``dynamic``) or that expose no resolvable components are skipped
    automatically so the recipe corpus is built only from clean wrappers.
    """

    if record.get("error"):
        record["recipe_eligible"] = False
        record["unresolved_reason"] = "parse error"
        return record

    if not record.get("build_endpoint_found"):
        record["recipe_eligible"] = False
        record["unresolved_reason"] = "no build_endpoint"
        return record

    inputs = record.get("inputs", [])
    outputs = record.get("outputs", [])

    if not inputs or not outputs:
        record["recipe_eligible"] = False
        record["unresolved_reason"] = "no resolvable components"
        return record

    if any(component in UNRESOLVED_TYPES for component in inputs + outputs):
        record["recipe_eligible"] = False
        record["unresolved_reason"] = "dynamic/unresolved component types"
        return record

    record["recipe_eligible"] = True
    record["unresolved_reason"] = ""
    return record


def analyze_app_source(source: str) -> JSON:
    """Analyze a single ``app.py`` source string into a shape descriptor."""

    try:
        tree = ast.parse(source)
    except SyntaxError as exc:
        return _annotate_eligibility({"error": f"syntax error: {exc}"})

    var_components: Dict[str, JSON] = {}
    pyharp_imports: List[str] = []
    uses_spaces_gpu = False
    imports_spaces = False
    uses_labellist = False
    build_endpoint_call: Optional[ast.Call] = None

    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            for alias in node.names:
                if alias.name == "spaces" or alias.name.startswith("spaces."):
                    imports_spaces = True
                if alias.name.startswith("pyharp"):
                    pyharp_imports.append(alias.name)
        elif isinstance(node, ast.ImportFrom):
            module = node.module or ""
            if module == "spaces" or module.startswith("spaces."):
                imports_spaces = True
            if module.startswith("pyharp"):
                pyharp_imports.append(module)
                if any(alias.name == "LabelList" for alias in node.names):
                    uses_labellist = True
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            if any(_is_spaces_gpu(dec) for dec in node.decorator_list):
                uses_spaces_gpu = True
        elif isinstance(node, ast.Assign):
            if len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
                component = _gradio_component(node.value)
                if component:
                    var_components[node.targets[0].id] = component
        elif isinstance(node, ast.Call):
            if _is_build_endpoint(node) and build_endpoint_call is None:
                build_endpoint_call = node
            if isinstance(node.func, ast.Name) and node.func.id == "LabelList":
                uses_labellist = True

    inputs: List[JSON] = []
    outputs: List[JSON] = []
    if build_endpoint_call is not None:
        for keyword in build_endpoint_call.keywords:
            if keyword.arg == "input_components":
                inputs = _resolve_components(keyword.value, var_components)
            elif keyword.arg == "output_components":
                outputs = _resolve_components(keyword.value, var_components)

    return _annotate_eligibility(
        {
            "build_endpoint_found": build_endpoint_call is not None,
            "inputs": [component["type"] for component in inputs],
            "outputs": [component["type"] for component in outputs],
            "input_details": inputs,
            "output_details": outputs,
            "uses_spaces_gpu": uses_spaces_gpu,
            "imports_spaces": imports_spaces,
            "uses_labellist": uses_labellist,
            "pyharp_imports": sorted(set(pyharp_imports)),
        }
    )


def analyze_app_file(path: Path) -> JSON:
    path = Path(path)
    try:
        source = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return _annotate_eligibility({"path": str(path), "error": f"read failed: {exc}"})

    record = analyze_app_source(source)
    record["path"] = str(path)
    return record


def summarize(records: List[JSON]) -> JSON:
    input_types: Counter = Counter()
    output_types: Counter = Counter()
    input_counts: Counter = Counter()
    output_counts: Counter = Counter()
    pyharp_modules: Counter = Counter()
    parsed = 0
    errors = 0
    gpu = 0
    labels = 0
    missing_endpoint = 0
    eligible = 0
    unresolved_apps: List[JSON] = []

    for record in records:
        if record.get("recipe_eligible"):
            eligible += 1
        elif not record.get("error"):
            unresolved_apps.append(
                {
                    "path": record.get("path", ""),
                    "reason": record.get("unresolved_reason", "unresolved"),
                }
            )

        if record.get("error"):
            errors += 1
            continue
        parsed += 1
        if not record.get("build_endpoint_found"):
            missing_endpoint += 1
        for component in record.get("inputs", []):
            input_types[component] += 1
        for component in record.get("outputs", []):
            output_types[component] += 1
        input_counts[len(record.get("inputs", []))] += 1
        output_counts[len(record.get("outputs", []))] += 1
        if record.get("uses_spaces_gpu"):
            gpu += 1
        if record.get("uses_labellist"):
            labels += 1
        for module in record.get("pyharp_imports", []):
            pyharp_modules[module] += 1

    return {
        "apps_analyzed": parsed,
        "apps_with_errors": errors,
        "apps_without_build_endpoint": missing_endpoint,
        "recipe_eligible": eligible,
        "unresolved": len(unresolved_apps),
        "unresolved_apps": unresolved_apps,
        "input_component_types": dict(input_types),
        "output_component_types": dict(output_types),
        "input_count_distribution": {str(k): v for k, v in sorted(input_counts.items())},
        "output_count_distribution": {str(k): v for k, v in sorted(output_counts.items())},
        "uses_spaces_gpu": gpu,
        "uses_labellist": labels,
        "pyharp_import_modules": dict(pyharp_modules),
    }


def analyze_path(path: Path, *, filename: str = "app.py") -> JSON:
    """Analyze a single ``app.py`` file or every ``app.py`` under a directory."""

    path = Path(path)
    if path.is_file():
        records = [analyze_app_file(path)]
    else:
        records = [analyze_app_file(found) for found in sorted(path.rglob(filename))]

    return {"apps": records, "summary": summarize(records)}
