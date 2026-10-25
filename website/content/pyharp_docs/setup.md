# Setting Up PyHARP

HARP can run any [Gradio](https://www.gradio.app) app that follows a few conventions, and
[PyHARP](https://github.com/TEAMuP-dev/pyharp) is the Python library that applies them. This page
explains how HARP talks to such an app, installs PyHARP, and runs one of its examples in HARP.

## How It Works

HARP reaches an app through the Gradio API, at three endpoints that PyHARP creates:

- **`controls`** returns the model card (name, author, description, and tags) and a description
  of each component, which HARP draws as controls and tracks.
- **`process`** receives the input files and control values, runs the processing function of the
  app, and returns the output files and labels.
- **`cancel`** stops the job that the same copy of HARP started.

PyHARP provides the pieces of such an app:

- **`ModelCard`**, which describes the model.
- **`build_endpoint`**, which connects the Gradio components to a processing function,
  `process_fn`, and creates the three endpoints. It runs `process_fn` in a worker process, so that
  `Cancel` in HARP stops the work, and names each returned file after the model and the output.
- **Helpers** for audio (`load_audio`, `save_audio`), MIDI (`load_midi`, `save_midi`), and labels
  (`LabelList`, `AudioLabel`, `MidiLabel`).
- **Tags** that place a model in the taxonomy of HARP: `Category`, `Subcategory`, `SampleRate`,
  and `Channels`.

Each Gradio component becomes part of the model tab in HARP:

| Gradio component | In HARP | Position |
| :-- | :-- | :-- |
| `gr.Audio(type="filepath")` | Audio track | Input or output |
| `gr.File(type="filepath", file_types=[".mid", ".midi"])` | MIDI track | Input or output |
| `gr.File(type="filepath")` with other file types | File picker, or output file | Input or output |
| `gr.Slider` | Slider | Input |
| `gr.Number` | Number box | Input |
| `gr.Dropdown` | Dropdown, or a multiselect with `multiselect=True` | Input |
| `gr.Checkbox` | Toggle | Input |
| `gr.Textbox` | Text box | Input |
| `gr.JSON`, returning a `LabelList` | Labels over an output track | Output |

The [README](https://github.com/TEAMuP-dev/pyharp#readme) of PyHARP documents the library in full,
and our [deployment guidelines](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md)
describe the conventions the models we host follow.

## Install

PyHARP requires Python 3.10 or newer. Install it into an environment of its own, so that its
dependencies, and those of each model you deploy, stay apart from the rest of your system:

```bash
python3 -m venv .venv
source .venv/bin/activate        # On Windows: .venv\Scripts\activate
pip install "git+https://github.com/TEAMuP-dev/pyharp.git@v0.4.2"
```

This also installs Gradio, the [audiotools](https://github.com/descriptinc/audiotools) of Descript
(which brings PyTorch and librosa), and [symusic](https://github.com/Yikai-Liao/symusic) for MIDI.
Install [FFmpeg](https://ffmpeg.org) as well, _e.g._, with `brew install ffmpeg` on macOS or
`sudo apt install ffmpeg` on Ubuntu, since several audio libraries decode compressed formats
through it.

### Versions

<!--@include: @/../README.md#version-compatibility-->

PyHARP 0.4.2 installs a Gradio of at least 6.13, and below 7. HARP calls the `/gradio_api/call/`
endpoints that Gradio 5.0 introduced, so an app on Gradio 4.x or earlier cannot load. An app on
Gradio 5.x loads, but cannot tell HARP why a job failed.

## Run an Example

PyHARP ships with four examples, minimal templates for each combination of input and output media,
and a tester of every component:

| Example | In | Out | Illustrates |
| --- | --- | --- | --- |
| [`pitch_shifter`](https://github.com/TEAMuP-dev/pyharp/tree/main/examples/pitch_shifter) | Audio | Audio | One track in, one control, and one track out. |
| [`midi_pitch_shifter`](https://github.com/TEAMuP-dev/pyharp/tree/main/examples/midi_pitch_shifter) | MIDI | MIDI | The same shape, on MIDI tracks. |
| [`midi_synthesizer`](https://github.com/TEAMuP-dev/pyharp/tree/main/examples/midi_synthesizer) | MIDI | Audio | Input and output tracks of different media types. |
| [`ui_tester`](https://github.com/TEAMuP-dev/pyharp/tree/main/examples/ui_tester) | Any | Audio, MIDI, file | Every control, track, and label type, with no real processing. |

To run the MIDI synthesizer, which renders a MIDI file to audio with a soundfont, clone PyHARP and
install it in editable mode, which covers everything the examples import:

```bash
git clone https://github.com/TEAMuP-dev/pyharp
pip install -e pyharp
python pyharp/examples/midi_synthesizer/app.py
```

The first run downloads the MuseScore General soundfont. Once the app is ready, it prints its
addresses:

```
* Running on local URL:  http://127.0.0.1:7860
* Running on public URL: https://<RANDOM_ID>.gradio.live
```

<!-- TODO: screenshot. A terminal after starting the MIDI synthesizer example, showing the local
     and public URLs.
![The terminal after starting an example, showing its local and public URLs](/images/example-terminal.png)
-->

The local URL serves the app to your own machine, on the next free port if `7860` is taken. The
public URL is a Gradio share link, which forwards to the app from anywhere until it expires after a
week, or until the app stops. Open the local URL in a browser to try the app on its Gradio page.

## Open It in HARP

1. On the `Home` tab of HARP, click `Custom Path...`.
2. Enter the local URL, _e.g._, `http://localhost:7860`, and click `Load`. The model opens in a tab
   of its own, and keeps a card on the Home tab.
3. Drag a MIDI file onto the `Input MIDI` track, and select `Process`. The rendered audio appears
   in the `Synthesized Audio` track.

<!-- TODO: screenshot. The MIDI synthesizer open in HARP, with a MIDI file in its input track and
     the rendered audio in its output track.
![The MIDI synthesizer in HARP, after rendering a MIDI file](/images/example-in-harp.png)
-->

HARP reads the interface of an app when it loads the model, so after changing a component, close
the tab of the model and open it again from its card. Stop the app with `Ctrl+C`.

## Next Steps

- [Example 1](/content/pyharp_docs/example_skey) walks through a small model that detects the key
  of a song.
- [Example 2](/content/pyharp_docs/example_demucs) walks through a larger one that separates stems
  on a GPU.
- [Hosting Models](/content/hosting/spaces) puts an app online, so that it runs without your
  machine.
