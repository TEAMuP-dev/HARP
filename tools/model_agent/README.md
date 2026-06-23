# HARP Model Agent

> New here? Read [`OVERVIEW.md`](./OVERVIEW.md) first — a plain-language tour of
> how model deployment works in HARP, with a pipeline diagram and a glossary.
> This README is the command reference.

`model_agent` discovers Hugging Face Spaces, probes HARP-compatible Gradio
endpoints, scores raw Hugging Face models, and packages model metadata into
reviewable artifacts.

The agent is intentionally conservative:

- it only uses the Python standard library;
- it treats Hugging Face search results as candidates until a Space is probed;
- it packages metadata and endpoint contracts, not model weights;
- it can score raw Hugging Face model cards before spending time on wrappers;
- it can render a starter `app.py` for `audio-to-audio` models whose framework
  has a runnable template (currently SpeechBrain), and refuses to emit a wrapper
  it cannot actually run;
- it can optionally smoke-test a generated wrapper by launching it and verifying
  the endpoint exposes HARP controls;
- it resolves a model's canonical `author/name` from the live Gradio config so
  documentation links keep the correct `_` vs `-` spelling;
- it writes deterministic package folders that can be reviewed before models are
  added to HARP's featured list.

## File Map

- `agent.py` contains the core behavior: Hugging Face discovery, HARP endpoint
  probing, task classification, license checks, compatibility scoring, template
  generation, package data structures, and package writing.
- `analyze.py` statically parses harvested `app.py` wrappers and reports the
  input/output component shapes across the corpus (no code execution). Per
  component it resolves the type, label, the `harp_required(...)` boolean,
  tooltips (`info=` / `.set_info(...)`), dropdown `choices`, `file_types`, and
  slider ranges/defaults when they are literals.
- `recipe.py` renders a runnable pyharp `app.py` (and the rest of a Space
  package) from a declarative recipe JSON describing inputs, outputs, framework,
  and inference glue.
- `llm.py` is an *optional* layer that lets an LLM (Gemini/Claude/GPT) draft a
  recipe for arbitrary frameworks, then runs it through the deterministic
  validate -> render -> repair loop. Providers use `urllib` only; the module is
  never imported during normal offline use of the core.
- `cli.py` exposes the core behavior as terminal commands.
- `__main__.py` lets you run the package with `python3 -m tools.model_agent`.
- `tests/test_agent.py` protects the behavior that is easiest to break while
  refactoring.

The tests use fake model names such as `example/audio-model`. They do not define
which models HARP supports; they only check that the agent can transform URLs,
parse Gradio responses, score candidates, render starter wrappers, and write
package files.

## Workflow

The commands fall into three groups by what they touch:

- **Offline (safe anywhere):** `score-card`, `render-app`, `generate-package`.
  They read a local model-card JSON and write files; they never use the network
  or run model code.
- **Network (reads metadata only):** `discover`, `probe`, `package`,
  `package-repo`. These call the Hugging Face / Gradio APIs but do not execute
  any model code.
- **Sandbox recommended:** `smoke-test` (and the `--smoke-test` flag). This
  launches a generated `app.py`, which downloads and runs third-party model
  code, so only run it after review or inside a venv/container.

**Where do model-card JSON files come from?** No command writes one for you. The
file-based commands (`score-card` / `render-app` / `generate-package`) expect a
card you supply, shaped like this:

```json
{
  "meta": {
    "id": "author/model-name",
    "author": "author",
    "pipeline_tag": "audio-to-audio",
    "library_name": "speechbrain",
    "license": "apache-2.0",
    "tags": ["audio-to-audio", "speechbrain"]
  },
  "files": ["hyperparams.yaml", "model.ckpt"],
  "readme": "..."
}
```

A ready-to-use example lives at `tools/model_agent/examples/example_card.json` (a
SpeechBrain `audio-to-audio` card). To work from a real model instead, use
`package-repo <hf-repo-id>`, which fetches the card from Hugging Face and
packages it in one step — no hand-authored card needed.

## Running the tests

From the repository root (the directory that contains `tools/`):

```bash
python3 -m unittest discover -s tools/model_agent/tests -v
```

The suite is offline and uses only the standard library, so no network access or
extra dependencies are required.

## Examples

Discover Gradio Spaces. This example searches the HARP team account, but the
agent can search other Hugging Face authors too:

```bash
python3 -m tools.model_agent discover --query audio --author teamup-tech --limit 20
```

Probe a known HARP endpoint:

```bash
python3 -m tools.model_agent probe teamup-tech/demucs-source-separation
```

