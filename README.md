# CINTENDO 1.0

**Your 3DS. Your way.**

CINTENDO is a homebrew application for the Nintendo 3DS family, built with devkitPro and libctru.

## Features

- Home menu with D-pad navigation
- Applications page listing entries in `sdmc:/3ds`
- Read-only SD card root directory viewer
- Basic system and environment information
- Settings page with a control-hints toggle
- About page with developer attribution

The Applications page currently lists homebrew files only. It does not launch them. The SD card browser is read-only and does not modify files.

## Install

Copy `CINTENDO.3dsx` to:

`sd:/3ds/CINTENDO/CINTENDO.3dsx`

Then launch CINTENDO from the Homebrew Launcher.

## Build

Build in a devkitPro devkitARM environment:

```sh
make
```

GitHub Actions builds the application and uploads the `.3dsx` and `.smdh` files as an artifact.

## Credits

Made by **danielwav1**.

Repository: https://github.com/danielwav1/CINTENDO
