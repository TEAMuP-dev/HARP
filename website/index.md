---
# https://vitepress.dev/reference/default-theme-home-page
layout: home

hero:
  name: "HARP"
  text: "<span style='font-size: 0.8em'>The power \nof Deep Learning \nin your DAW</span>"
  tagline: Hosted, asynchronous, remote processing of audio and MIDI with machine learning
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
    title: Use in your DAW
    details: Process audio from REAPER, Logic Pro, Pro Tools, Ableton Live, and other DAWs with a model, and bring the result back.
    link: /content/setup/
    linkText: Set up your DAW
  - icon:
      src: /logos/hf-logo.svg
    title: Choose from dozens of models
    details: Separate stems, transcribe to MIDI, generate, restore, and analyze, with models hosted on Hugging Face and ready to run.
    link: /content/usage/models
    linkText: Browse the models
  - icon:
      src: /logos/python-logo-only.svg
    title: Deploy your own
    details: Wrap any Python code for audio or MIDI in a few lines with PyHARP, and run it on your machine, a server, or a Hugging Face Space.
    link: /content/pyharp_docs/setup
    linkText: Deploy a model
---