Harvest the `app.py` wrappers from an author's Spaces for offline study (the
`teamup-tech` org is a corpus of real, working HARP wrappers):

```bash
python3 -m tools.model_agent harvest --author teamup-tech --output artifacts/model_agent/harvest
```

This writes `artifacts/model_agent/harvest/<slug>/app.py` for each Space plus an
`index.json` summary (`ok` / `missing` / `error` per Space). It only downloads
files; it never runs them.

Analyze a harvested folder (or a single `app.py`) to report the input/output
shapes real HARP wrappers use. This statically parses the files with `ast` and
never executes them:

```bash
python3 -m tools.model_agent analyze artifacts/model_agent/harvest
```

The report lists a per-app record (resolved `inputs` / `outputs` component types,
`@spaces.GPU` usage, pyharp import paths) plus an aggregate `summary`:
distribution of component types, how many inputs/outputs each wrapper uses, and
how many rely on GPU. Use `--summary-only` for just the aggregate view. This is
the data that validates the wrapper "recipe" schema against the real corpus.

Each record also carries a `recipe_eligible` flag. A wrapper is eligible only
when its input/output shapes are fully resolved statically; wrappers whose
components are passed as a variable (`dynamic`) or expose no resolvable
components are flagged with an `unresolved_reason` and excluded from the recipe
corpus automatically. The `summary` reports `recipe_eligible`, `unresolved`, and
the list of `unresolved_apps`.

Static analysis tells you what good wrappers *look like*; it does not tell you
whether a Space is *alive* right now (a working wrapper can go down after a
Hugging Face/runtime update). Add `--check-health` to also probe each harvested
Space's endpoint and fold a per-app `health` (`alive` / `dead` with control
counts and reason) plus a `summary.health` tally into the report. This needs
network access and the harvest folder's `index.json` (to recover Space ids):

```bash
python3 -m tools.model_agent analyze artifacts/model_agent/harvest --check-health
```

Probes run concurrently with a short per-Space timeout so a few sleeping/dead
Spaces don't serialize into a long wait, and progress is printed to stderr. Tune
with `--health-timeout` (default 20s) and `--health-workers` (default 8); e.g.
`--health-timeout 10 --health-workers 12` for a faster, more aggressive sweep.

Package endpoints into folders:

```bash
python3 -m tools.model_agent package \
  teamup-tech/demucs-source-separation \
  teamup-tech/midi-synthesizer \
  --output artifacts/model_agent/packages
```

Score a raw Hugging Face model-card JSON file:

```bash
python3 -m tools.model_agent score-card tools/model_agent/examples/example_card.json
```

Render a starter `app.py` for a raw `audio-to-audio` model card:

```bash
python3 -m tools.model_agent render-app tools/model_agent/examples/example_card.json \
  --output artifacts/model_agent/generated/example/app.py
```

Write a generated wrapper package:

```bash
python3 -m tools.model_agent generate-package tools/model_agent/examples/example_card.json
```

### Recipe-driven wrappers

Most real HARP wrappers are custom pip packages (Demucs, Matchering, Kokoro,
...), not a single `transformers` pipeline. A **recipe** is a declarative JSON
spec — model card, framework/dependencies, ordered input/output components, and
the model-specific inference glue — that the agent renders into a runnable
`app.py` plus `requirements.txt`, `packages.txt`, `README.md`, and a manifest.
The recipe shape is derived from the component shapes `analyze` reports across
the real corpus.

Input component types: `audio`, `file`, `dropdown`, `slider`, `textbox`,
`number`, `checkbox`. Output types: `audio`, `file`, `labels` (a `gr.JSON` /
`LabelList` track).

Each component may carry an optional `info` tooltip. It is rendered the way
HARP's own reference wrappers do: as a native `info=` kwarg on standard Gradio
components and as a chained pyharp `.set_info(...)` on media components
(`audio` / `file`). A `file` component may also set `file_types` (e.g.
`[".mid", ".midi"]`). Generated wrappers use `from pyharp import *`, matching the
reference template, so inference glue can call any pyharp helper (`load_audio`,
`save_audio`, `AudioLabel`, `MidiLabel`, ...) without managing imports.
`examples/recipe_ui_test.json` reproduces HARP's canonical 3.0.0 UI-test
wrapper end to end and is a good reference for the full schema.

Render an `app.py` from a recipe, or write the whole package:

```bash
python3 -m tools.model_agent render-recipe tools/model_agent/examples/recipe_stem_separation.json
python3 -m tools.model_agent generate-recipe tools/model_agent/examples/recipe_stem_separation.json
```

