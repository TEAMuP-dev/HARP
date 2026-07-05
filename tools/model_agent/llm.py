"""Optional LLM-backed recipe generation for arbitrary models.

Hardcoded per-framework templates (see ``agent.render_pyharp_app``) only cover a
handful of frameworks and raise ``NotImplementedError`` for the long tail
(PyTorch Hub, Transformers, Diffusers, custom GitHub repos, ...). This module
lets an LLM write the two genuinely model-specific parts of a wrapper -- the
``inference.setup`` (imports + model loading) and ``inference.body`` (the
``process_fn`` body) -- while everything else stays deterministic.

The design is "LLM proposes, the deterministic pipeline disposes":

1. The LLM is constrained to emit a :mod:`recipe` JSON object, not a freehand
   ``app.py``. It only authors ``framework`` + ``inference`` (and may shape
   inputs/outputs); the model card fields are backfilled from real metadata.
2. The draft is run through :func:`recipe.validate_recipe`,
   :func:`recipe.render_app_from_recipe`, and a ``compile()`` check.
3. On failure the validation/syntax error is fed back for a bounded number of
   repair attempts.

This keeps the agent's core offline and stdlib-only: providers use
:mod:`urllib` (no SDKs), network calls happen only inside provider methods, and
the whole module is optional -- nothing else imports it at module load time.

Set an API key and pick a provider via env vars:
``GEMINI_API_KEY`` / ``GOOGLE_API_KEY`` (gemini), ``ANTHROPIC_API_KEY``
(anthropic), or ``OPENAI_API_KEY`` (openai); override the default with
``HARP_LLM_PROVIDER``.
"""

from __future__ import annotations

import json
import os
import re
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional

from .recipe import render_app_from_recipe, validate_recipe

JSON = Dict[str, Any]

_README_LIMIT = 6000
_SPACE_SOURCE_LIMIT = 4000


class LLMError(Exception):
    """Raised when an LLM provider is misconfigured or fails to produce a recipe."""


# --------------------------------------------------------------------------- #
# Providers (urllib only; no third-party SDKs)
# --------------------------------------------------------------------------- #

# Model names move fast; these are sane defaults but a key/region may differ.
# Use the `list-models` command (or --llm-model) to pick a valid one.
_DEFAULT_MODELS = {
    "gemini": "gemini-2.5-flash",
    "anthropic": "claude-sonnet-4-latest",
    "openai": "gpt-4o",
}


def _http_post_json(url: str, payload: JSON, headers: Mapping[str, str], timeout: float) -> JSON:
    body = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json", **dict(headers)},
        method="POST",
    )
    return _send(request, timeout)


def _http_get_json(url: str, headers: Mapping[str, str], timeout: float) -> JSON:
    request = urllib.request.Request(url, headers=dict(headers), method="GET")
    return _send(request, timeout)


def _send(request: urllib.request.Request, timeout: float) -> JSON:
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", "replace")
        hint = ""
        if exc.code == 404 and request.method == "POST":
            hint = " (model not found for this key/region; run `list-models` or pass --llm-model)"
        raise LLMError(f"{request.full_url} returned HTTP {exc.code}{hint}: {detail[:500]}") from exc
    except urllib.error.URLError as exc:
        raise LLMError(f"request to {request.full_url} failed: {exc}") from exc


def _loads_lenient(text: str) -> JSON:
    """Parse JSON from a model response, tolerating ```json fences / prose."""

    text = (text or "").strip()
    if text.startswith("```"):
        text = re.sub(r"^```[A-Za-z0-9]*\s*", "", text)
        text = re.sub(r"\s*```$", "", text).strip()
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        start, end = text.find("{"), text.rfind("}")
        if 0 <= start < end:
            try:
                return json.loads(text[start : end + 1])
            except json.JSONDecodeError:
                pass
    raise LLMError("LLM did not return valid JSON")


class LLMProvider:
    """Minimal provider interface: turn a (system, user) prompt into a JSON object."""

    name = "base"

    def __init__(self, *, api_key: str, model: str, timeout: float = 120.0, temperature: float = 0.2):
        self.api_key = api_key
        self.model = model
        self.timeout = timeout
        self.temperature = temperature

    def complete_json(self, system: str, user: str, *, schema: Optional[JSON] = None) -> JSON:
        raise NotImplementedError

    def list_models(self) -> List[str]:
        """List model names usable for content generation with this provider."""
        raise NotImplementedError


class GeminiProvider(LLMProvider):
    name = "gemini"

    def list_models(self) -> List[str]:
        url = f"https://generativelanguage.googleapis.com/v1beta/models?key={self.api_key}"
        data = _http_get_json(url, {}, self.timeout)
        models: List[str] = []
        for entry in data.get("models", []) if isinstance(data, Mapping) else []:
            methods = entry.get("supportedGenerationMethods") or []
            if "generateContent" in methods:
                models.append(str(entry.get("name", "")).split("/")[-1])
        return [name for name in models if name]

    def complete_json(self, system: str, user: str, *, schema: Optional[JSON] = None) -> JSON:
        url = (
            "https://generativelanguage.googleapis.com/v1beta/models/"
            f"{self.model}:generateContent?key={self.api_key}"
        )
        # Use JSON mode but DO NOT attach a strict responseSchema: Gemini's
        # structured output drops any field not declared in the schema, and our
        # recipe components are polymorphic (different fields per type), so a
        # strict schema would strip name/type/label and yield empty objects.
        # The detailed system prompt + few-shot examples constrain the shape.
        generation_config: JSON = {
            "responseMimeType": "application/json",
            "temperature": self.temperature,
        }
        payload = {
            "systemInstruction": {"parts": [{"text": system}]},
            "contents": [{"role": "user", "parts": [{"text": user}]}],
            "generationConfig": generation_config,
        }
        data = _http_post_json(url, payload, {}, self.timeout)
        try:
            text = data["candidates"][0]["content"]["parts"][0]["text"]
        except (KeyError, IndexError, TypeError) as exc:
            raise LLMError(f"unexpected Gemini response: {json.dumps(data)[:500]}") from exc
        return _loads_lenient(text)


