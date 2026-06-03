# HARP Model Agent

`model_agent` discovers Hugging Face Spaces, probes HARP-compatible Gradio
endpoints, scores raw Hugging Face models, and packages model metadata into
reviewable artifacts.

The agent is intentionally conservative:

- it only uses the Python standard library;
- it treats Hugging Face search results as candidates until a Space is probed;
- it packages metadata and endpoint contracts, not model weights;
- it can score raw Hugging Face model cards before spending time on wrappers;
- it can render a starter `app.py` for `audio-to-audio` models;
- it writes deterministic package folders that can be reviewed before models are
  added to HARP's featured list.

## File Map

- `agent.py` contains the core behavior: Hugging Face discovery, HARP endpoint
  probing, task classification, license checks, compatibility scoring, template
  generation, package data structures, and package writing.
- `cli.py` exposes the core behavior as terminal commands.
- `__main__.py` lets you run the package with `python3 -m tools.model_agent`.
- `tests/test_agent.py` protects the behavior that is easiest to break while
  refactoring.

The tests use fake model names such as `example/audio-model`. They do not define
which models HARP supports; they only check that the agent can transform URLs,
parse Gradio responses, score candidates, render starter wrappers, and write
package files.

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

Package endpoints into folders:

```bash
python3 -m tools.model_agent package \
  teamup-tech/demucs-source-separation \
  teamup-tech/midi-synthesizer \
  --output artifacts/model_agent/packages
```

Score a raw Hugging Face model-card JSON file:

```bash
python3 -m tools.model_agent score-card artifacts/model_agent/cards/example.json
```

Render a starter `app.py` for a raw `audio-to-audio` model card:

```bash
python3 -m tools.model_agent render-app artifacts/model_agent/cards/example.json \
  --output artifacts/model_agent/generated/example/app.py
```

Write a generated wrapper package:

```bash
python3 -m tools.model_agent generate-package artifacts/model_agent/cards/example.json
```

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
