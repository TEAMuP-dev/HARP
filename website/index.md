---
# https://vitepress.dev/reference/default-theme-home-page
layout: home

hero:
  name: "HARP"
  text: "<span style='font-size: 0.8em'>The power \nof Deep Learning \nin your DAW</span>"
  #tagline: project tagline
  image:
    src: /logos/harp_logo.png
    alt: HARP logo
  actions:
    - theme: brand
      text: Get Started
      link: /content/intro
    # - theme: alt
    #   text: API Examples
    #   link: /api-examples

features:
  - icon:
      src: /logos/python-logo-only.svg
    title: Develop in Python
    details: Wrap audio and MIDI processing applications – from simple effects to state-of-the-art neural network models – in a few lines of Python
  - icon:
      src: /logos/hf-logo.svg
    title: Deploy on a server
    details: Host your applications locally or on a remote server to meet your compute needs
  - icon:
      src:  /logos/reaper-logo.png
    title: Use in your DAW
    details: Point HARP at your audio processing application to pull it into your DAW!
---