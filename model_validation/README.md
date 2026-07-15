# HARP Model Validation

Automated validation of HARP model deployments. Verifies that each deployment
is reachable, exposes the HARP gradio endpoints (`/controls`, `/process`),
and can actually process test inputs end-to-end.

Two tiers share the same harness ([validate_models.py](validate_models.py)):

| Tier | What it validates | What a failure means |
|---|---|---|
| **Baseline** (`--local-examples`) | The apps under `pyharp/examples/`, launched locally | pyharp or gradio itself is broken |
| **Spaces** (default) | Every Hugging Face Space under `teamup-tech` | That specific deployment is broken (build error, HF platform change, dependency drift, ...) |

Models are validated **independently**: each model (and each test case within
it) runs inside its own error boundary, so a crash, hang, or timeout in one
model never stops validation of the others. All results are collected into a
single report and the process exits non-zero only after everything has run.

A daily GitHub Action ([model_validation.yml](../.github/workflows/model_validation.yml))
runs both tiers at 06:30 UTC and publishes the results to the run summary and
as artifacts. Model failures do not turn the run red (no notification emails);
check the run summary for results.

## Token setup (IMPORTANT — read this)

ZeroGPU spaces require an authenticated request to obtain GPU quota, and
private spaces require read access. Validation reads the token **only** from
the `HF_TOKEN` environment variable.

- **Never** commit a token, paste it in an issue/PR, or pass it as a
  command-line argument (argv is visible in process listings).
- **CI:** add the token as a repository secret named `HF_TOKEN`
  (Settings → Secrets and variables → Actions). GitHub masks secrets in logs.
- **Locally:** `export HF_TOKEN=...` in your shell (consider `read -s` so it
  stays out of shell history).
- Use a [fine-grained token](https://huggingface.co/settings/tokens) scoped to
  read access on the org's spaces. Grant write access only if you want
  `--restart-failed` / the workflow's restart option to work.
- **If a token is ever exposed, rotate it immediately** at
  https://huggingface.co/settings/tokens.

## Running locally

```bash
pip install -r model_validation/requirements.txt

# All spaces in the org
export HF_TOKEN=hf_...   # see token setup above
python model_validation/validate_models.py

# A single space, with verbose errors
python model_validation/validate_models.py --spaces teamup-tech/pitch_shifter --verbose

# Exclude specific models (also configurable via `exclude` in config.yml)
python model_validation/validate_models.py --exclude teamup-tech/broken-space

# Availability + /controls only (fast; no inference, no GPU quota used)
python model_validation/validate_models.py --skip-process

# Try to restart crashed/stopped spaces first (token needs write access)
python model_validation/validate_models.py --restart-failed

# Baseline tier (needs pyharp + example deps; no token required)
pip install torch torchaudio --index-url https://download.pytorch.org/whl/cpu
pip install -e ./pyharp
python model_validation/validate_models.py --local-examples
```

Reports land in `reports/` as `report.json` (machine-readable) and
`report.md` (human-readable table). Exit code is `0` when everything passes,
`1` on any model failure, `2` on configuration/infrastructure errors.

## Excluding models

Two equivalent ways, merged together:

- `exclude` in [config.yml](config.yml) — for permanent exclusions
  (non-HARP spaces, archived deployments, known-broken examples).
- `--exclude <model> ...` on the command line — for ad-hoc runs.

Use the space id (`teamup-tech/some-space`) for remote models and
`local/<example-dir>` (e.g. `local/midi_synthesizer`) for baseline examples.

## Per-model test cases

By default every model gets one `default` test case with inputs synthesized
from its `/controls` spec (a sine-sweep WAV for audio tracks, a two-note MIDI
file for MIDI tracks, declared default values for sliders/toggles/etc.).

To validate deployment-specific behavior, add named cases in
[config.yml](config.yml). Cases override control values by their **label** and
can substitute custom input files (checked into `model_validation/test_data/`):

```yaml
overrides:
  teamup-tech/pitch_shifter:
    process_timeout: 900          # this model is slow; extend the timeout
    test_cases:
      - name: default             # keep the synthesized case
      - name: extreme-shift
        controls:
          "Pitch Shift (semitones)": 24
      - name: real-audio
        files:
          "Input Audio A": test_data/short_vocal.wav
```

Local examples use the key `local/<example-dir>`, e.g. `local/pitch_shifter`.
See the comment block at the top of config.yml for the full schema.

## CI behavior

- **Schedule:** daily at 06:30 UTC; also runnable manually from the Actions
  tab (with options to validate specific spaces, skip inference, or
  auto-restart crashed spaces).
- **Reports:** markdown summary on each run + JSON/markdown artifacts.
- **Failure signal:** model failures keep the run green (deployments are not
  yet stable enough for daily failure emails to be useful) — they show up as
  warning annotations and in the run summary/report instead. No issues are
  opened and no emails are sent. Only infrastructure errors (bad token,
  discovery failure) turn the run red, since those mean validation itself
  has stopped working.
- **Opting back into notifications later:** remove the exit-code handling in
  the two `Validate` steps of the workflow so the script's exit code 1
  propagates; failed runs then turn red and GitHub emails maintainers.
- Baseline failures indicate a pyharp/gradio-level breakage that likely
  affects every deployment — fix those before debugging individual spaces.
