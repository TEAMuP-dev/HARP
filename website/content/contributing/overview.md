# Building and Releasing HARP

HARP is part of a collaborative research project funded by
[NSF Award 2222129](https://www.nsf.gov/awardsearch/showAward?AWD_ID=2222129). All of its code is
open source, and we follow issues and pull requests on both the
[HARP](https://github.com/TEAMuP-dev/HARP) and [PyHARP](https://github.com/TEAMuP-dev/pyharp)
repositories.

There are three ways to contribute:

- **A model for others to use.** [Deploying Models with PyHARP](/content/pyharp_docs/setup) ends
  in a hosted app that any user of HARP can open. To propose it for the Home tab, open an
  [issue](https://github.com/TEAMuP-dev/HARP/issues).
- **A change to the application.** This page covers building, debugging, and releasing HARP.
- **Support for a new service.** A service with an API of its own is reached through a client class
  of its own, as [Adding an API Provider](/content/contributing/add_provider) describes.

Reports of bugs and requests for features are welcome as
[issues](https://github.com/TEAMuP-dev/HARP/issues). Attach the log to a bug report, which
`File > Settings > General > Open Log Folder` reveals, and which never contains API keys.

## Guidelines

- [Developing](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEVELOPING.md): coding
  conventions, error handling, logging, and layout.
- [Reviewing](https://github.com/TEAMuP-dev/HARP/blob/main/docs/REVIEWING.md): what a pull request
  needs, and how to review one. Every pull request needs two approvals.
- [Deployment](https://github.com/TEAMuP-dev/HARP/blob/main/docs/DEPLOYMENT.md): the conventions
  of the models we host.
- [Code Signing](https://github.com/TEAMuP-dev/HARP/blob/main/docs/SIGNING.md): signing and
  notarizing the release packages.
- [Website](https://github.com/TEAMuP-dev/HARP/blob/main/docs/WEBSITE.md): building and editing
  this site, and administering its hosting and domain.

## Code Organization

HARP is a [JUCE](https://juce.com) application in C++20, under
[`src/`](https://github.com/TEAMuP-dev/HARP/tree/main/src). A request passes through these parts:

- **`MainComponent`** holds the menu bar, the tabs, the media clipboard, and the status area.
- **`ModelTabContainer`** holds the Home tab, which browses the model catalog, and a tab for each
  open model.
- **`ModelTab`** controls one model, and runs its work on thread pools, so that nothing blocks the
  interface.
- **`Model`** holds the state of a model, parses its interface, and builds each request.
- **`Client`** and its subclasses translate requests into the API of a provider: `GradioClient` for
  any Gradio app, and `StabilityClient` for Stability AI.
- **`media/`** draws audio, MIDI, and file tracks, and the labels over them.

## Building

HARP builds with [CMake](https://cmake.org) 3.22 or newer and a C++20 compiler: Xcode on macOS,
Visual Studio on Windows, or GCC or Clang on Linux.

<!--@include: @/../README.md#build-->

Add `-G Ninja` to the configure command to build with [Ninja](https://ninja-build.org), and replace
`Debug` with `Release` for a release build. New source files go in `target_sources` in
`CMakeLists.txt`, since the build does not glob for them. Format C++ with `clang-format`, using the
`.clang-format` of the repository.

## Debugging

<!--@include: @/../README.md#debug-->

HARP logs every request, every change in the state of a model, and every error to `main.log`, which
explains most failures the interface reports. HARP runs as a single instance, so a second launch
hands its file arguments to the open window and exits. Close any running copy before starting one
under the debugger.

## Distribution

<!--@include: @/../README.md#distribution-->

To release a version, update `VERSION` and `RELEASE.md`, name the PyHARP it pairs with in the root
`README.md`, which [Setting Up PyHARP](/content/pyharp_docs/setup#versions) includes, and push a tag
such as `v4.0.0`. The [install page](/content/install/) builds its download links from `VERSION`,
so they lead to the packages of the new release once the workflow publishes them. The demo files it
links are an asset of the 4.0.0 release. To change them, attach a new `HARP-demo-files.zip` to the
latest release, and point the install page at that release.
