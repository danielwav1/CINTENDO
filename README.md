# CINTENDO 1.1

**Your 3DS. Your way.**

CINTENDO is an independent Nintendo 3DS homebrew utility built with devkitPro and libctru.

## Features

- Two-screen homebrew interface with keyboard-style app badges
- SD card file browser with folder navigation and file details
- Homebrew library that lists `.3dsx` files in `sdmc:/3ds`
- Save Vault archive at `sdmc:/3ds/CINTENDO/SaveVault`
- Import a selected regular file into Save Vault with X from its details page
- Save Vault refuses to overwrite an existing archive file
- System dashboard with SD card total, used and free space
- Six persistent theme presets
- Persistent settings for control hints and selected theme
- Advanced Tools page protected by an L+R+X+Y interface toggle
- Developer attribution throughout the app

## Themes

- Standard: red, blue, black and white
- Aesthetic Pink: pink, green, mint and sprinkle accents
- CindraOS: red, black and white
- Midnight: purple and cyan
- Ocean: aqua and blue
- Sunset: amber and pink

## Save Vault and save-game safety

Save Vault stores ordinary backup-file copies in CINTENDO's own folder. Browse to a file on the SD card, open its details and press X to copy it into Save Vault. Existing files are not overwritten.

This is an archive/import feature, not a universal game-save restore tool. 3DS games use different save formats and title-specific save services. Use Checkpoint or JKSM to restore a backup to the correct game. CINTENDO does not overwrite game saves or NAND data.

## Advanced Tools

Hold L + R + X + Y together to toggle the Advanced Tools menu. This is only an interface lock. A normal `.3dsx` homebrew app does not gain root or NAND privileges from a button combination. Advanced Tools therefore exposes safe diagnostics only.

## Controls

- D-Pad: Navigate
- A: Select / open
- B: Back
- X: Copy selected regular file into Save Vault from its details page
- START: Exit
- L + R + X + Y: Toggle Advanced Tools access

## Install

Copy `CINTENDO.3dsx` to:

`sd:/3ds/CINTENDO/CINTENDO.3dsx`

Launch it from the Homebrew Launcher.

## Build

Use a devkitPro devkitARM environment:

```sh
make
```

GitHub Actions builds `CINTENDO.3dsx` and `CINTENDO.smdh` and uploads them as the `CINTENDO-3DS` artifact.

## Credits

Made by **danielwav1**.

https://github.com/danielwav1/CINTENDO
