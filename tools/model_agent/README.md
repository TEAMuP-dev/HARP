# HARP Model Agent

`model_agent` helps package open-source audio models for HARP. It can discover
Hugging Face candidates, inspect existing HARP Spaces, generate pyHARP wrappers,
write deployment packages, and optionally use an LLM for model-specific glue.

For the newcomer-friendly explanation, see [`OVERVIEW.md`](./OVERVIEW.md).

## What It Writes

Generated packages are reviewable Hugging Face Space folders:

- `app.py` - pyHARP/Gradio wrapper for the model.
- `requirements.txt` - Python dependencies, including pyHARP.
- `packages.txt` - optional apt packages such as `ffmpeg`.
- `README.md` - Space metadata and short package notes.
- `.harp/manifest.json` - model id, task, I/O, framework, and entry point.

Endpoint packages created from already-running Spaces contain:

- `manifest.json` - normalized source, endpoint, license, and HARP controls.
- `controls.json` - raw HARP controls payload.
- `README.md` - human review summary.

## Safety Model

- Offline commands: `score-card`, `render-app`, `generate-package`,
  `render-recipe`, `generate-recipe`.
- Network-only commands: `discover`, `probe`, `harvest`, `analyze --check-health`,
  `package`, `package-repo`, `deploy-space`.
- Code-running commands: `smoke-test` and any `--smoke-test` flag. These launch
  generated wrappers and may download third-party model code. Prefer `--venv`.

The agent only writes wrapper packages and metadata. It does not vendor model
weights.

## Commands

Run from the repository root:

```bash
python3 -m tools.model_agent <command> [options]
```

### Discover And Inspect

```bash
python3 -m tools.model_agent discover --query audio --author teamup-tech --limit 20
python3 -m tools.model_agent probe teamup-tech/demucs-source-separation
python3 -m tools.model_agent package teamup-tech/demucs-source-separation
```

Use `discover` for Hugging Face Spaces, `probe` to read a live HARP/Gradio
endpoint contract, and `package` to save that contract into a review folder.

### Learn From Existing Wrappers

```bash
python3 -m tools.model_agent harvest --author teamup-tech --output artifacts/model_agent/harvest
python3 -m tools.model_agent analyze artifacts/model_agent/harvest --summary-only
python3 -m tools.model_agent analyze artifacts/model_agent/harvest --check-health
```

`harvest` downloads `app.py` files without running them. `analyze` statically
extracts component shapes, pyHARP usage, GPU decorators, and recipe eligibility.
`--check-health` also probes each harvested Space's endpoint.

### Deterministic Generation

```bash
python3 -m tools.model_agent score-card tools/model_agent/examples/example.json
python3 -m tools.model_agent render-app tools/model_agent/examples/example.json \
  --output artifacts/model_agent/generated/example/app.py
python3 -m tools.model_agent generate-package tools/model_agent/examples/example.json
python3 -m tools.model_agent package-repo speechbrain/sepformer-wsj02mix
```

These commands work from a model-card JSON shaped like:

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

`render-app` currently emits runnable templates only for supported frameworks
(SpeechBrain). Unsupported frameworks are refused instead of producing a wrapper
known to fail.

### Recipe Generation

A recipe is a declarative wrapper spec: model metadata, dependencies, ordered
inputs/outputs, and inference code.

```bash
python3 -m tools.model_agent render-recipe tools/model_agent/examples/recipe_stem_separation.json
python3 -m tools.model_agent generate-recipe tools/model_agent/examples/recipe_stem_separation.json
python3 -m tools.model_agent scaffold-recipe \
  artifacts/model_agent/harvest/teamup-tech-demucs-source-separation/app.py \
  --output demucs_scaffold.json
```

Supported input types: `audio`, `file`, `dropdown`, `slider`, `textbox`,
`number`, `checkbox`.

Supported output types: `audio`, `file`, `labels`.

`scaffold-recipe` fills the I/O contract from a harvested wrapper and leaves
dependencies/inference as `_todo` stubs.

### LLM-Assisted Recipes

```bash
export GEMINI_API_KEY=...        # or ANTHROPIC_API_KEY / OPENAI_API_KEY

python3 -m tools.model_agent generate-recipe-from-llm \
  --repo speechbrain/sepformer-wsj02mix \
  --inputs audio --outputs audio,labels \
  --output artifacts/model_agent/recipes/sepformer.json

python3 -m tools.model_agent complete-recipe demucs_scaffold.json \
  --repo speechbrain/sepformer-wsj02mix \
  --output demucs_recipe.json
```

Useful grounding options:

- `--space <author/space>` - include an existing Space's `app.py` and local
  modules. Use this when porting app-like Spaces; deploy the result into a
  duplicate Space with `deploy-space --into-space`.
- `--github <owner/repo> [--ref <branch/tag/SHA>]` - include GitHub source and
  add the repo as a `git+https://...` dependency.
- `--llm-model <name>` - override the provider default. Use `list-models` if a
  provider rejects the default model name.

The LLM produces a recipe, not final opaque code. The recipe is validated,
rendered, and can be smoke-tested like any other package.

### Smoke Test

```bash
python3 -m tools.model_agent smoke-test artifacts/model_agent/generated/example --venv
python3 -m tools.model_agent generate-recipe recipe.json --smoke-test --venv
```

`--venv` creates or reuses `<package>/.venv` based on a hash of
`requirements.txt`, so the active Python environment is not modified.

### Deploy To Hugging Face

```bash
python3 -m tools.model_agent deploy-space \
  artifacts/model_agent/generated/<package> \
  --repo your-username/your-space
```

Requirements:

- `pip install huggingface_hub`
- a write token via `--token`, `HF_TOKEN`, `HUGGING_FACE_HUB_TOKEN`, or cached
  `huggingface-cli login`

For wrappers that import modules from an existing Space, duplicate that Space on
Hugging Face and overlay only the HARP wrapper:

```bash
python3 -m tools.model_agent deploy-space \
  artifacts/model_agent/generated/<package> \
  --repo your-username/<duplicated-space> \
  --into-space
```

`--into-space` preserves the Space's model code while reconciling Gradio pins
with pyHARP. If you have a known-good environment, add `--freeze-from
working.txt` to lock model-critical ML libraries while leaving Gradio-coupled
infrastructure resolvable.

## Tests

```bash
python3 -m unittest discover -s tools/model_agent/tests -v
```

The suite is offline and uses only the Python standard library.

## File Map

- `agent.py` - discovery, probing, packaging, smoke tests, Space deployment.
- `analyze.py` - static wrapper analysis.
- `recipe.py` - recipe validation/rendering.
- `llm.py` - optional LLM providers and repair loop.
- `cli.py` / `__main__.py` - command-line interface.
- `tests/test_agent.py` - regression coverage.