class AnthropicProvider(LLMProvider):
    name = "anthropic"

    def list_models(self) -> List[str]:
        headers = {"x-api-key": self.api_key, "anthropic-version": "2023-06-01"}
        data = _http_get_json("https://api.anthropic.com/v1/models", headers, self.timeout)
        return [str(item.get("id")) for item in (data.get("data") or []) if item.get("id")]

    def complete_json(self, system: str, user: str, *, schema: Optional[JSON] = None) -> JSON:
        url = "https://api.anthropic.com/v1/messages"
        headers = {"x-api-key": self.api_key, "anthropic-version": "2023-06-01"}
        payload = {
            "model": self.model,
            "max_tokens": 4096,
            "temperature": self.temperature,
            "system": system,
            "messages": [{"role": "user", "content": user}],
        }
        data = _http_post_json(url, payload, headers, self.timeout)
        try:
            text = data["content"][0]["text"]
        except (KeyError, IndexError, TypeError) as exc:
            raise LLMError(f"unexpected Anthropic response: {json.dumps(data)[:500]}") from exc
        return _loads_lenient(text)


class OpenAIProvider(LLMProvider):
    name = "openai"

    def list_models(self) -> List[str]:
        headers = {"Authorization": f"Bearer {self.api_key}"}
        data = _http_get_json("https://api.openai.com/v1/models", headers, self.timeout)
        return [str(item.get("id")) for item in (data.get("data") or []) if item.get("id")]

    def complete_json(self, system: str, user: str, *, schema: Optional[JSON] = None) -> JSON:
        url = "https://api.openai.com/v1/chat/completions"
        headers = {"Authorization": f"Bearer {self.api_key}"}
        payload = {
            "model": self.model,
            "temperature": self.temperature,
            "response_format": {"type": "json_object"},
            "messages": [
                {"role": "system", "content": system},
                {"role": "user", "content": user},
            ],
        }
        data = _http_post_json(url, payload, headers, self.timeout)
        try:
            text = data["choices"][0]["message"]["content"]
        except (KeyError, IndexError, TypeError) as exc:
            raise LLMError(f"unexpected OpenAI response: {json.dumps(data)[:500]}") from exc
        return _loads_lenient(text)


_PROVIDERS = {
    "gemini": (GeminiProvider, ("GEMINI_API_KEY", "GOOGLE_API_KEY")),
    "anthropic": (AnthropicProvider, ("ANTHROPIC_API_KEY",)),
    "openai": (OpenAIProvider, ("OPENAI_API_KEY",)),
}


def provider_from_env(
    name: Optional[str] = None,
    *,
    model: Optional[str] = None,
    timeout: float = 120.0,
    temperature: float = 0.2,
) -> LLMProvider:
    """Construct a provider from CLI args + environment, auto-detecting if needed."""

    name = (name or os.environ.get("HARP_LLM_PROVIDER") or "").strip().lower()
    if not name:
        for candidate, (_cls, keys) in _PROVIDERS.items():
            if any(os.environ.get(key) for key in keys):
                name = candidate
                break
    # A generic key with no provider specified defaults to the first provider.
    if not name and os.environ.get("HARP_LLM_API_KEY"):
        name = next(iter(_PROVIDERS))
    if not name:
        raise LLMError(
            "No LLM provider configured. Set --provider and the matching API key "
            "env var (GEMINI_API_KEY / ANTHROPIC_API_KEY / OPENAI_API_KEY), or set "
            "the generic HARP_LLM_API_KEY."
        )
    if name not in _PROVIDERS:
        raise LLMError(f"Unknown LLM provider '{name}' (expected one of {sorted(_PROVIDERS)}).")

    provider_cls, keys = _PROVIDERS[name]
    api_key = next((os.environ[key] for key in keys if os.environ.get(key)), "")
    if not api_key:
        # Generic fallback so a single env var works for any provider (used by the
        # GUI widget's "LLM API key" field).
        api_key = (os.environ.get("HARP_LLM_API_KEY") or "").strip()
    if not api_key:
        raise LLMError(
            f"{name} selected but no API key found. Set one of {keys} (or the generic "
            "HARP_LLM_API_KEY) in the environment."
        )
    return provider_cls(
        api_key=api_key,
        model=model or _DEFAULT_MODELS[name],
        timeout=timeout,
        temperature=temperature,
    )


# --------------------------------------------------------------------------- #
# Prompting
# --------------------------------------------------------------------------- #