Example recipes are committed under `examples/`: `recipe_stem_separation.json`
(audio + dropdown -> 4 audio stems + labels), `recipe_audio_to_audio_labels.json`
(audio + slider -> audio + labels), and `recipe_ui_test.json` (the full HARP
3.0.0 UI-test wrapper exercising every component type, tooltips, `file_types`,
and audio + MIDI + label outputs). Like the other generators, `generate-recipe`
accepts `--smoke-test` to launch and verify the wrapper (runs downloaded code;
review/sandbox first).

#### Scaffolding a recipe from a harvested wrapper

To start a recipe from an existing, well-formed wrapper rather than from
scratch, `scaffold-recipe` reads a harvested `app.py`, fills in the model card
and the input/output components from the statically-resolved shapes — including
dropdown `choices`, slider `min`/`max`/`step`, defaults, tooltips, and
`file_types` whenever they are literals in the source — and leaves the parts
that can't be derived statically (dependencies and the inference glue, plus any
ranges/choices that weren't literals) as clearly-marked `_todo` placeholders:

```bash
python3 -m tools.model_agent scaffold-recipe \
  artifacts/model_agent/harvest/teamup-tech-demucs-source-separation/app.py \
  --output my_recipe.json
```

The Space id is recovered from the harvest `index.json` when present. Only
recipe-eligible wrappers can be scaffolded (the ones `analyze` resolves
cleanly); `dynamic`/unresolved wrappers are refused with the reason. The result
is itself a valid recipe that renders to a stub wrapper raising
`NotImplementedError`, so you can fill in the `_todo` items incrementally.

#### LLM-drafted recipes (for the long tail of frameworks)

Hardcoded templates only cover a few frameworks; `render-app` raises
`NotImplementedError` for everything else. For arbitrary models (PyTorch Hub,
Transformers, Diffusers, custom GitHub repos, ...), `generate-recipe-from-llm`
asks an LLM to draft a **recipe** — specifically the model-specific
`inference.setup` (imports + model loading) and `inference.body` (the
`process_fn` body) — and then runs the draft through the same deterministic
`validate -> render -> compile` pipeline, feeding any error back to the model
for a bounded number of repairs. The LLM proposes; the pipeline disposes:

```bash
export GEMINI_API_KEY=...        # or ANTHROPIC_API_KEY / OPENAI_API_KEY
python3 -m tools.model_agent generate-recipe-from-llm \
  --repo speechbrain/sepformer-wsj02mix --inputs audio --outputs audio,labels \
  --output artifacts/model_agent/recipes/demucs.json
```

Provide the model card with `--card <file.json>` (offline) or `--repo <hf-id>`
(fetches the card from Hugging Face). The provider is auto-detected from whichever
API key env var is set, or forced with `--provider gemini|anthropic|openai` and
`--llm-model <name>`. The drafted recipe is grounded with the real model card,
the repo file list, your desired I/O types, and a couple of committed example
recipes as few-shot exemplars (disable with `--no-examples`).

**Ground on an existing Space's source with `--space <author/space>`.** The model
card alone is often *not enough* for app-like Spaces (e.g. multi-stage pipelines
whose real inference lives in `webui.py`/`cli.py` modules, not in a one-call API).
A card-only draft tends to invent a plausible-but-wrong interface. With `--space`,
the agent downloads the Space's `app.py` and the first-party modules it imports
(stdlib/third-party imports are skipped, bounded crawl) and feeds them to the LLM
as ground truth, so the wrapper **reuses the real functions and mirrors the real
input/output components** instead of guessing:

```bash
python3 -m tools.model_agent generate-recipe-from-llm \
  --repo Soul-AILab/SoulX-Singer \
  --space Soul-AILab/SoulX-Singer \
  --output artifacts/model_agent/recipes/soulx-singer.json
```

Because the wrapper reuses the Space's own modules, deploy it **into a duplicate
of that Space** (HF → *Duplicate this Space*), where those modules and the
pretrained weights are present. A hand-authored reference for exactly this case
ships at `examples/soulx_singer_recipe.json`.

LLM model names change often and vary by key/region, so the built-in defaults can
go stale. If a call fails with an HTTP 404 about the model, list what your key can
actually use and pass it with `--llm-model`:

```bash
python3 -m tools.model_agent list-models               # uses the auto-detected provider
python3 -m tools.model_agent generate-recipe-from-llm --card card.json --llm-model gemini-2.5-flash ...
```

The result is a normal recipe — reviewable, diffable, and re-renderable offline —
not opaque generated code. Add `--generate-package` to also write the wrapper
package, and `--smoke-test` to launch and verify it (runs downloaded code; review
or sandbox first). This is the long-tail fallback; the deterministic templates
and hand-written recipes remain the fast, reproducible, offline path.

