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
      link: /content/intro
    - theme: alt
      text: Install HARP
      link: /content/install/
    - theme: alt
      text: Deploy a Model
      link: /content/pyharp_docs/setup

features:
  - icon:
      src: /logos/reaper-logo.png
    title: Works with your DAW
    details: Process audio or MIDI from REAPER, Logic Pro, Pro Tools, Ableton Live, or another DAW with a model, and send the result back.
    link: /content/setup/
    linkText: Set up your DAW
  - icon:
      src: /logos/hf-logo.svg
    title: Dozens of hosted models
    details: Separate stems, transcribe to MIDI, generate, restore, and analyze, with models that run on Hugging Face instead of your computer.
    link: /content/usage/models
    linkText: Browse the models
  - icon:
      src: /logos/python-logo-only.svg
    title: Deploy your own model
    details: Turn Python code for audio or MIDI into a HARP model with a few lines of PyHARP, then host it on Hugging Face, a server, or your own machine.
    link: /content/pyharp_docs/setup
    linkText: Deploy a model
---
