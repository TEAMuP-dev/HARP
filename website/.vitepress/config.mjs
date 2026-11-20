import { defineConfig } from 'vitepress'

// The address of the site, which the sitemap, the canonical links, and the link previews share
const hostname = 'https://harp.tools'

// The description of any page that does not give one of its own in its frontmatter
const description = 'Machine learning models for audio and MIDI, run remotely from your DAW.'

// Gives a heading the anchor that GitHub gives it, so that a link to a heading within text
// included from README.md or docs/ works in both places. GitHub drops punctuation, where
// VitePress would turn it into hyphens. As in VitePress, an underscore goes before a leading
// digit, which keeps the anchor a valid CSS selector.
const slugify = (heading) =>
  heading
    .trim()
    .toLowerCase()
    .replace(/[^\p{L}\p{M}\p{Nd}\p{Pc}\- ]/gu, '')
    .replace(/ /g, '-')
    .replace(/^(\d)/, '_$1')

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: "HARP",
  description,

  // Links and the sitemap name each page without its .html extension, a form in which Netlify
  // serves it. The address with the extension loads as well, and the links in released versions
  // of HARP use it.
  cleanUrls: true,

  // What a link to the site shows where it is shared. Each page adds its own title, description,
  // and address in transformPageData below.
  head: [
    ['meta', { property: 'og:type', content: 'website' }],
    ['meta', { property: 'og:site_name', content: 'HARP' }],
    ['meta', { property: 'og:image', content: `${hostname}/images/hero.png` }],
    ['meta', { name: 'twitter:card', content: 'summary_large_image' }],
  ],

  ignoreDeadLinks: [
    // ignore all localhost links
    /^https?:\/\/localhost/,
    // custom function, ignore all links include "ignore"
    (url) => {
      return url.toLowerCase().includes('ignore')
    }
  ],

  markdown: {
    lineNumbers: true,
    anchor: { slugify }
  },

  sitemap: {
    hostname,
    lastmodDateOnly: false
  },

  // Names one address for each page as its canonical one, since Netlify serves a page both with
  // and without its .html extension, and gives link previews the title and description of the
  // page. The addresses match those of the sitemap.
  transformPageData(pageData) {
    const url = `${hostname}/${pageData.relativePath}`
      .replace(/index\.md$/, '')
      .replace(/\.md$/, '')

    pageData.frontmatter.head ??= []
    pageData.frontmatter.head.push(
      ['link', { rel: 'canonical', href: url }],
      ['meta', { property: 'og:url', content: url }],
      ['meta', { property: 'og:title', content: pageData.title || 'HARP' }],
      ['meta', { property: 'og:description', content: pageData.description || description }],
    )
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
          { text: 'Deployment Guidelines', link: '/pyharp/guidelines' },
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