RECIPE_SYSTEM_PROMPT = """\
You generate HARP pyharp wrapper "recipes" as STRICT JSON. A recipe is a
declarative spec that a deterministic renderer turns into a runnable Gradio
app.py for the HARP audio plugin.

Return ONE JSON object with exactly these top-level keys:
  - "model":     {"id", "name", "description", "author", "tags": [...]}
  - "framework": {"import": "<top pip import name>", "pip": [...], "apt": [...], "gpu": bool}
  - "inputs":    ordered list of input components
  - "outputs":   ordered list of output components
  - "inference": {"setup": "<module-level python>", "body": "<process_fn body>"}

Input component types: audio, file, dropdown, slider, textbox, number, checkbox.
Output component types: audio, file, labels.
Every component needs "name" (a valid python identifier) and "type"; most need
"label". Extra fields:
  - dropdown: "choices": [...], optional "default"
  - slider:   "min", "max", optional "step", "default"
  - textbox/number/checkbox: optional "default"
  - any component: optional "info" (tooltip string)
  - file: optional "file_types": [".mid", ".midi"]
  - input components: optional "required": true  (renders .harp_required(True))

The generated module begins with:  import gradio as gr  /  from pyharp import *
so your code may freely use pyharp helpers WITHOUT importing them: ModelCard,
build_endpoint, LabelList, AudioLabel, MidiLabel, OutputLabel, load_audio,
load_midi, save_audio, save_midi.

inference.setup rules:
  - module-level python that runs once at import: imports for your framework and
    one-time model loading (assign the loaded model to a module-level variable).
  - put every third-party dependency in framework.pip (and OS packages in
    framework.apt). Use only real, importable packages.

inference.body rules:
  - this is the BODY of process_fn ONLY (no "def" line, no surrounding
    indentation). The function parameters are exactly the input components'
    "name" values, in order, and each audio/file input arrives as a string path.
  - it MUST end with a `return` of the outputs in the SAME ORDER as "outputs".
    Return a filepath string for audio/file outputs; return a LabelList (or its
    .to_json()) for a labels output.
  - set framework.gpu true only if the model needs a GPU; if true the renderer
    adds `import spaces` and an `@spaces.GPU` decorator. Do NOT set gpu true if
    you are reusing a Space function that already has its own @spaces.GPU
    decorator (avoid nested GPU allocation).

When a "# Original Space source" section is provided, it is the GROUND TRUTH:
  - the wrapper is deployed INTO that Space, so its modules are importable.
  - Find the SINGLE highest-level inference entry point: the one function the
    Space's UI button calls to go from raw inputs to the final output (often
    named like `synthesis_function` / `infer` / `predict` / `generate`). Import
    THAT and call it, passing every parameter through. Do NOT re-call its
    internal multi-stage helpers (e.g. a `run_preprocess` step plus a separate
    `run_svs`/inference step) and do NOT reimplement audio preprocessing
    (sample-rate resampling, mono conversion, trimming): doing so silently
    changes results. A common, hard-to-debug failure is correct melody/timbre but
    GIBBERISH WORDS, caused by feeding the lyric/ASR path audio or options that
    differ from what the entry function would have prepared.
  - Mirror the Space UI's REAL components AND their DEFAULT VALUES exactly
    (labels, types, choices, and especially defaults such as vocal-separation
    flags and language selections). Pass option strings through verbatim.
  - If that entry function already carries an @spaces.GPU decorator, set
    framework.gpu false to avoid nested GPU allocation.

Output ONLY the JSON object. No markdown, no commentary.
"""


BACKEND_SYSTEM_PROMPT = """\
You generate plain-Gradio "backend" recipes as STRICT JSON. A deterministic
renderer turns your recipe into a standalone Gradio app.py that RUNS the model
and exposes its inference function at the "/predict" API endpoint. This backend
is the model-running half of a two-Space deployment: a separate thin HARP
frontend will later proxy to it over the network. THIS APP DOES NOT USE PYHARP.

Return ONE JSON object with exactly these top-level keys:
  - "model":     {"id", "name", "description", "author", "tags": [...]}
  - "framework": {"import": "<top pip import name>", "pip": [...], "apt": [...], "gpu": bool}
  - "inputs":    ordered list of input components
  - "outputs":   ordered list of output components
  - "inference": {"setup": "<module-level python>", "body": "<predict() body>"}

Input component types: audio, file, dropdown, slider, textbox, number, checkbox.
Output component types: audio, file, labels.
Every component needs "name" (a valid python identifier) and "type"; most need
"label". Extra fields:
  - dropdown: "choices": [...], optional "default"
  - slider:   "min", "max", optional "step", "default"
  - textbox/number/checkbox: optional "default"
  - any component: optional "info" (tooltip string)
  - file: optional "file_types": [".mid", ".midi"]

CRITICAL -- no pyharp:
  - The generated module begins with ONLY `import gradio as gr`. pyharp is NOT
    installed. Do NOT use ModelCard, build_endpoint, LabelList, AudioLabel,
    MidiLabel, OutputLabel, load_audio, load_midi, save_audio, save_midi, or any
    other pyharp helper. Use plain libraries instead (soundfile / librosa /
    numpy for audio I/O), and declare them in framework.pip.

inference.setup rules:
  - module-level python that runs once at import: imports for your framework and
    one-time model loading (assign the loaded model to a module-level variable).
  - put every third-party dependency in framework.pip (and OS packages in
    framework.apt). Use only real, importable packages. Do NOT list pyharp.

inference.body rules:
  - this is the BODY of predict() ONLY (no "def" line, no surrounding
    indentation). The function parameters are exactly the input components'
    "name" values, in order; each audio/file input arrives as a string path.
  - it MUST end with a `return` of the outputs in the SAME ORDER as "outputs".
    For an audio/file output, write the result to a file (e.g. with soundfile)
    and return its path STRING. For a labels output, return a JSON-serializable
    dict/list.
  - set framework.gpu true only if the model needs a GPU; if true the renderer
    adds `import spaces` and an `@spaces.GPU` decorator on predict().

When a "# Upstream GitHub source" section is provided it is the GROUND TRUTH for
the real loading/inference API: import and call the repo's real functions rather
than guessing from the README, and add the repo (plus its deps) to framework.pip.

Output ONLY the JSON object. No markdown, no commentary.
"""


