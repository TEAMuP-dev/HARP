# Limitations

Some limits of HARP follow from processing remotely, and others are features we have yet to build.
Requests and contributions are welcome as [issues](https://github.com/TEAMuP-dev/HARP/issues).

## Processing

- **HARP processes files, not streams.** A model receives whole files and returns whole files, so
  HARP cannot process live input, follow the transport of a DAW, or respond to automation. It is an
  external editor rather than a plugin, and offers no VST3, AU, or AAX version.
- **Results replace the original.** Sending a result back to a DAW overwrites the file behind the
  region, with no undo in HARP (see
  [Protecting the Original](/content/setup/#protecting-the-original)).
- **An app runs one job at a time.** Requests from several users to the same app wait their turn.
- **Jobs have time limits.** PyHARP stops a job after 15 minutes by default, and a ZeroGPU request
  must fit within the remaining daily allowance of the caller.
- **Progress is not reported.** HARP shows the state of a request, but not the progress bars,
  notices, or warnings an app reports through Gradio, which reach only the page of the app.

## Network and Hosting

- **HARP needs a network connection**, except to reach an app running on your own machine.
- **Your audio leaves your machine.** Every input is uploaded to whoever hosts the model. For
  material that must stay private, host the model yourself (see
  [Hosting Elsewhere](/content/hosting/spaces#hosting-elsewhere)).
- **Hosted models can change.** A Space can sleep, run out of allowance, fail, or be taken down by
  its owner. A sleeping Space takes a minute or two to wake.
- **Custom paths cannot sign in.** HARP cannot log in to a Gradio app protected by a password.

## Media and Controls

- **Audio tracks** accept WAV, AIFF, FLAC, OGG, and MP3 files, and **MIDI tracks** accept `.mid`
  and `.midi` files. Other files travel through a generic file input or output, which HARP shows
  but does not play.
- **Controls** are limited to sliders, number boxes, dropdowns, multiselect dropdowns, toggles, text
  boxes, and file pickers. Other Gradio components, such as images or radio buttons, have no
  counterpart in HARP.
- **Text is an input only.** A model that produces text returns it as a file.
- **Labels are drawn over an output track** of the same request, so a model that returns labels
  also returns the media they describe.

## DAWs and Platforms

- **Only some DAWs open regions in HARP.** REAPER, Logic Pro, Ableton Live, and FL Studio use it as
  an external editor. Pro Tools, Cubase, Audacity, and Adobe Audition exchange files with HARP
  instead (see [Setting Up HARP](/content/setup/)).
- **Most DAWs open only audio** in an external editor, so MIDI is exchanged as files (see
  [MIDI](/content/setup/#midi)).
- **The Windows executable is not code signed**, so Windows warns before running it the first time.
- **Linux builds need glibc 2.35 or newer**, and run under X11 or XWayland.

## Versions

- **HARP calls the Gradio API**, which apps built on Gradio 4.x or earlier do not offer. Apps on
  Gradio 5.x work, but cannot tell HARP why a job failed.
- **HARP and PyHARP are released in pairs** (see [Versions](/content/pyharp_docs/setup#versions)),
  so an app is best deployed with the PyHARP that its users expect.
