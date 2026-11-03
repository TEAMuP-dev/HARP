# Overview

<!-- TODO: screenshot. A DAW session with HARP open beside it, mid-process on a region the DAW
     opened in it, so that the relationship between the two is clear at a glance. Use the same
     image as the README.
![HARP processing a region opened from a DAW session](/images/hero.png)
-->

<!--@include: @/../README.md#overview-->

## How It Works

HARP never runs a model itself. Most models are [Gradio](https://www.gradio.app) apps built with
our companion library [PyHARP](https://github.com/TEAMuP-dev/pyharp), and hosted on Hugging Face
Spaces, a dedicated server, or a local machine. A few are third-party APIs that HARP calls directly,
such as that of Stability AI. Each model describes its interface to HARP, which displays it as native controls and
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

HARP's Home tab lists the models we host, in the categories of
[PyHARP's taxonomy](https://github.com/TEAMuP-dev/pyharp#tags):

- **Generation**: music, sound, or MIDI from a text prompt, and the continuation, infilling,
  accompaniment, and editing of an existing recording or score.
- **Synthesis**: an instrument performance rendered from MIDI, or speech and singing from text.
- **Effects**: neural emulations of analog gear, timbre transfer, and the removal of effects.
- **Enhancement**: denoising, dereverberation, bandwidth extension, and restoration.
- **Production**: automatic mixing, mixing style transfer, and mastering.
- **Separation**: a mix into stems, or a single source described in text or by example.
- **Analysis**: key, beats, chords, and structure, transcription to MIDI, speech to text, tagging,
  and quality scores, returned as labels over the audio, as MIDI, or as a file.
- **Utility**: tools such as watermarking.

HARP opens any HARP-compatible Gradio app by its address as well, including one running on your own
machine.

## Using This Guide

- **[Using HARP](/download/)**: download HARP, set it up as a standalone application or with
  your DAW, and learn the interface and the hosted models.
- **[Deploying Models with PyHARP](/pyharp/setup)**: run a model of your own in
  HARP, through two examples taken from the models we host.
- **[Hosting Models](/hosting/spaces)**: put a model on Hugging Face Spaces, or reach an
  existing API.
- **[Contributing to HARP](/contributing/overview)**: build, debug, and extend the
  application.

## Citing HARP

If you use HARP in academic work, please cite the paper for the version you used.

<!--@include: @/../README.md#citations-->

## Our Team

HARP is developed by [TEAMuP](https://github.com/TEAMuP-dev), a collaboration funded by
[NSF Award 2222129](https://www.nsf.gov/awardsearch/showAward?AWD_ID=2222129).

<Team />