# A permissive structured-output schema (providers that ignore it still work).
RECIPE_RESPONSE_SCHEMA: JSON = {
    "type": "object",
    "properties": {
        "model": {"type": "object"},
        "framework": {"type": "object"},
        "inputs": {"type": "array", "items": {"type": "object"}},
        "outputs": {"type": "array", "items": {"type": "object"}},
        "inference": {
            "type": "object",
            "properties": {"setup": {"type": "string"}, "body": {"type": "string"}},
        },
    },
    "required": ["inputs", "outputs", "inference"],
}


@dataclass
class RecipeGenerationContext:
    """All grounding the LLM gets about the target model."""

    model_id: str
    readme: str = ""
    pipeline_tag: str = ""
    library_name: str = ""
    license: str = ""
    tags: List[str] = field(default_factory=list)
    author: str = ""
    name: str = ""
    files: List[str] = field(default_factory=list)
    target_inputs: List[str] = field(default_factory=list)
    target_outputs: List[str] = field(default_factory=list)
    examples: List[JSON] = field(default_factory=list)
    # {filename: source} for the grounding source's entry file and its
    # first-party modules -- the ground truth for the real loading/inference
    # API (and UI, for Spaces).
    space_sources: Dict[str, str] = field(default_factory=dict)
    # Where space_sources came from: "space" (deploy INTO the Space, modules
    # already importable) or "github" (write a NEW wrapper and add the repo as a
    # git+ pip dependency). Controls how the grounding block is framed.
    grounding_origin: str = "space"
    # For GitHub grounding: the pip requirement the wrapper must declare so the
    # repo's modules are importable (e.g. "git+https://github.com/o/r.git@main").
    source_repo_url: str = ""

    @classmethod
    def from_card(
        cls,
        card: Mapping[str, Any],
        *,
        target_inputs: Optional[List[str]] = None,
        target_outputs: Optional[List[str]] = None,
        examples: Optional[List[JSON]] = None,
    ) -> "RecipeGenerationContext":
        meta = card.get("meta") if isinstance(card.get("meta"), Mapping) else {}
        model_id = str(meta.get("id") or card.get("id") or "")
        author = str(meta.get("author") or (model_id.split("/")[0] if "/" in model_id else ""))
        name = str(meta.get("name") or (model_id.split("/")[-1] if model_id else ""))
        return cls(
            model_id=model_id,
            readme=str(card.get("readme") or ""),
            pipeline_tag=str(meta.get("pipeline_tag") or ""),
            library_name=str(meta.get("library_name") or ""),
            license=str(meta.get("license") or ""),
            tags=[str(tag) for tag in (meta.get("tags") or [])],
            author=author,
            name=name,
            files=[str(item) for item in (card.get("files") or [])],
            target_inputs=list(target_inputs or []),
            target_outputs=list(target_outputs or []),
            examples=list(examples or []),
        )


def default_examples(limit: int = 2) -> List[JSON]:
    """Load committed example recipes for few-shot grounding."""

    examples_dir = Path(__file__).resolve().parent / "examples"
    examples: List[JSON] = []
    for recipe_file in sorted(examples_dir.glob("recipe_*.json")):
        try:
            examples.append(json.loads(recipe_file.read_text(encoding="utf-8")))
        except (OSError, json.JSONDecodeError):
            continue
        if len(examples) >= limit:
            break
    return examples


def build_recipe_user_prompt(context: RecipeGenerationContext, *, backend: bool = False) -> str:
    lines: List[str] = ["# Target model", f"id: {context.model_id or '(unknown)'}"]
    if context.name:
        lines.append(f"name: {context.name}")
    if context.author:
        lines.append(f"author: {context.author}")
    if context.pipeline_tag:
        lines.append(f"pipeline_tag: {context.pipeline_tag}")
    if context.library_name:
        lines.append(f"library_name: {context.library_name}")
    if context.license:
        lines.append(f"license: {context.license}")
    if context.tags:
        lines.append(f"tags: {', '.join(context.tags)}")
    if context.files:
        listed = "\n".join(f"  - {name}" for name in context.files[:60])
        lines.append("repo_files:\n" + listed)
    if context.target_inputs:
        lines.append(f"desired_input_types: {', '.join(context.target_inputs)}")
    if context.target_outputs:
        lines.append(f"desired_output_types: {', '.join(context.target_outputs)}")

    readme = context.readme.strip()
    if readme:
        if len(readme) > _README_LIMIT:
            readme = readme[:_README_LIMIT] + "\n...[truncated]"
        lines.append("# Model card (README.md)\n" + readme)

    if context.space_sources:
        if context.grounding_origin == "github":
            pip_hint = context.source_repo_url or "git+https://github.com/<owner>/<repo>.git"
            lines.append(
                "# Upstream GitHub source (GROUND TRUTH for the real inference API)\n"
                "These files come from the model's GitHub repository. Reuse this code: "
                "import and call the repo's REAL functions/classes for model loading and "
                "inference instead of guessing from the README. This repo is NOT a Gradio "
                "app you deploy into -- you are writing a NEW HARP/Gradio wrapper around its "
                "Python API. Therefore you MUST:\n"
                f"  - add the repo as a pip dependency in framework.pip: \"{pip_hint}\" "
                "(plus its other pip/apt deps), so its modules are importable;\n"
                "  - import the repo's modules in inference.setup and load the model once "
                "there;\n"
                "  - design HARP input/output components for the desired I/O types (the repo "
                "has no UI to mirror)."
            )
        else:
            lines.append(
                "# Original Space source (GROUND TRUTH for the real API and UI)\n"
                "Reuse this code: import and call the Space's own modules/functions, "
                "and mirror its REAL input/output components. Do NOT invent a different "
                "interface from the README. The wrapper is deployed into this Space, so "
                "its modules are importable."
            )
        for filename, source in context.space_sources.items():
            snippet = source.strip()
            if len(snippet) > _SPACE_SOURCE_LIMIT:
                snippet = snippet[:_SPACE_SOURCE_LIMIT] + "\n...[truncated]"
            lines.append(f"## {filename}\n```python\n{snippet}\n```")

    if context.examples:
        lines.append(
            "# Example recipes (for SHAPE reference only; do not copy their model logic)"
        )
        for example in context.examples:
            lines.append("```json\n" + json.dumps(example, indent=2) + "\n```")

    if backend:
        lines.append(
            "# Task\n"
            "Return ONE JSON recipe for a PLAIN-GRADIO BACKEND (no pyharp) that runs "
            "THIS model with real, importable setup/body code. Do not use any pyharp "
            "helper; use plain libraries (soundfile/librosa/numpy) for audio I/O and "
            "return filepath strings for audio/file outputs. Return only JSON."
        )
    else:
        lines.append(
            "# Task\n"
            "Return ONE JSON recipe for the target model with real, importable "
            "setup/body code specific to THIS model. Return only JSON."
        )
    return "\n\n".join(lines)


