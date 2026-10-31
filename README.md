# CINTENDO 1.0

**Your 3DS. Your way.**

CINTENDO is a Nintendo 3DS homebrew utility built with devkitPro and libctru.

## Features

- D-pad controlled home menu
- SD card file manager with folder navigation
- File details including type and size
- Homebrew library that filters and lists `.3dsx` files in `sdmc:/3ds`
- System dashboard with SD card capacity, used space and free space
- Settings for control hints and accent color
- Settings are saved to `sdmc:/3ds/CINTENDO/config.ini`
- Two-screen interface and developer credits

## Current limitations

The Homebrew Library discovers and displays `.3dsx` files but does not launch them. The file manager is currently read-only: it does not copy, move, rename or delete files. System information is limited to information safely available to this homebrew application.

## Install

Copy `CINTENDO.3dsx` to:

`sd:/3ds/CINTENDO/CINTENDO.3dsx`

Launch it from the Homebrew Launcher.

## Controls

- D-Pad: Navigate
- A: Open or select
- B: Go back
- START: Exit

## Build

Use a devkitPro devkitARM environment:

```sh
make
```

GitHub Actions builds `CINTENDO.3dsx` and `CINTENDO.smdh` and uploads them as the `CINTENDO-3DS` artifact.

## Credits

Made by **danielwav1**.

https://github.com/danielwav1/CINTENDO
