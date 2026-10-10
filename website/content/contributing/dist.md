# Distribution

HARP is built and packaged for macOS (universal), Windows (x64), and Linux (x64 and arm64) by [GitHub Actions](https://github.com/TEAMuP-dev/HARP/blob/main/.github/workflows/build.yml) on every push.
Pushing a tag of the form `v*` also publishes the packages as a release, with `RELEASE.md` as its notes.
The macOS and Windows packages are code signed, as described in [SIGNING.md](https://github.com/TEAMuP-dev/HARP/blob/main/docs/SIGNING.md).

### MacOS

A build can also be packaged locally into a DMG with [dmgbuild](https://github.com/dmgbuild/dmgbuild) (`pipx install dmgbuild`):
```bash
packaging/package.sh build/HARP_artefacts/Release/HARP.app HARP.dmg
```

To sign and notarize the DMG, set the following environment variables first:
```bash
export MACOS_SIGNING_IDENTITY="Developer ID Application: <NAME> (<TEAM_ID>)" # Certificate in your keychain
export APPLE_ID=<APPLE_ID>                       # Apple Account email address
export APPLE_APP_PASSWORD=<APP_SPECIFIC_PASSWORD> # App-specific password for notarization
```