# --------------------------------------------------------------------------- #
# Generation loop
# --------------------------------------------------------------------------- #


@dataclass
class RecipeDraft:
    recipe: JSON
    app_py: str
    attempts: int
    raw_responses: List[Any] = field(default_factory=list)
    provider: str = ""
    model: str = ""


def _normalize_recipe(data: Any, context: RecipeGenerationContext) -> JSON:
    if not isinstance(data, Mapping):
        raise LLMError("LLM response was not a JSON object")

    recipe: JSON = json.loads(json.dumps(data))  # plain, mutable deep copy
    model = recipe.get("model")
    if not isinstance(model, dict):
        model = {}
    recipe["model"] = model

    if not str(model.get("id") or "").strip():
        model["id"] = context.model_id or "TODO-author/TODO-model"
    if not str(model.get("name") or "").strip():
        model["name"] = context.name or str(model["id"]).split("/")[-1]
    if not str(model.get("author") or "").strip() and context.author:
        model["author"] = context.author
    if not model.get("tags") and context.tags:
        model["tags"] = list(context.tags)
    if not str(model.get("license") or "").strip() and context.license:
        model["license"] = context.license
    return recipe


def _repair_prompt(base_user: str, recipe: JSON, error: str) -> str:
    return (
        base_user
        + "\n\n# Previous attempt failed validation\nPrevious JSON:\n```json\n"
        + json.dumps(recipe, indent=2)
        + "\n```\nError:\n"
        + error
        + "\nReturn a corrected, COMPLETE JSON recipe that fixes this."
    )


def generate_recipe(
    context: RecipeGenerationContext,
    provider: LLMProvider,
    *,
    max_repairs: int = 2,
    backend: bool = False,
) -> RecipeDraft:
    """Ask ``provider`` for a recipe and validate/render/repair until it is runnable.

    When ``backend`` is true, the LLM is prompted to author a plain-Gradio backend
    (no pyharp) and the resulting recipe is marked ``framework.backend = true`` so
    the renderer emits a standalone ``/predict`` Space for the two-Space workflow.
    """

    system_prompt = BACKEND_SYSTEM_PROMPT if backend else RECIPE_SYSTEM_PROMPT
    base_user = build_recipe_user_prompt(context, backend=backend)
    raw_responses: List[Any] = []
    feedback: Optional[str] = None
    last_recipe: JSON = {}
    attempts = 0

    for attempt in range(max_repairs + 1):
        attempts = attempt + 1
        user = base_user if feedback is None else _repair_prompt(base_user, last_recipe, feedback)
        data = provider.complete_json(system_prompt, user, schema=RECIPE_RESPONSE_SCHEMA)
        raw_responses.append(data)

        recipe = _normalize_recipe(data, context)
        if backend:
            framework = recipe.get("framework")
            if not isinstance(framework, dict):
                framework = {}
                recipe["framework"] = framework
            framework["backend"] = True
        last_recipe = recipe
        try:
            validate_recipe(recipe)
            app_py = render_app_from_recipe(recipe)
            compile(app_py, f"<llm:{context.model_id or 'recipe'}>", "exec")
        except (ValueError, SyntaxError) as exc:
            feedback = f"{type(exc).__name__}: {exc}"
            continue

        return RecipeDraft(
            recipe=recipe,
            app_py=app_py,
            attempts=attempts,
            raw_responses=raw_responses,
            provider=getattr(provider, "name", ""),
            model=getattr(provider, "model", ""),
        )

    raise LLMError(
        f"LLM could not produce a valid recipe for "
        f"{context.model_id or '(unknown)'} after {attempts} attempt(s). "
        f"Last error: {feedback}"
    )


# --------------------------------------------------------------------------- #
# Completing a scaffolded recipe (fill the _todo stubs)
# --------------------------------------------------------------------------- #

_TODO_INPUT_RE = re.compile(r"inputs\.([A-Za-z_][A-Za-z0-9_]*)")


def _context_from_recipe(recipe: Mapping[str, Any]) -> RecipeGenerationContext:
    model = recipe.get("model") if isinstance(recipe.get("model"), Mapping) else {}
    model_id = str(model.get("id") or "")
    return RecipeGenerationContext(
        model_id=model_id,
        name=str(model.get("name") or ""),
        author=str(model.get("author") or (model_id.split("/")[0] if "/" in model_id else "")),
        tags=[str(tag) for tag in (model.get("tags") or [])],
        license=str(model.get("license") or ""),
    )


