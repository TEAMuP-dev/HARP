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


class LLMError(Exception):
    """Raised when an LLM provider is misconfigured or fails to produce a recipe."""


# --------------------------------------------------------------------------- #
# Providers (urllib only; no third-party SDKs)
# --------------------------------------------------------------------------- #

_DEFAULT_MODELS = {
    "gemini": "gemini-2.5-flash",
    "anthropic": "claude-3-5-sonnet-latest",
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
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", "replace")
        raise LLMError(f"{url} returned HTTP {exc.code}: {detail[:500]}") from exc
    except urllib.error.URLError as exc:
        raise LLMError(f"request to {url} failed: {exc}") from exc


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


class GeminiProvider(LLMProvider):
    name = "gemini"

    def complete_json(self, system: str, user: str, *, schema: Optional[JSON] = None) -> JSON:
        url = (
            "https://generativelanguage.googleapis.com/v1beta/models/"
            f"{self.model}:generateContent?key={self.api_key}"
        )
        generation_config: JSON = {
            "responseMimeType": "application/json",
            "temperature": self.temperature,
        }
        if schema:
            generation_config["responseSchema"] = schema
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
    if not name:
        raise LLMError(
            "No LLM provider configured. Set --provider and the matching API key "
            "env var (GEMINI_API_KEY / ANTHROPIC_API_KEY / OPENAI_API_KEY)."
        )
    if name not in _PROVIDERS:
        raise LLMError(f"Unknown LLM provider '{name}' (expected one of {sorted(_PROVIDERS)}).")

    provider_cls, keys = _PROVIDERS[name]
    api_key = next((os.environ[key] for key in keys if os.environ.get(key)), "")
    if not api_key:
        raise LLMError(f"{name} selected but none of {keys} is set in the environment.")
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
    adds `import spaces` and an `@spaces.GPU` decorator.

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


def build_recipe_user_prompt(context: RecipeGenerationContext) -> str:
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

    if context.examples:
        lines.append(
            "# Example recipes (for SHAPE reference only; do not copy their model logic)"
        )
        for example in context.examples:
            lines.append("```json\n" + json.dumps(example, indent=2) + "\n```")

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
) -> RecipeDraft:
    """Ask ``provider`` for a recipe and validate/render/repair until it is runnable."""

    base_user = build_recipe_user_prompt(context)
    raw_responses: List[Any] = []
    feedback: Optional[str] = None
    last_recipe: JSON = {}
    attempts = 0

    for attempt in range(max_repairs + 1):
        attempts = attempt + 1
        user = base_user if feedback is None else _repair_prompt(base_user, last_recipe, feedback)
        data = provider.complete_json(RECIPE_SYSTEM_PROMPT, user, schema=RECIPE_RESPONSE_SCHEMA)
        raw_responses.append(data)

        recipe = _normalize_recipe(data, context)
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
