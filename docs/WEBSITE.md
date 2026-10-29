# Website

Guidelines for the HARP website at [harp.tools](https://harp.tools/): how to build and edit it, how
it deploys, and how to administer its hosting and its domain, including how to hand both to a new
maintainer. The site is built from [`website/`](../website) with
[VitePress](https://vitepress.dev/), and hosted by [Netlify](https://www.netlify.com/).

## Table of Contents

- [Building Locally](#building-locally)
- [Editing the Content](#editing-the-content)
  - [Screenshots](#screenshots)
  - [Moving a Page](#moving-a-page)
- [Dependencies](#dependencies)
- [Deploying](#deploying)
- [Administration](#administration)
  - [Netlify](#netlify)
  - [Domain](#domain)
  - [DNS and HTTPS](#dns-and-https)
  - [Renewing and Handing Off](#renewing-and-handing-off)
  - [Changing the Domain](#changing-the-domain)

## Building Locally

Install [Node.js](https://nodejs.org/) 24, the version Netlify uses, then, in `website/`:

```bash
corepack enable          # Provides the pnpm version named in package.json
pnpm install
pnpm run docs:dev        # Serves the site at http://localhost:5173, reloading on every change
```

`pnpm run build` makes the production build in `website/.vitepress/dist`, and
`pnpm run docs:preview` serves it. The build fails on a link to a page that does not exist.

## Editing the Content

- **Pages** are the Markdown files under `website/content/`, and `website/index.md` is the home
  page.
- **Navigation**, the sidebar and the top bar, is set in `website/.vitepress/config.mjs`. A new
  page needs an entry there.
- **Shared text is included, not copied.** Pages include sections of the root `README.md` and of the
  guidelines under `docs/`, which stay the source of truth for them. A section is marked in its
  source with a pair of comments,

  ```markdown
  <!-- #region build -->
  ...
  <!-- #endregion build -->
  ```

  and a page includes it with `<!--@include: @/../README.md#build-->`, where `@` is `website/`. An
  include without a region takes the whole file, as `content/contributing/add_provider.md` does with
  `docs/PROVIDERS.md`. Included text appears on GitHub and on the site alike, so its links have to
  be absolute URLs.
- **`website/scripts/check-includes.mjs`** runs before every build, and fails it if an include
  names a file or region that does not exist. VitePress would otherwise include the whole file in
  place of a missing region.
- **The two examples** under `website/content/pyharp_docs/` quote the code of the S-KEY and Demucs
  Spaces at pinned revisions. Update the revision and the quoted lines together.
- **The install page** links the packages that the release workflow attaches to the GitHub release
  named by the root `VERSION`. `website/content/install/release.data.mjs` builds the links when the
  site is built, and the `DownloadButtons` component of the theme shows them, with the platform of
  the visitor highlighted. The site holds no copy of the packages.

### Screenshots

- **Images that the README shares** with the site go in `website/public/images/`, which the site
  serves at `/images/<name>.png`. The README refers to them as `website/public/images/<name>.png`,
  and the README of PyHARP as `https://harp.tools/images/<name>.png`, so that one file serves all
  three.
- **Images used by one page alone** can sit in `website/content/images/` instead.
- **Figures**, such as the teaser on the introduction page, are draw.io exports kept under
  `website/public/images/` with the diagram embedded, so that they open in draw.io for editing.
  Export them with a white background and `color-scheme: light`, since draw.io writes its colors
  as `light-dark()` pairs, which turn black text white in a dark theme. The HARP mark in the
  teaser embeds `resources/icons/harp_logo.png`, scaled to 200 px.

### Moving a Page

Released versions of HARP and PyHARP link to some pages, such as `/content/intro.html` and
`/content/usage/models.html`, so a page cannot simply be renamed. Add a redirect from its old URL to
`website/public/_redirects` whenever a page moves or is removed, in both forms, with and without
`.html`.

## Dependencies

- **Vite is overridden to 6.4.5 or newer** in `website/package.json`. VitePress 1.6 depends on Vite
  5, whose development server has security advisories that are fixed only in Vite 6.4.4 and later.
  Vite 6 builds this site with the same output. Remove the override once a stable VitePress depends
  on a fixed Vite itself.
- **The install script of esbuild is skipped**, since it only checks the binary that pnpm installs
  anyway.
- **Netlify** takes Node.js from `website/netlify.toml`, and pnpm from the `packageManager` field of
  `website/package.json`.
- `pnpm audit` lists any new advisories. Update the lockfile with `pnpm update`, and commit it.

## Deploying

Netlify builds the project `teamup-harp` from `website/`, its base directory, with the command and the
publish directory in [`netlify.toml`](../website/netlify.toml):

- **`main`** deploys to production, at [harp.tools](https://harp.tools/).
- **Every pull request against `main`** gets a deploy preview at
  `deploy-preview-<number>--teamup-harp.netlify.app`, linked from the checks of the pull request.
- **Every branch named `website/*`** gets a branch deploy at `<branch>--teamup-harp.netlify.app`, with the
  slash of the name replaced by a hyphen. The pattern is set as **Branch deploys**, under
  **Project configuration → Build & deploy → Continuous deployment → Branches and deploy contexts**.

A build that fails leaves the last successful deploy in place. `Deploys`, in the project on Netlify, lists
every build with its log, and publishing an earlier deploy from there rolls the site back.

**The project answers at `teamup-harp.netlify.app` as well**, as every Netlify project does at its
own address. The first rule in `website/public/_redirects` forwards every page there to the same path
on harp.tools, so that the site is served from one address. The rule names the production address
alone, so deploy previews and branch deploys are unaffected.

## Administration

The site rests on two accounts: the Netlify team that hosts it and serves its DNS, and the Porkbun
account that holds the registration of the domain.

### Netlify

The project `teamup-harp` belongs to a Netlify team with one member, its Owner, who alone controls the
deployment, since a second member would take the team off the free plan. Changes to the site go
through pull requests to the repository, so contributors need no Netlify account. Netlify builds
from `TEAMuP-dev/HARP` through its GitHub app, shown under
**Project configuration → Build & deploy → Continuous deployment**, which also sets which branches
get deploys.

Under **Domain management**, `harp.tools` is the primary domain, and `www.harp.tools` redirects
there.

### Domain

`harp.tools` is registered at [Porkbun](https://porkbun.com), in the account of the maintainer who
looks after the site, and renews each year in October. Porkbun was the cheapest registrar for
`.tools` at the time of registration, in October 2026: $9.78 for the first year, then $32.44 a year.
Cloudflare, which sells domains at cost, charged about $28 a year, which would only make up the
difference after five years, and requires its own DNS.

WHOIS privacy hides the contact details of the registrant at no cost, and auto-renew charges the
card of the account. Keep both on, and protect the account with two-factor login.

### DNS and HTTPS

The domain uses Netlify DNS. Its nameservers at Porkbun are the four that Netlify assigned,
`dns1.p08.nsone.net` through `dns4.p08.nsone.net`, and Netlify manages every record, under
**Domains** in the team. It points `harp.tools` and `www.harp.tools` at the project, and issues and
renews the HTTPS certificate itself, with Let's Encrypt. Because DNS lives in Netlify, the
registration can change hands without touching the site.

To keep DNS at the registrar instead, set its nameservers back to its own, and add two records
there: an `ALIAS` record for the apex pointing to `apex-loadbalancer.netlify.com` (or an `A` record
pointing to `75.2.60.5` where `ALIAS` is unavailable), and a `CNAME` record for `www` pointing to
`teamup-harp.netlify.app`. Netlify issues the certificate once the records resolve.

### Renewing and Handing Off

**Renewing.** A domain that expires stops resolving, and once the grace period of the registry ends,
anyone can register it. Keep auto-renew on and keep the card of the Porkbun account current.

**Handing off the domain.** Porkbun moves a domain between its own accounts with a push, which needs
no authorization code and costs nothing:

1. The new maintainer creates a Porkbun account and completes its ID verification. A new account
   cannot receive a domain until it completes this step.
2. The current holder opens **Account → Domain Management**, selects **Details** on `harp.tools`,
   chooses **Push Domain** under **Account Transfer**, and enters the username of the new account.
3. The new maintainer accepts from the email Porkbun sends, within 72 hours, or the push has to
   start again.
4. The new maintainer replaces the registrant contact with their own, and checks auto-renew and the
   card on the account.

The site keeps running throughout, since the nameservers stay those of Netlify. Moving the domain to
another registrar instead takes an authorization code from Porkbun, a few days, and a year of
registration at the price of the new registrar.

**Handing off Netlify is not straightforward.** The team has one member, and a second would take it
off the free plan. The dashboard moves a project, and the DNS zone of its domain, only to a team on
which the person moving it is also a member. Either way, one team has to hold two members for a
moment, so the handoff will most likely mean paying for one month of a team plan.

### Changing the Domain

Should the site move to another domain:

1. Add the new domain under **Domain management**, make it the primary domain, and point its DNS at
   Netlify as above. Wait until it loads over HTTPS.
2. Replace the old address wherever the repository links it: `src/windows/AboutWindow.h` and
   `src/windows/tutorial/TutorialWindow.h`, which are compiled into the app, the root `README.md`,
   `docs/`, the `hostname` of the sitemap in `website/.vitepress/config.mjs`, and the README of
   PyHARP, whose images load from the site. `grep -rn "harp.tools" .` finds them.
3. Add a rule like the first one in `website/public/_redirects`, from the old address to the new, at
   the top, and keep every existing rule. The `!` after its status makes it apply even where a file
   exists at the path, which on this site is every page.
4. Keep the old domain registered for as long as released apps link it.
