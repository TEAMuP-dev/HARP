# Model Validation

This directory contains HARP's model validation tooling.

## What it does

- Reads the shared registry at `resources/models/model_registry.json`
- Verifies the registry has unique ids and featured model paths
- Runs local PyHARP example validations through pytest
- Runs remote HARP dropdown-model validation through native C++ tests
- Calls local PyHARP `process_fn` implementations with small fixture media files
- Verifies that outputs exist and match the expected file type
- Writes a machine-readable report to `artifacts/model_validation/latest.json`
- Writes a Markdown summary to `artifacts/model_validation/latest.md`
- Writes stable dashboard aliases to `artifacts/model_validation/status.json` and `artifacts/model_validation/dashboard.md`

## Run locally

Install the base dependencies:

```bash
python3 -m pip install -r requirements-model-validation.txt
```

Run the Python local-example validations:

```bash
python3 -m pytest tests/model_validation -rA
```

Write Python validation reports to a custom directory:

```bash
python3 -m pytest tests/model_validation -rA \
  --model-validation-report-dir build/model-validation
```

Build and run the native remote-model tests:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target HARPRemoteModelTests --config Release
build/HARPRemoteModelTests_artefacts/Release/HARPRemoteModelTests
```

Optionally limit the native run to a single remote model:

```bash
export HARP_MODEL_VALIDATION_ID=audioseal
build/HARPRemoteModelTests_artefacts/Release/HARPRemoteModelTests
```

In `Debug` builds, low-level JUCE `DBG(...)` logging from the existing client code will still appear.
Use the `Release` target above for the clean one-line-per-model output.

Provide a Stability key for the Stability dropdown models:

```bash
export HARP_STABILITY_API_KEY=...
```

## Notes

- The registry is shared with HARP's featured model picker through bundled `BinaryData`.
- Validation entries can declare optional Python modules; missing modules are reported as skipped instead of failing the full suite.
- Remote dropdown models are smoke-tested through HARP's existing C++ `Model` and client logic.
- Python now covers only local PyHARP examples and report generation.
- The scheduled GitHub Actions workflow uploads the report directory as an artifact and publishes `dashboard.md` to the workflow summary.
