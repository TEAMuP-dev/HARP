# Walkthrough

This page tours the interface of HARP. The welcome tutorial, under `Help > Welcome Tutorial`, covers
the same ground inside the app.

<!-- TODO: screenshot. The whole window with a model tab open and the media clipboard showing,
     annotated with the five regions named below.
![The HARP window, with its main regions labeled](/images/window-overview.png)
-->

The window has five regions:

- **The menu bar.** `File` adds files to the media clipboard (`Open`), saves the selected one
  (`Save As`), and opens `Settings`. `View` shows or hides the status area, the media clipboard, and
  the preview pane. `Help` opens `About HARP` and the `Welcome Tutorial`.
- **The tab bar.** The `Home` tab lists the models, and each model you open gets a tab of its own.
- **The open tab**, which fills most of the window.
- **The media clipboard**, on the right, which holds files independently of any model.
- **The status area**, at the bottom. Its left half is a history of what each model has done, and
  its right half explains whatever is under the mouse.

## Finding and Running Models

<!-- TODO: screenshot. The Home tab with a search term entered, showing the category and domain
     chips, the search box, the "Hide models that need an API key" toggle, and several model cards.
![The Home tab, listing models by category](/images/home-tab.png)
-->

<!--@include: @/../README.md#workflow-->

A model tab shows the model card at the top, then the controls, then the input and output tracks.
Hover over a control to see what it does. [Using Hosted Models](/content/usage/models) explains the
badges on each card and the API keys some models need.

<!-- TODO: screenshot. A model tab after processing, such as Demucs, showing the model card, its
     controls, an input track, its output tracks, and the Process button.
![A model tab after processing, with its controls, input track, and output tracks](/images/model-tab.png)
-->

## Tracks

<!--@include: @/../README.md#tracks-->

<!-- TODO: screenshot. An output track with labels drawn over it, such as from S-KEY, with the mouse
     over a label and its description shown in the status area.
![Labels drawn over an output track](/images/output-labels.png)
-->

## Media Clipboard

<!--@include: @/../README.md#media-clipboard-->

<!-- TODO: screenshot. The media clipboard with two tracks in it, one being dragged onto an input
     track of a model, and the preview pane beneath the list. Use the same image as the README.
![Dragging a track from the media clipboard onto an input track of a model](/images/media-clipboard.png)
-->

## Settings

`File > Settings` has two tabs. `API Keys` holds a key for each service HARP can reach. `General`
opens the log folder and the settings file, clears the logs, and restores the default settings,
keeping API keys and custom models. The log records each request HARP makes, never an API key, and
is the first thing to attach to a [bug report](https://github.com/TEAMuP-dev/HARP/issues).
