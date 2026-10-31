# CINTENDO 1.2

**Your 3DS. Your way.**

CINTENDO is an independent Nintendo 3DS homebrew utility built with devkitPro and libctru.

## Features

- Two-screen homebrew interface and SD file browser
- Homebrew library listing `.3dsx` files
- Save Vault archive that preserves existing backup files
- SD storage dashboard and six themes
- Advanced Tools protected by the L+R+X+Y interface toggle
- Plugin Center with SD-card package installation and ten included utility plugins
- Reset CINTENDO option for app preferences and installed plugins
- Made by danielwav1

## Advanced Tools

Hold **L + R + X + Y** together to unlock Advanced Tools.

- **A — Plugin Center:** browse and run installed utility plugins
- **Y — Reset CINTENDO:** reset the theme and hints, remove installed plugins, and restore default settings
- Reset confirmation uses **Y** to confirm and **B** to cancel

Reset deliberately preserves the **Save Vault** backup folder and the plugin package inbox. It does not delete personal backup files.

## Included plugins

| Plugin | Function |
|---|---|
| Battery Monitor | Reads the current battery percentage |
| System Clock | Shows a system tick counter for diagnostics |
| Storage Inspector | Shows free SD-card space |
| Save Vault Stats | Counts archived backup files |
| File Type Guide | Explains file types recognized by the browser |
| Config Inspector | Shows the current settings and config path |
| Theme Inspector | Shows the active theme and preset count |
| 3DS Folder Counter | Counts entries in `sdmc:/3ds` |
| SD Health Check | Checks that CINTENDO folders exist |
| Plugin API Info | Shows the plugin API version and credits |

## Installing plugins from the SD card

1. Copy one or more `.cplug` files from this repository's `plugins/available/` folder.
2. Put them on the SD card in:
   `sd:/3ds/CINTENDO/PluginInstall/`
3. Start CINTENDO and unlock Advanced Tools with **L + R + X + Y**.
4. Open **Plugin Center** and press **Y** to install the packages.
5. Select an installed plugin and press **A** to run it.

Installed packages are stored in `sd:/3ds/CINTENDO/Plugins/`.

A `.cplug` package is a plain-text manifest, not executable native code. CINTENDO accepts only the ten known plugin IDs compiled into the app. This prevents an SD-card package from running arbitrary code. The plugin packages provide applet-style tools through CINTENDO's built-in plugin handlers.

## Save Vault safety

Save Vault stores ordinary backup-file copies in CINTENDO's own folder. It is not a universal game-save restore tool. Use Checkpoint or JKSM to restore a backup to the correct game. CINTENDO does not overwrite game saves or NAND data.

## Important limitations

Advanced Tools is an interface lock only. A normal `.3dsx` homebrew app does not gain root, NAND, or elevated system privileges from a button combination.

## Controls

- D-Pad: Navigate
- A: Select / open
- B: Back
- X: Copy a selected regular file into Save Vault from its details page
- Y: Install plugins in Plugin Center / confirm reset
- START: Exit
- L + R + X + Y: Toggle Advanced Tools access

## Install

Copy `CINTENDO.3dsx` to `sd:/3ds/CINTENDO/CINTENDO.3dsx` and launch it from the Homebrew Launcher.

## Build

Use a devkitPro devkitARM environment:

```sh
make
```

GitHub Actions builds `CINTENDO.3dsx` and `CINTENDO.smdh` and uploads them as the `CINTENDO-3DS` artifact.

## Credits

Made by **danielwav1**.

https://github.com/danielwav1/CINTENDO
