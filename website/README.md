# HARP-WEBSITE

This website is built using [Vitepress](https://vitepress.dev/).

We use [Netlify](https://www.netlify.com/) to deploy it under the domain [harp-plugin.netlify.app](https://harp-plugin.netlify.app/).

## Build locally

### Install dependencies

1.  Install [Node.js](https://nodejs.org/en/download/)
    *   On Mac, if you prefer you can use [Homebrew](https://brew.sh/) to install Node.js:
        `brew install node`
2.  Install [pnpm](https://pnpm.io/): `npm install -g pnpm`
3.  Navigate to the `website` directory and install dependencies:
    ```bash
    cd website
    pnpm install
    ```

### Run the development server

To run the development server with hot-reloading, run the following commands in the `website` directory:

```bash
pnpm run build
pnpm run docs:dev
```

The website should then be available at http://localhost:5173 (or the next available port). Every time you save a relevant file, the website will automatically refresh in your browser.

**Important Note on `prebuild`:**
When you run `pnpm run build`, a script named `prebuild` is automatically executed first.

**What `pnpm run prebuild` does for this website:**
The `prebuild` script, defined in `package.json`, is responsible for copying Markdown (`.md`) files from the main `HARP` repository and the `pyHARP` submodule into the website's content directory. Specifically:
*   It copies all `.md` files from the root of the `HARP` repository (e.g., `../README.md`) into the `website/content/HARP/` directory.
*   It copies all `.md` files from the `pyHARP` submodule (e.g., `../pyharp/README.md`) into the `website/content/pyHARP/` directory.
This ensures that the documentation displayed on the website is sourced directly from the repositories, maintaining a single source of truth.

### Edit the content

The website's content is primarily managed through Markdown files and VitePress configuration:

*   **Source Markdown Files:**
    *   Thanks to the `prebuild` script, Markdown files from the root of the `HARP` repository are automatically available under `website/content/HARP/`.
    *   Similarly, Markdown files from the `pyHARP` submodule are available under `website/content/pyHARP/`.
    *   You should edit these original files in their respective repositories (`HARP/*.md` or `HARP/pyHARP/*.md`) when you want to update the documentation. The changes will be reflected on the website after the next build.
*   **Additional Website-Specific Content:**
    *   You can create new Markdown files directly within `website/content/` or its subdirectories (e.g., `website/content/new-page.md`) for content that is specific to the website and doesn't belong in the main repositories.
*   **Site Structure and Navigation:**
    *   The navigation bar, sidebar, footers, and other site-wide configurations are managed in the `website/.vitepress/config.mjs` file. You edit this file to define how your Markdown files are organized and presented in the site's navigation.
*   **Homepage:**
    *   You can modify the contents of the homepage in `website/index.md`.
*   **Images:**
    *   If you want to add images, you can put them in `website/public/images` (or any subdirectory within `public`) and reference them in your markdown files using a root-relative path, e.g.,
    ```markdown
    ![image](/images/my-image.png)
    ```
    Alternatively, for images specific to content pages, you can place them alongside your markdown files in `website/content/` and use relative paths.

## Deploy to Netlify

*   **Production Deployment:** The `main` branch of the `HARP` repository is automatically deployed to our live URL: [harp-plugin.netlify.app](https://harp-plugin.netlify.app/).
*   **Deployment Previews:** Netlify automatically generates deployment previews for:
    *   All Pull Requests (PRs) made against the `main` branch.
    *   Any branch whose name starts with the prefix `website/*` (e.g., `website/new-feature`, `website/docs-update`).

This allows for easy review of changes before they go live on the production site.