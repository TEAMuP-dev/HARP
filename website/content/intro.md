# Welcome to HARP

[![HARP Repo](https://github-readme-stats.vercel.app/api/pin/?username=TEAMuP-dev&repo=HARP)](https://github.com/TEAMuP-dev/HARP)

HARP is a sample editor for **h**osted, **a**synchronous, **r**emote **p**rocessing of audio with
machine learning. It runs as a standalone application or as a plugin-like editor within a DAW, and
sends audio, MIDI, and metadata to [Gradio](https://www.gradio.app) endpoints for processing.

Most machine learning models for audio are impractical to run alongside a session. They expect a
GPU, a particular version of PyTorch, and weights measured in gigabytes, and they take far longer
than a buffer period to produce a result. HARP leaves each model where it is already installed,
whether that is a [Hugging Face Space](https://huggingface.co/spaces) or a machine on your own
network, and returns the result to the editor. Processing is asynchronous and happens outside the
audio thread, so a model that runs for a minute delays nothing but itself.

This gives users of DAWs that support external sample editors, among them
[REAPER](https://www.reaper.fm), [Logic Pro](https://www.apple.com/logic-pro/), and
[Ableton Live](https://www.ableton.com/en/live/), access to these models without leaving the
session.

To learn how to install, use, or contribute to HARP, see the linked documentation in the left sidebar, or our papers: [HARP 3.0](https://openreview.net/pdf?id=Ha2z921r51) (NeurIPS 2025 Workshop on AI for Music), [HARP 2.0](https://ismir2024program.ismir.net/lbd_497.html) (ISMIR 2024 Late Breaking Demo), and [HARP](https://neuripscreativityworkshop.github.io/2023/papers/ml4cd2023_paper23.pdf) (NeurIPS 2023 Workshop on Machine Learning for Creativity and Design).

<br/><br/>

<!-- TODO - Update this figure! -->
![herofig_revised](https://github.com/TEAMuP-dev/HARP/assets/26678616/c4f5cdbb-aaff-4196-b9d2-3b6f69130856)

<br/><br/>

# Our Team

<Team />
