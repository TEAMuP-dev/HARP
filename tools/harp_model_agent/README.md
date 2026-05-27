# HARP Model Agent

`harp-model-agent` discovers Hugging Face Spaces, probes HARP-compatible Gradio
endpoints, and packages model metadata into reviewable JSON manifests.

The agent is intentionally conservative:

- it only uses the Python standard library;
- it treats Hugging Face search results as candidates until a Space is probed;
- it packages metadata and endpoint contracts, not model weights;
- it writes deterministic package folders that can be reviewed before models are
  added to HARP's featured list.

## Examples

Discover Gradio Spaces:

```bash
python3 -m tools.harp_model_agent discover --query audio --author teamup-tech --limit 20
```

Probe known HARP endpoints:

```bash
python3 -m tools.harp_model_agent probe teamup-tech/demucs-source-separation
```

Package endpoints into folders:

```bash
python3 -m tools.harp_model_agent package \
  teamup-tech/demucs-source-separation \
  teamup-tech/midi-synthesizer \
  --output artifacts/harp_model_agent/packages
```

The package command creates one folder per model containing:

- `manifest.json`: normalized source, endpoint, license, and HARP controls data;
- `controls.json`: raw HARP controls payload returned by the endpoint;
- `README.md`: short human-review summary.

