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
      { text: 'Guide', link: '/intro' },
      { text: 'Deploy a Model', link: '/pyharp/setup' },
      { text: 'Contribute', link: '/contributing/overview' },
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
          { text: 'Overview', link: '/intro' },
        ]
      },
      {
        text: 'Using HARP',
        items: [
          { text: 'Downloading HARP', link: '/download/' },
          {
            text: 'Setting Up HARP',
            link: '/setup/',
            collapsed: false,
            items: [
              { text: 'Standalone', link: '/setup/standalone' },
              { text: 'REAPER', link: '/setup/reaper' },
              { text: 'Logic Pro', link: '/setup/logic' },
              { text: 'Pro Tools', link: '/setup/protools' },
              { text: 'Ableton Live', link: '/setup/ableton' },
              { text: 'Cubase', link: '/setup/cubase' },
              { text: 'Audacity', link: '/setup/audacity' },
              { text: 'FL Studio', link: '/setup/flstudio' },
              { text: 'Adobe Audition', link: '/setup/audition' },
            ]
          },
          { text: 'Walkthrough', link: '/usage/workflow' },
          { text: 'Using Hosted Models', link: '/usage/models' },
        ]
      },
      {
        text: 'Deploying Models with PyHARP',
        items: [
          { text: 'Setting Up PyHARP', link: '/pyharp/setup' },
          { text: 'Example 1: Key Detection', link: '/pyharp/example_skey' },
          { text: 'Example 2: Stem Separation', link: '/pyharp/example_demucs' },
        ]
      },
      {
        text: 'Hosting Models',
        items: [
          { text: 'Hugging Face Spaces', link: '/hosting/spaces' },
          { text: 'Third-Party APIs', link: '/hosting/third_party' },
        ]
      },
      {
        text: 'Contributing to HARP',
        items: [
          { text: 'Building and Releasing', link: '/contributing/overview' },
          { text: 'Adding an API Provider', link: '/contributing/add_provider' },
        ]
      },
      {
        text: 'Reference',
        items: [
          { text: 'Limitations', link: '/limitations' },
        ]
      },
    ],

    socialLinks: [
      { icon: 'github', link: 'https://github.com/TEAMuP-dev/HARP' }
    ]
  }
})