def _todo_component_names(recipe: Mapping[str, Any]) -> set:
    """Names of input components the scaffold flagged as needing manual values."""

    names: set = set()
    for item in recipe.get("_todo") or []:
        match = _TODO_INPUT_RE.match(str(item))
        if match:
            names.add(match.group(1))
    return names


def _strip_meta(recipe: Mapping[str, Any]) -> JSON:
    return {key: value for key, value in recipe.items() if not str(key).startswith("_")}


def _merge_components(base_list: List[JSON], llm_list: Any, fillable: set) -> List[JSON]:
    """Preserve the scaffold's components, splicing in LLM values only where stubbed."""

    by_name: Dict[str, JSON] = {}
    if isinstance(llm_list, list):
        by_name = {str(item.get("name")): item for item in llm_list if isinstance(item, Mapping)}

    merged: List[JSON] = []
    for component in base_list:
        if not isinstance(component, Mapping):
            continue
        out = dict(component)
        other = by_name.get(str(component.get("name")), {})
        if out.get("name") in fillable and isinstance(other, Mapping):
            if out.get("type") == "dropdown" and isinstance(other.get("choices"), list) and other["choices"]:
                out["choices"] = list(other["choices"])
                if other.get("default") is not None:
                    out["default"] = other["default"]
            if out.get("type") == "slider":
                for key in ("min", "max", "step", "default"):
                    if other.get(key) is not None:
                        out[key] = other[key]
        # Always allow filling a missing tooltip.
        if not out.get("info") and isinstance(other, Mapping) and other.get("info"):
            out["info"] = str(other["info"])
        merged.append(out)
    return merged


def _assemble_completed(
    base: Mapping[str, Any],
    llm: Mapping[str, Any],
    *,
    fillable: set,
    context: RecipeGenerationContext,
) -> JSON:
    base_model = dict(base.get("model") or {})
    llm_model = llm.get("model") if isinstance(llm.get("model"), Mapping) else {}

    if not str(base_model.get("id") or "").strip():
        base_model["id"] = context.model_id or "TODO-author/TODO-model"
    if not str(base_model.get("name") or "").strip():
        base_model["name"] = context.name or str(base_model["id"]).split("/")[-1]
    description = str(base_model.get("description") or "")
    if (not description or description.upper().startswith("TODO")) and llm_model.get("description"):
        base_model["description"] = str(llm_model["description"])
    if not base_model.get("tags") and llm_model.get("tags"):
        base_model["tags"] = list(llm_model["tags"])

    base_inputs = base.get("inputs") if isinstance(base.get("inputs"), list) else []
    base_outputs = base.get("outputs") if isinstance(base.get("outputs"), list) else []

    framework = llm.get("framework") if isinstance(llm.get("framework"), Mapping) else {}
    inference = llm.get("inference") if isinstance(llm.get("inference"), Mapping) else {}

    return {
        "model": base_model,
        "framework": dict(framework),
        "inputs": _merge_components(base_inputs, llm.get("inputs"), fillable),
        "outputs": _merge_components(base_outputs, llm.get("outputs"), set()),
        "inference": dict(inference),
    }


def build_completion_user_prompt(base: Mapping[str, Any], context: RecipeGenerationContext) -> str:
    lines = [
        "# Scaffolded recipe to COMPLETE",
        "```json\n" + json.dumps(_strip_meta(base), indent=2) + "\n```",
    ]
    todos = base.get("_todo") or []
    if todos:
        lines.append("# Items still to fill\n" + "\n".join(f"- {item}" for item in todos))

    if context.readme.strip():
        readme = context.readme.strip()
        if len(readme) > _README_LIMIT:
            readme = readme[:_README_LIMIT] + "\n...[truncated]"
        lines.append("# Model card (README.md)\n" + readme)
    if context.files:
        listed = "\n".join(f"  - {name}" for name in context.files[:60])
        lines.append("repo_files:\n" + listed)

    lines.append(
        "# Task\n"
        "Return ONE COMPLETE JSON recipe. PRESERVE the inputs and outputs "
        "(names, types, labels, order) EXACTLY as given. Fill 'framework' with "
        "real importable pip dependencies and 'inference.setup'/'inference.body' "
        "with working code for THIS specific model. Replace any TODO_ placeholder "
        "dropdown choices with real values and set real slider min/max/step. Do "
        "not include _todo or _source. Return only JSON."
    )
    return "\n\n".join(lines)


def complete_recipe(
    base_recipe: Mapping[str, Any],
    provider: LLMProvider,
    *,
    context: Optional[RecipeGenerationContext] = None,
    max_repairs: int = 2,
) -> RecipeDraft:
    """Fill the ``_todo`` stubs of a scaffolded recipe with an LLM.

    The resolved input/output contract from the scaffold is preserved; the LLM
    only supplies the framework dependencies, the inference glue, and any values
    the scaffold flagged as TODO (dropdown choices, slider ranges).
    """

    if not isinstance(base_recipe, Mapping):
        raise LLMError("base recipe must be a JSON object")

    base: JSON = json.loads(json.dumps(base_recipe))
    if context is None:
        context = _context_from_recipe(base)
    fillable = _todo_component_names(base)

    base_user = build_completion_user_prompt(base, context)
    raw_responses: List[Any] = []
    feedback: Optional[str] = None
    last_final: JSON = {}
    attempts = 0

    for attempt in range(max_repairs + 1):
        attempts = attempt + 1
        user = base_user if feedback is None else _repair_prompt(base_user, last_final, feedback)
        data = provider.complete_json(RECIPE_SYSTEM_PROMPT, user, schema=RECIPE_RESPONSE_SCHEMA)
        raw_responses.append(data)
        if not isinstance(data, Mapping):
            feedback = "response was not a JSON object"
            continue

        final = _assemble_completed(
            base, json.loads(json.dumps(data)), fillable=fillable, context=context
        )
        last_final = final
        try:
            validate_recipe(final)
            app_py = render_app_from_recipe(final)
            compile(app_py, f"<llm:{context.model_id or 'recipe'}>", "exec")
        except (ValueError, SyntaxError) as exc:
            feedback = f"{type(exc).__name__}: {exc}"
            continue

        return RecipeDraft(
            recipe=final,
            app_py=app_py,
            attempts=attempts,
            raw_responses=raw_responses,
            provider=getattr(provider, "name", ""),
            model=getattr(provider, "model", ""),
        )

    raise LLMError(
        f"LLM could not complete the recipe for "
        f"{context.model_id or '(unknown)'} after {attempts} attempt(s). "
        f"Last error: {feedback}"
    )


