# Supported Operating Systems

Every release of HARP is built for each platform below.

| Platform | Package | Requires |
| :-- | :-- | :-- |
| macOS | Universal binary, signed and notarized | macOS 11 or newer, on Apple silicon or Intel |
| Windows | x64 executable | Windows 10 or 11 |
| Linux | x64 and arm64 archives | glibc 2.35 or newer |

The Linux packages are built on the oldest supported Ubuntu, which sets that glibc requirement. They
run on Ubuntu 22.04, Debian 12, Fedora 36, RHEL 10, openSUSE Leap 16, and Arch, or anything newer.
At runtime HARP needs ALSA, FreeType, and X11, and loads libcurl for network requests, all of
which a desktop installation provides. Under Wayland it runs through XWayland, which GNOME and
KDE enable by default.