##### Filling a scaffold's TODOs with an LLM

`scaffold-recipe` and the LLM compose naturally: the scaffold deterministically
pins the input/output contract from a *real, working* wrapper and leaves the
dependencies and inference glue as `_todo` stubs; `complete-recipe` then asks an
LLM to fill exactly those stubs. The resolved I/O is preserved verbatim (the LLM
cannot change component names/types/order); it only supplies `framework`,
`inference.setup`/`body`, and any values the scaffold flagged as TODO (e.g.
unresolved dropdown choices):

```bash
python3 -m tools.model_agent scaffold-recipe \
  artifacts/model_agent/harvest/teamup-tech-demucs-source-separation/app.py \
  --output demucs_scaffold.json

python3 -m tools.model_agent complete-recipe demucs_scaffold.json \
  --repo speechbrain/sepformer-wsj02mix --output demucs_recipe.json
```

This is usually more reliable than `generate-recipe-from-llm` from scratch,
because the contract the LLM must satisfy is already fixed by static analysis —
the model only has to write the loading and inference code. `--card`/`--repo`
optionally enrich the prompt with the model's README.

Smoke-test a generated package (launches `app.py` and verifies HARP controls).
This runs downloaded third-party code, so only do it after review or inside a
sandbox/venv:

```bash
python3 -m tools.model_agent smoke-test artifacts/model_agent/generated/example
```

`generate-package`, `package-repo`, `generate-recipe`, `generate-recipe-from-llm`,
and `complete-recipe` also accept `--smoke-test` to run the same check immediately
after writing the package.

**Dependencies for the smoke-test.** A wrapper only boots if its dependencies
(`torch`, `speechbrain`, …) are importable. Rather than installing them into your
active interpreter — which risks breaking your base/conda environment with a
heavy, third-party/LLM-authored dependency list — add `--venv`:

```bash
python3 -m tools.model_agent smoke-test artifacts/model_agent/generated/example --venv
```

`--venv` builds an isolated virtual environment under `<package>/.venv`, installs
the package's `requirements.txt` into it, and runs the wrapper there. It is keyed
by a hash of `requirements.txt`, so it is created once and reused on later runs
(no reinstalling `torch` every time) and rebuilt automatically when the
requirements change. Your active environment is never touched. Use `--python
/path/to/python` instead if you want to point at an interpreter you manage
yourself.

For the real deployment target — a Hugging Face Space — you do not install
anything locally: the Space installs `requirements.txt` in its own clean
container. The local `--venv` smoke-test is a fast pre-flight before pushing.

### Deploying to a Hugging Face Space

If you (or a teammate) have Hugging Face access, deploying the package to a
Space is often the easiest way to verify everything end-to-end without
installing any dependencies locally — the Space's container installs
`requirements.txt` and runs `app.py` for you:

```bash
python3 -m tools.model_agent deploy-space \
  artifacts/model_agent/generated/speechbrain-sepformer-wsj02mix \
  --repo your-username/sepformer-harp
```

This creates (or reuses) the Space, then uploads the package folder. The
generated `README.md` already carries the `sdk: gradio` / `app_file: app.py`
front matter a Space needs, so the build starts automatically. `.venv/` and
`__pycache__/` are skipped during upload.

Requirements:

- `pip install huggingface_hub` (an intentionally optional dependency — the rest
  of the agent stays SDK-free);
- a **write** token, passed via `--token` or the `HF_TOKEN` /
  `HUGGING_FACE_HUB_TOKEN` environment variable (or an existing `huggingface-cli
  login`). Never paste a token where it can be logged or shared.

Other flags: `--private` (create a private Space), `--sdk` (defaults to
`gradio`), and `--message` (commit message).

Fetch a raw Hugging Face model repository and package it as a HARP-compatible
Hugging Face Space:

```bash
python3 -m tools.model_agent package-repo Awais/Audio_Source_Separation \
  --output artifacts/model_agent/hf_spaces
```

The package command creates one folder per model containing:

- `manifest.json`: normalized source, endpoint, license, and HARP controls data;
- `controls.json`: raw HARP controls payload returned by the endpoint;
- `README.md`: short human-review summary.

Generated wrapper packages contain:

- `app.py`: starter pyharp wrapper;
- `README.md`: Hugging Face Space metadata using HARP's Gradio layout;
- `requirements.txt`: dependencies for the generated wrapper;
- `packages.txt`: system package hints for the generated wrapper;
- `.harp/manifest.json`: score, task, I/O hints, and entrypoint metadata.
