## v4.1.0
### Overview
- HARP 4.1.0 is a maintenance release. It fixes settings that were not remembered, backs up the
  file that Send to DAW overwrites, gives the Settings, About, and tutorial windows the look of the
  main window, and keeps settings and logs in the standard place on Linux.

### Fixes
- **Settings**: The panels shown in the `View` menu and the choice to skip the welcome tutorial are
  remembered between sessions. HARP 4.0.0 could read a setting that was saved as on as off.
- **Window Size**: Hiding the status area or the media clipboard and showing it again no longer
  leaves the window larger than it was.
- **Window Position**: The window is saved together with its frame, so on Linux it no longer
  reopens lower by the height of its title bar each time.
- **Custom Paths**: The prompt for a custom path is shown inside the window. As a window of its
  own it could open empty on Linux. A local address with a path and without `http://`, such as
  `localhost:7860/demo`, is no longer mistaken for a Space.
- **MIDI Playback**: Notes that are sounding continue when the playback position is moved, where
  clicking in a track used to silence them. Scrolling with Cmd and Shift held moves a piano roll
  through pitch on macOS, as it does with Ctrl and Shift elsewhere.
- **Launching with Files**: A file passed at launch is imported once, however the system delivers
  it.
- **Stability**: Fixed a crash when a model answered with a list where HARP expected an object.

### Improvements
- **Send to DAW**: HARP copies the file it is about to overwrite to a `HARP Backups` folder in the
  system temporary folder, and names the copy in the status area. A result of more than 16 bits
  keeps 24 bits when it is converted to the format of the file it replaces.
- **Tracks**: An input track refuses a dropped file while its model is processing, and a label too
  long for its place ends with an ellipsis. The instructions of a MIDI track explain the colors of
  notes and the bar that shows velocity.
- **Model Tabs**: A tab opened from a custom path takes the name of its model once loaded. Error
  popups offer to report an issue only for models listed on the Home tab.
- **Settings, About, and Tutorial**: The three windows share the fonts, colors, and spacing of the
  main window. The About window links the third-party notices, which are now part of the
  repository.
- **Logging**: Everything is logged to one file, `main.log`, with the time on each line and the
  version and operating system at the start of each session.
- **Linux**: The settings, the cached list of models, and the log are kept in `~/.config/HARP/`.
  Files from `~/HARP/`, where 4.0.0 kept them, are moved on the first launch.

### Documentation and Deployment
- **Website**: The deployment guidelines are published on [harp.tools](https://harp.tools/), the
  two PyHARP examples build their apps from scratch, the download page covers uninstalling, and
  the demo files are listed with their sources.
- **Checking a Deployment**: The validation harness checks a model under any account, and its
  file checks run on a public Space without a token. The deployment guidelines end with the
  commands.
- **Model Validation**: A failing model now fails the daily run, and the harness also checks that a
  Space turns off server-side rendering and downloads its weights whole.
- **PyHARP**: The MIDI synthesizer example takes an optional soundfont.
- **Formatting**: The C++ and Python of the repository are formatted with pinned versions of
  clang-format and ruff, which a workflow checks on every push.

### Requirements
- Supports models deployed with [`pyharp` **v0.4.2**](https://github.com/TEAMuP-dev/pyharp/releases/tag/v0.4.2) and `gradio` **v6.13** or newer.
