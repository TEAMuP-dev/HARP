# Installing HARP

Download the package for your platform from the
[latest release](https://github.com/TEAMuP-dev/HARP/releases) on GitHub.

<!--@include: @/../README.md#install-requirements-->

## macOS

1. Download `HARP-<version>-macOS.dmg`.
2. Open the disk image, and drag `HARP.app` onto the `Applications` folder beside it:

   <img src="/content/images/macos-install.png" alt="The HARP disk image, with HARP.app and a link to the Applications folder" style="display:block;margin:1rem auto;max-width:480px;width:100%;">

3. Eject the disk image, then open HARP from `Applications`.

::: warning
Do not run HARP from the disk image. It runs from a read-only volume there, and a DAW set to open
that copy loses it once the disk image is ejected.
:::

## Windows

1. Download `HARP-<version>-Windows-x64.zip`.
2. Extract `HARP.exe`, and move it to a permanent location, _e.g._, `C:\Program Files\HARP\`.
3. Open `HARP.exe`. The first time it runs, Windows shows "Windows protected your PC", since the
   executable is not code signed. Select **More info**, then **Run anyway**.

<!-- TODO: screenshot. The SmartScreen dialog after selecting "More info", with the "Run anyway"
     button visible.
![The SmartScreen dialog, with the Run anyway button shown after selecting More info](/images/windows-smartscreen.png)
-->

## Linux

1. Download the archive for your architecture, `HARP-<version>-Linux-x64.tar.gz` or
   `HARP-<version>-Linux-arm64.tar.gz`.
2. Extract the `HARP` executable, and move it to a directory on your `PATH`:

   ```bash
   tar -xzf HARP-<version>-Linux-x64.tar.gz
   mkdir -p ~/.local/bin
   mv HARP ~/.local/bin/
   ```

3. Run `HARP`.

## First Launch

HARP opens on its Home tab, which lists the available models. The first time, a short tutorial
walks through the interface, highlighting each part as it describes it. `Help > Welcome Tutorial`
shows it again.

<!-- TODO: screenshot. HARP on first launch, with the welcome tutorial open over the Home tab.
![HARP on first launch, with the welcome tutorial open over the Home tab](/images/first-launch-tutorial.png)
-->

Next, [set up HARP](/content/setup/) on its own or with your DAW.

## Updating

To update HARP, install a newer release over the old one. Settings, API keys, and custom model paths
live in a settings file of their own, which `File > Settings > General > Open Settings File`
reveals, and carry over from one version to the next.
