# HARP-WEBSITE

This website is built using [Vitepress](https://vitepress.dev/).

We use [Netlify](https://www.netlify.com/) to deploy it under the domain [harp-plugin.netlify.app](https://harp-plugin.netlify.app/).

## Build locally

### Install dependencies

1. Install [Node.js](https://nodejs.org/en/download/)
    - On Mac, if you prefer you can use [Homebrew](https://brew.sh/) to install Node.js:
    `brew install node`
2. Install [pnpm](https://pnpm.io/): `npm install -g pnpm`
3. Navigate to the `website` directory and install dependencies:
    ```bash
    cd website
    pnpm install
    ```
### Run the development server

To run the development server, run the following command in the `website` directory:

  ```bash
  pnpm run docs:dev
  ```

The website should be available at http://localhost:5173

Every time you save a file, the website will automatically refresh in your browser.


### Edit the content

- The content of the website is located in `website/content`.
- You can create and edit the markdown files in this directory to add or modify the content of the website.
- You can also edit the `website/.vitepress/config.mjs` file to modify the navigation bar, the sidebar, the footer, etc.
- You can modify the contents of the homepage in `website/index.md`.
- If you want to add images, you can put them in `website/content/images` and reference them in your markdown files using the relative path e.g
```markdown
![image](./images/my-image.png)
```

## Deploy to Netlify

Push the changes you made to the `main` branch.
The website is automatically deployed when changes are pushed to the `main` .
