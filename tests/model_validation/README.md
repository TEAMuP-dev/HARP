# Model Validation

This directory contains the first pass of HARP's automated model validation harness.

## What it does

- Reads the shared registry at `resources/models/model_registry.json`
- Verifies the registry has unique ids and featured model paths
- Runs enabled validation entries through pytest
- Calls local PyHARP `process_fn` implementations with small fixture media files
- Exercises remote Hugging Face dropdown models through the same Gradio API flow HARP uses
- Exercises Stability dropdown models when an API key is provided
- Verifies that outputs exist and match the expected file type
- Writes a machine-readable report to `artifacts/model_validation/latest.json`
- Writes a Markdown summary to `artifacts/model_validation/latest.md`
- Writes stable dashboard aliases to `artifacts/model_validation/status.json` and `artifacts/model_validation/dashboard.md`

## Run locally

Install the base dependencies:

```bash
python3 -m pip install -r requirements-model-validation.txt
```

Run the validation suite:

```bash
python3 -m pytest tests/model_validation -rA
```

Write reports to a custom directory:

```bash
python3 -m pytest tests/model_validation -rA \
  --model-validation-report-dir build/model-validation
```

Enable validations that require network access:

```bash
python3 -m pytest tests/model_validation -rA --run-network-validation
```

You can also enable network-backed validation with:

```bash
export HARP_ENABLE_NETWORK_VALIDATION=1
```

Provide a Stability key for the Stability dropdown models:

```bash
export HARP_STABILITY_API_KEY=...
```

## Notes

- The registry is shared with HARP's featured model picker through bundled `BinaryData`.
- Validation entries can declare optional Python modules; missing modules are reported as skipped instead of failing the full suite.
- Remote Hugging Face dropdown models are smoke-tested through their `controls` and `process` endpoints when network validation is enabled.
- Stability dropdown models are smoke-tested through the provider API when `HARP_STABILITY_API_KEY` or `STABILITY_API_KEY` is set.
- The scheduled GitHub Actions workflow uploads the report directory as an artifact and publishes `dashboard.md` to the workflow summary.
