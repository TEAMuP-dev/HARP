---
# https://vitepress.dev/reference/default-theme-home-page
layout: home

hero:
  name: "HARP"
  # The non-breaking spaces keep the end of each line together when it wraps
  text: Machine learning in&nbsp;your&nbsp;DAW
  tagline: Hosted, asynchronous, remote processing of&nbsp;audio&nbsp;and&nbsp;MIDI
  image:
    src: /logos/harp_logo.png
    alt: HARP logo
  actions:
    - theme: brand
      text: Get Started
      link: /intro
    - theme: alt
      text: Download HARP
      link: /download/
    - theme: alt
      text: Deploy a Model
      link: /pyharp/setup

features:
  - icon:
      src: /logos/reaper-logo.png
    title: Works with your DAW
    details: Open audio from REAPER, Logic Pro, Ableton Live, and other DAWs, process it with a model, and send the result back, or exchange audio and MIDI files with any DAW.
    link: /setup/
    linkText: Set up your DAW
  - icon:
      src: /logos/hf-logo.svg
    title: Dozens of hosted models
    details: Separate stems, transcribe to MIDI, find the key and the beats, denoise, master, or generate music and speech from text, with models that run remotely rather than on your computer.
    link: /usage/models
    linkText: Browse the models
  - icon:
      src: /logos/python-logo-only.svg
    title: Deploy your own model
    details: Wrap inference code for audio or MIDI in a PyHARP app, then host it on Hugging Face, a server, or your own machine.
    link: /pyharp/setup
    linkText: Deploy a model
---
