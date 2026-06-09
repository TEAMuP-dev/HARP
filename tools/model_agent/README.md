# HARP Model Agent

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
  input/output component shapes across the corpus (no code execution).
- `recipe.py` renders a runnable pyharp `app.py` (and the rest of a Space
  package) from a declarative recipe JSON describing inputs, outputs, framework,
  and inference glue.
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

Render an `app.py` from a recipe, or write the whole package:

```bash
python3 -m tools.model_agent render-recipe tools/model_agent/examples/recipe_stem_separation.json
python3 -m tools.model_agent generate-recipe tools/model_agent/examples/recipe_stem_separation.json
```

Two example recipes are committed under `examples/`: `recipe_stem_separation.json`
(audio + dropdown -> 4 audio stems + labels) and
`recipe_audio_to_audio_labels.json` (audio + slider -> audio + labels). Like the
other generators, `generate-recipe` accepts `--smoke-test` to launch and verify
the wrapper (runs downloaded code; review/sandbox first).

#### Scaffolding a recipe from a harvested wrapper

To start a recipe from an existing, well-formed wrapper rather than from
scratch, `scaffold-recipe` reads a harvested `app.py`, fills in the model card
and the input/output components from the statically-resolved shapes, and leaves
the parts that can't be derived (dependencies, dropdown choices, slider ranges,
and the inference glue) as clearly-marked `_todo` placeholders:

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

Smoke-test a generated package (launches `app.py` and verifies HARP controls).
This runs downloaded third-party code, so only do it after review or inside a
sandbox/venv:

```bash
python3 -m tools.model_agent smoke-test artifacts/model_agent/generated/example
```

`generate-package` and `package-repo` also accept `--smoke-test` to run the same
check immediately after writing the package.

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
