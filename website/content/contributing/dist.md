# Distribution

HARP is built and packaged for macOS (universal), Windows (x64), and Linux (x64 and arm64) by [GitHub Actions](https://github.com/TEAMuP-dev/HARP/blob/main/.github/workflows/build.yml) on every push.
Pushing a tag of the form `v*` also publishes the packages as a release, with `RELEASE.md` as its notes.
The macOS and Windows packages are code signed, as described in [SIGNING.md](https://github.com/TEAMuP-dev/HARP/blob/main/docs/SIGNING.md).

### MacOS

A build can also be packaged locally into a DMG with [dmgbuild](https://github.com/dmgbuild/dmgbuild) (`pip install dmgbuild`):
```bash
packaging/package.sh build/HARP_artefacts/Release/HARP.app HARP.dmg
```

To sign and notarize the DMG, set the following environment variables first:
```bash
export MACOS_SIGNING_IDENTITY="Developer ID Application: <NAME> (<TEAM_ID>)" # Certificate in your keychain
export APPLE_ID=<APPLE_ID>                       # Apple Account email address
export APPLE_APP_PASSWORD=<APP_SPECIFIC_PASSWORD> # App-specific password for notarization
```

### Linux

A Linux binary requires at least the versions of glibc and libstdc++ used to build it, so the workflow builds on the oldest Ubuntu available, 22.04 (glibc 2.35, and libstdc++ from GCC 12).
The packages therefore run on these distributions:

- Ubuntu 22.04 or newer, and distributions based on it, such as Linux Mint 21 and Pop!_OS 22.04
- Debian 12 or newer, including 64-bit Raspberry Pi OS (with the `arm64` package)
- Fedora 36 or newer, RHEL 10 and its rebuilds, openSUSE Tumbleweed and Leap 16, and Arch Linux

They do not run on Ubuntu 20.04, Debian 11, RHEL 8 or 9, or openSUSE Leap 15.
Reaching those would mean building inside an older Linux container, and reaching every distribution would mean publishing a Flatpak.

At runtime, HARP needs ALSA, FreeType, and X11, and loads libcurl for network requests, all of which come with a standard desktop installation.
On Wayland, it runs through XWayland, which GNOME and KDE enable by default.
