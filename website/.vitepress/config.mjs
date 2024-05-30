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
      { text: 'Content', link: '/content/intro' },
      { text: 'API', link: 'https://harp-plugin.netlify.app/doxygen-docs/html/index.html'},

    ],

    footer: {
      message: 'Released under the {{what}} License.',
      copyright: 'Copyright © 2023-present TEAMuP-dev'
    },
    search: {
      provider: 'local'
    },

    sidebar: [
      {
        text: 'Content',
        items: [
          { text: 'Intro', link: '/content/intro' },
          // { text: 'Runtime API Examples', link: '/api-examples' }
        ]
      }
    ],

    socialLinks: [
      { icon: 'github', link: 'https://github.com/TEAMuP-dev/HARP' }
    ]
  }
})
