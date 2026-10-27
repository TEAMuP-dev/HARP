import { defineConfig } from 'vitepress'

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: "HARP",
  description: "Machine learning models for audio and MIDI, run remotely from your DAW.",

  ignoreDeadLinks: [
    // ignore all localhost links
    /^https?:\/\/localhost/,
    // custom function, ignore all links include "ignore"
    (url) => {
      return url.toLowerCase().includes('ignore')
    }
  ],

  markdown: {
    lineNumbers: true
  },

  sitemap: {
    hostname: 'https://harp.tools',
    lastmodDateOnly: false
  },

  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Guide', link: '/content/intro' },
      { text: 'Deploy a Model', link: '/content/pyharp_docs/setup' },
      { text: 'Contribute', link: '/content/contributing/overview' },
    ],

    footer: {
      message: 'Released under the BSD 3-Clause License.',
      copyright: 'Copyright © 2026 TEAMuP-dev'
    },

    search: {
      provider: 'local'
    },

    editLink: {
      pattern: 'https://github.com/TEAMuP-dev/HARP/edit/main/website/:path',
      text: 'Edit this page on GitHub'
    },

    outline: {
      level: [2, 3]
    },

    // Ordered as a new user meets HARP: using it, deploying a model for it, hosting that model,
    // and contributing to the application
    sidebar: [
      {
        text: 'Introduction',
        items: [
          { text: 'Overview', link: '/content/intro' },
        ]
      },
      {
        text: 'Using HARP',
        items: [
          { text: 'Installing HARP', link: '/content/install/' },
          {
            text: 'Setting Up HARP',
            link: '/content/setup/',
            collapsed: false,
            items: [
              { text: 'Standalone', link: '/content/setup/standalone' },
              { text: 'REAPER', link: '/content/setup/reaper' },
              { text: 'Logic Pro', link: '/content/setup/logic' },
              { text: 'Pro Tools', link: '/content/setup/protools' },
              { text: 'Ableton Live', link: '/content/setup/ableton' },
              { text: 'Cubase', link: '/content/setup/cubase' },
              { text: 'Audacity', link: '/content/setup/audacity' },
              { text: 'FL Studio', link: '/content/setup/flstudio' },
              { text: 'Adobe Audition', link: '/content/setup/audition' },
            ]
          },
          { text: 'Walkthrough', link: '/content/usage/workflow' },
          { text: 'Using Hosted Models', link: '/content/usage/models' },
        ]
      },
      {
        text: 'Deploying Models with PyHARP',
        items: [
          { text: 'Setting Up PyHARP', link: '/content/pyharp_docs/setup' },
          { text: 'Example 1: Key Detection', link: '/content/pyharp_docs/example_skey' },
          { text: 'Example 2: Stem Separation', link: '/content/pyharp_docs/example_demucs' },
        ]
      },
      {
        text: 'Hosting Models',
        items: [
          { text: 'Hugging Face Spaces', link: '/content/hosting/spaces' },
          { text: 'Third-Party APIs', link: '/content/hosting/third_party' },
        ]
      },
      {
        text: 'Contributing to HARP',
        items: [
          { text: 'Building and Releasing', link: '/content/contributing/overview' },
          { text: 'Adding an API Provider', link: '/content/contributing/add_provider' },
        ]
      },
      {
        text: 'Reference',
        items: [
          { text: 'Limitations', link: '/content/limitations' },
        ]
      },
    ],

    socialLinks: [
      { icon: 'github', link: 'https://github.com/TEAMuP-dev/HARP' }
    ]
  }
})
