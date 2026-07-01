# How Model Deployment Works In HARP

HARP runs AI audio models through small pyHARP/Gradio wrappers. For a new model,
the wrapper is the important part: it loads the model, exposes the right inputs
and outputs, and gives HARP a stable endpoint to call.

The model agent helps create that wrapper and the Hugging Face Space package
around it.

## Pipeline

```mermaid
flowchart LR
    A["Find candidates"] --> B["Inspect model or Space"]
    B --> C["Design recipe"]
    C --> D["Generate app.py package"]
    D --> E["Smoke-test"]
    E --> F["Deploy Space"]
```

## Main Ideas

**Space**
A running Hugging Face app. HARP talks to Gradio endpoints exposed by Spaces.

**Wrapper / `app.py`**
The Python glue that loads a model and exposes HARP-compatible Gradio controls.

**Recipe**
A JSON spec for a wrapper: model metadata, dependencies, input/output
components, and inference code. Recipes are reviewable and can be rendered
repeatedly.

**Package**
The deployable folder: `app.py`, `requirements.txt`, `packages.txt`,
`README.md`, and `.harp/manifest.json`.

**Smoke test**
Launches the generated wrapper and checks that the endpoint exposes HARP
controls. This is the main local proof that a package starts.

## Common Workflows

**Wrap a known-framework model**

```bash
python3 -m tools.model_agent package-repo speechbrain/sepformer-wsj02mix
python3 -m tools.model_agent smoke-test artifacts/model_agent/hf_spaces/<package> --venv
```

**Generate a wrapper with an LLM**

```bash
python3 -m tools.model_agent generate-recipe-from-llm \
  --repo author/model \
  --inputs audio --outputs audio \
  --generate-package --smoke-test --venv
```

**Reuse an existing Space's real code**

```bash
python3 -m tools.model_agent generate-recipe-from-llm \
  --repo author/model \
  --space author/space \
  --output recipe.json

python3 -m tools.model_agent generate-recipe recipe.json
python3 -m tools.model_agent deploy-space artifacts/model_agent/generated/<package> \
  --repo your-name/duplicated-space --into-space
```

Use `--space` when the model is an app-like pipeline and the README is not
enough to reconstruct inference safely.

**Start from a proven wrapper**

```bash
python3 -m tools.model_agent harvest --author teamup-tech
python3 -m tools.model_agent analyze artifacts/model_agent/harvest --summary-only
python3 -m tools.model_agent scaffold-recipe artifacts/model_agent/harvest/<space>/app.py \
  --output scaffold.json
python3 -m tools.model_agent complete-recipe scaffold.json --repo author/model \
  --generate-package --smoke-test --venv
```

## What To Explain To Others

1. HARP needs a wrapper, not just model weights.
2. The agent turns model evidence into a recipe or package.
3. Existing wrappers are the best source of truth for HARP I/O shapes.
4. LLMs are optional and only draft the model-specific glue.
5. The generated package should be reviewed and smoke-tested before deployment.

For exact commands and flags, see [`README.md`](./README.md).
