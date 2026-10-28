# Overview

<!-- TODO: screenshot. A DAW session with HARP open beside it, mid-process on a region the DAW
     opened in it, so that the relationship between the two is clear at a glance. Use the same
     image as the README.
![HARP processing a region opened from a DAW session](/images/hero.png)
-->

<!--@include: @/../README.md#overview-->

## How It Works

HARP never runs a model itself. Most models are [Gradio](https://www.gradio.app) apps, built with
our companion library [PyHARP](https://github.com/TEAMuP-dev/pyharp) and hosted wherever their
hardware lives, and a few are commercial services that HARP calls directly, such as those of
Stability AI. Each model describes its interface to HARP, which draws it as native controls and
tracks.

1. **Open a model.** HARP reads the model card (name, author, description, and tags) and the
   components of the model: the tracks it reads and writes, and the controls that adjust it.
2. **Process.** HARP uploads the input tracks, sends the control values, and waits for the result
   without blocking the interface.
3. **Use the result.** Output audio, MIDI, files, and labels appear in output tracks, from which
   they can be played, saved, dragged into a DAW, or used as the input of another model.

<!-- The horizontal layout suits the content column, and the vertical one keeps the text legible on
     a phone. Both are draw.io SVGs (see docs/WEBSITE.md). -->
<figure style="margin:1.5rem auto;max-width:600px;text-align:center;">
  <picture>
    <source media="(max-width: 539px)" srcset="/images/harp-teaser-vertical.svg">
    <img src="/images/harp-teaser.svg"
         alt="Two regions. Producers launch HARP, on its own or from a DAW, and access deployed models. Developers write inference code, deploy it with PyHARP, and optionally host it remotely, or wrap an existing third-party API, and each of these routes leads into HARP."
         style="width:100%;border-radius:8px;">
  </picture>
  <figcaption style="font-size:0.9em;color:var(--vp-c-text-2);">
    Developers deploy models with PyHARP or wrap existing third-party APIs, and producers use any
    deployed model in HARP, on its own or from a DAW.
  </figcaption>
</figure>

A model only has to consume and produce files, so HARP works with any Python code and any
framework, from a few lines of signal processing to a large generative model on a GPU.

## What You Can Do

The Home tab of HARP lists the models we host, grouped by what they do:

- **Separate** a mix into stems, or extract a single instrument or voice.
- **Generate** music, sound effects, speech, or MIDI from a text prompt, or continue and edit a
  recording.
- **Transcribe** audio to MIDI, find beats, chords, and keys, or tag a recording.
- **Enhance** a recording by denoising, dereverberating, or restoring it.
- **Transform** audio with neural effects, timbre transfer, or automatic mixing.

HARP opens any HARP-compatible Gradio app by its address as well, including one running on your own
machine.

## Using This Guide

- **[Using HARP](/content/install/)**: install HARP, set it up alone or with your DAW, and learn
  the interface and the hosted models.
- **[Deploying Models with PyHARP](/content/pyharp_docs/setup)**: run a model of your own in
  HARP, through two examples taken from the models we host.
- **[Hosting Models](/content/hosting/spaces)**: put a model on Hugging Face Spaces, or reach an
  existing API.
- **[Contributing to HARP](/content/contributing/overview)**: build, debug, and extend the
  application.

## Citing HARP

If you use HARP in academic work, please cite the paper for the version you used.

<!--@include: @/../README.md#citations-->

## Our Team

HARP is developed by [TEAMuP](https://github.com/TEAMuP-dev), a collaboration funded by
[NSF Award 2222129](https://www.nsf.gov/awardsearch/showAward?AWD_ID=2222129).

<Team />
