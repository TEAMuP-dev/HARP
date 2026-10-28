// The downloads of the install page, built from the version that the root VERSION file names.
//
// VitePress runs this when it builds the site. For a tag v<VERSION>, the release workflow
// (.github/workflows/build.yml) attaches the packages to the GitHub release of that tag, named
// HARP-<VERSION>-<platform>, and these links lead to them once it has run. The repository holds
// no copy of the packages.

import { readFileSync } from 'node:fs'

const repository = 'https://github.com/TEAMuP-dev/HARP'

export default {
  load() {
    const version = readFileSync(new URL('../../../VERSION', import.meta.url), 'utf-8').trim()

    const asset = (tag, name) => ({ name, url: `${repository}/releases/download/${tag}/${name}` })
    const harpPackage = (platform, extension) =>
      asset(`v${version}`, `HARP-${version}-${platform}.${extension}`)

    return {
      version,
      notes: `${repository}/releases/tag/v${version}`,
      macOS: harpPackage('macOS', 'dmg'),
      windows: harpPackage('Windows-x64', 'zip'),
      linuxX64: harpPackage('Linux-x64', 'tar.gz'),
      linuxArm64: harpPackage('Linux-arm64', 'tar.gz'),
      // The demo files do not change with each version, so they stay with the release that has them
      demoFiles: asset('v4.0.0', 'HARP-demo-files.zip'),
    }
  },
}