# --------------------------------------------------------------------------- #
# Refining a remote-backend (proxy) recipe with an LLM
# --------------------------------------------------------------------------- #

REMOTE_REFINE_SYSTEM_PROMPT = """\
You refine HARP "remote-backend" recipes as STRICT JSON. This recipe does NOT run
the model: it deploys a thin pyharp/Gradio frontend whose process_fn proxies to an
existing Gradio Space via gradio_client. You never write inference code or install
the model's dependencies.

You are given a deterministic scaffold (built from the backend Space's live API)
plus the backend endpoint's exact POSITIONAL signature and the model's README.
Improve the scaffold using that grounding.

Return ONE JSON object with exactly these top-level keys:
  - "model":     {"id","name","description","author","tags":[...]}
  - "framework": {"gpu": false, "pip": [], "remote": {...}}
  - "inputs":    ordered input components
  - "outputs":   ordered output components
Do NOT include an "inference" key.

framework.remote rules (CRITICAL):
  - KEEP "space" and "api_name" byte-for-byte from the scaffold.
  - "args" is the backend's POSITIONAL call signature. It MUST contain exactly one
    entry per backend parameter, in the SAME order as the numbered parameter list
    (same length). For each entry choose ONE of:
      * {"from": "<input name>"}  -> expose it as a HARP control the user sets.
        Add "file": true for audio/file parameters; add "cast": "int"|"float"|
        "str"|"bool" if the backend needs a specific type.
      * {"const": <value>}        -> send a FIXED value the user should not set
        (use for hidden session/state args, metadata the UI shouldn't expose, or
        toggles that must stay at a specific value). null is a valid const.
  - Decide expose-vs-const from the README + parameter labels (e.g. a hidden
    "state"/"metadata"/"session" arg should be a const).
  - When a "# Backend Space UI source" section is provided, it is the GROUND TRUTH
    and OVERRIDES guesses: read the Space's Gradio code to (1) fill each dropdown's
    REAL choices and default; (2) decide expose-vs-const -- any positional arg the
    UI computes internally or leaves OPTIONAL (e.g. a metadata/state/session slot
    that defaults to None and is produced by another step) MUST be {"const": null}
    (or its real fixed default) and MUST NOT be exposed as an input; (3) mirror the
    real component labels and default values.
  - Every input component you declare MUST be referenced by exactly one {"from"}.
  - "returns" maps backend return positions to your outputs: [{"index": i, "to":
    "<output name>"}]; keep indices valid and cover every output.

Component rules: input types audio, file, dropdown, slider, textbox, number,
checkbox; output types audio, file, labels. Fill real dropdown "choices", slider
"min"/"max"/"step", sensible defaults, clear "label"/"info", and a real model
description + tags from the README.

Output ONLY the JSON object. No markdown, no commentary.
"""

# Structured-output schema for the refine step: like the recipe schema but with
# NO required "inference" (remote recipes have none).
REMOTE_RESPONSE_SCHEMA: JSON = {
    "type": "object",
    "properties": {
        "model": {"type": "object"},
        "framework": {"type": "object"},
        "inputs": {"type": "array", "items": {"type": "object"}},
        "outputs": {"type": "array", "items": {"type": "object"}},
    },
    "required": ["framework", "inputs", "outputs"],
}


def _remote_block(recipe: Mapping[str, Any]) -> Mapping[str, Any]:
    framework = recipe.get("framework") if isinstance(recipe.get("framework"), Mapping) else {}
    remote = framework.get("remote") if isinstance(framework.get("remote"), Mapping) else {}
    return remote


