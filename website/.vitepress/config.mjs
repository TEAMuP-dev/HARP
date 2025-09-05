import { defineConfig } from 'vitepress'

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: "HARP",
  // description: "Deep Learning in your DAW",

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
    hostname: 'https://harp.netlify.app',
    lastmodDateOnly: false
  },

  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Guide', link: '/content/intro' },
      // { text: 'API', link: 'https://harp-plugin.netlify.app/doxygen-docs/html/index.html'},
    ],

    footer: {
      message: 'Released under the BSD 3-Clause License.',
      copyright: 'Copyright © 2023-present TEAMuP-dev'
    },
    search: {
      provider: 'local'
    },

    sidebar: [
      {
        text: 'Docs',
        items: [
          { text: 'Intro', link: '/content/intro' },
          {
            text: "Installing HARP",
            items: [
              { text: 'Supported Operating Systems', link: '/content/supported_os' },
              { text: 'Mac OS', link: '/content/install/macos' },
              { text: 'Windows', link: '/content/install/windows' },
              { text: 'Linux', link: '/content/install/linux' },
            ]
          },
          {
            text: "Setup",
            items: [
              { text: 'Standalone', link: '/content/setup/standalone' },
              { text: 'Logic Pro X', link: '/content/setup/logic' },
              { text: 'Reaper', link: '/content/setup/reaper' },
              { text: 'Mixcraft', link: '/content/setup/mixcraft' },
              { text: 'Ableton Live', link: '/content/setup/ableton' },

            ]
          },

          {
            text: "Using HARP",
            items: [
              { text: 'Workflow', link: '/content/usage/workflow' },
              { text: 'Editing a Partial Region', link: '/content/usage/partial_track' },
              { text: 'Available Models', link: '/content/usage/models' },
              { text: 'Warnings', link: '/content/usage/warnings' },

            ]
          },

          {
             text: "Deploy Your Own Models with PyHARP",
             items: [
              { text: 'About PyHARP', link: '/content/pyharp_docs/overview' },
              { text: 'Installing PyHARP', link: '/content/pyharp_docs/install' },
              { text: 'HARP / PyHARP Compatibility', link: '/content/contributing/version_compat' },
              { text: 'PyHARP Essentials', link: '/content/pyharp_docs/pyharp_app' },
              { text: 'Example: Speech Separation with TIGER', link: '/content/pyharp_docs/example' },
              { text: 'Hosting Apps in the Cloud', link: '/content/pyharp_docs/host' },
             ]
          },

          {
            text: "Contributing to HARP",
            items: [
              { text: 'Overview', link: '/content/contributing/overview' },
              { text: 'Building HARP', link: '/content/contributing/build_source' },
              { text: 'Debugging', link: '/content/contributing/debug' },
              { text: 'Distribution', link: '/content/contributing/dist' },
            ]
          },

          /*{
            text: "ReadMe Pages",
            items: [
              { text: 'HARP Readme', link: '/content/HARP/README' },
              { text: 'pyHARP Readme', link: '/content/pyHARP/README' },
            ]
          },*/
         
          
        ]
      }
    ],

    socialLinks: [
      { icon: 'github', link: 'https://github.com/TEAMuP-dev/HARP' }
    ]
  }
})