def build_remote_refine_prompt(
    scaffold: Mapping[str, Any],
    endpoint: Mapping[str, Any],
    context: RecipeGenerationContext,
) -> str:
    remote = _remote_block(scaffold)
    lines = [
        "# Backend endpoint (GROUND TRUTH positional signature)",
        f"space: {remote.get('space')}",
        f"api_name: {remote.get('api_name')}",
    ]

    parameters = endpoint.get("parameters") or []
    param_lines = ["parameters (args[i] maps to parameter i, in THIS exact order):"]
    for index, param in enumerate(parameters):
        param = param if isinstance(param, Mapping) else {}
        component = param.get("component") or "?"
        label = param.get("label")
        pname = param.get("parameter_name")
        default = param.get("parameter_default")
        param_lines.append(
            f"  [{index}] component={component} label={label!r} "
            f"parameter_name={pname!r} default={default!r}"
        )
    lines.append("\n".join(param_lines))

    returns = endpoint.get("returns") or []
    ret_lines = ["returns:"]
    for index, ret in enumerate(returns):
        ret = ret if isinstance(ret, Mapping) else {}
        ret_lines.append(f"  [{index}] component={ret.get('component') or '?'} label={ret.get('label')!r}")
    lines.append("\n".join(ret_lines))

    lines.append(
        "# Scaffold to REFINE (keep space/api_name; keep args length & order)\n"
        "```json\n" + json.dumps(_strip_meta(scaffold), indent=2) + "\n```"
    )

    readme = context.readme.strip()
    if readme:
        if len(readme) > _README_LIMIT:
            readme = readme[:_README_LIMIT] + "\n...[truncated]"
        lines.append("# Model card (README.md)\n" + readme)

    if context.space_sources:
        lines.append(
            "# Backend Space UI source (GROUND TRUTH for choices & expose-vs-const)\n"
            "This is the backend Space's own Gradio UI code. Use it to fill REAL "
            "dropdown choices/defaults, mirror real labels/defaults, and decide which "
            "positional args to expose vs. send as constants. Args the UI computes "
            "internally or leaves OPTIONAL (metadata/state/session slots that default "
            "to None) must be {\"const\": null} and must NOT be exposed as inputs. Keep "
            "framework.remote.args length and order unchanged."
        )
        for filename, source in context.space_sources.items():
            snippet = source.strip()
            if len(snippet) > _SPACE_SOURCE_LIMIT:
                snippet = snippet[:_SPACE_SOURCE_LIMIT] + "\n...[truncated]"
            lines.append(f"## {filename}\n```python\n{snippet}\n```")

    lines.append(
        "# Task\n"
        "Return ONE refined remote-backend JSON recipe (keys: model, framework, "
        "inputs, outputs; NO inference). Keep framework.remote.space and api_name "
        f"exactly, and keep framework.remote.args at exactly {len(parameters)} "
        "entries in the same order as the parameters above. Return only JSON."
    )
    return "\n\n".join(lines)


def _remote_invariant_error(refined: Mapping[str, Any], scaffold: Mapping[str, Any]) -> Optional[str]:
    """Guard the positional integrity the LLM must not break."""

    refined_remote = _remote_block(refined)
    scaffold_remote = _remote_block(scaffold)
    if not refined_remote:
        return "framework.remote is missing; this must stay a remote-backend recipe."
    scaffold_args = scaffold_remote.get("args") or []
    refined_args = refined_remote.get("args")
    if not isinstance(refined_args, list) or len(refined_args) != len(scaffold_args):
        return (
            f"framework.remote.args must have exactly {len(scaffold_args)} entries, "
            "one per backend parameter, in the same order."
        )
    return None


def refine_remote_recipe(
    scaffold: Mapping[str, Any],
    endpoint: Mapping[str, Any],
    provider: LLMProvider,
    *,
    context: Optional[RecipeGenerationContext] = None,
    max_repairs: int = 2,
) -> RecipeDraft:
    """LLM-refine a deterministic remote scaffold, grounded on the live API schema.

    The scaffold guarantees the positional ``args`` count/order (which the README
    alone can't convey); the LLM refines the judgement calls -- which args to
    expose vs. send as constants, dropdown choices, slider ranges, labels, and the
    model card. ``space``/``api_name`` are pinned and the ``args`` length is
    enforced, so the call signature stays aligned with the backend.
    """

    if not isinstance(scaffold, Mapping):
        raise LLMError("scaffold must be a JSON object")

    scaffold_copy: JSON = json.loads(json.dumps(scaffold))
    if context is None:
        context = _context_from_recipe(scaffold_copy)

    scaffold_remote = _remote_block(scaffold_copy)
    base_user = build_remote_refine_prompt(scaffold_copy, endpoint, context)
    raw_responses: List[Any] = []
    feedback: Optional[str] = None
    last: JSON = {}
    attempts = 0

    for attempt in range(max_repairs + 1):
        attempts = attempt + 1
        user = base_user if feedback is None else _repair_prompt(base_user, last, feedback)
        data = provider.complete_json(REMOTE_REFINE_SYSTEM_PROMPT, user, schema=REMOTE_RESPONSE_SCHEMA)
        raw_responses.append(data)

        recipe = _normalize_recipe(data, context)
        # Pin the fields the LLM must not change, so a stray edit can't misroute
        # the call (belt-and-suspenders alongside the invariant check).
        remote = recipe.get("framework")
        remote = remote.get("remote") if isinstance(remote, Mapping) else None
        if isinstance(remote, dict):
            remote["space"] = scaffold_remote.get("space")
            remote["api_name"] = scaffold_remote.get("api_name")
            remote.setdefault("token_env", scaffold_remote.get("token_env", "HF_TOKEN"))
            # user_token is a deployment decision the LLM shouldn't toggle; keep it.
            if scaffold_remote.get("user_token"):
                remote["user_token"] = True
        last = recipe

        invariant = _remote_invariant_error(recipe, scaffold_copy)
        if invariant is not None:
            feedback = invariant
            continue
        try:
            validate_recipe(recipe)
            app_py = render_app_from_recipe(recipe)
            compile(app_py, f"<llm-remote:{context.model_id or 'recipe'}>", "exec")
        except (ValueError, SyntaxError) as exc:
            feedback = f"{type(exc).__name__}: {exc}"
            continue

        return RecipeDraft(
            recipe=recipe,
            app_py=app_py,
            attempts=attempts,
            raw_responses=raw_responses,
            provider=getattr(provider, "name", ""),
            model=getattr(provider, "model", ""),
        )

    raise LLMError(
        f"LLM could not refine the remote recipe for "
        f"{context.model_id or '(unknown)'} after {attempts} attempt(s). "
        f"Last error: {feedback}"
    )
