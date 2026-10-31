# CINTENDO 1.5

**Your 3DS. Your way.**

CINTENDO is an independent Nintendo 3DS homebrew utility built with devkitPro and libctru.

## Features

- Unified two-screen graphical launcher UI with selectable app tiles, icon badges, cards and matching control panels
- Graphical file manager, homebrew library, Save Vault, themes, settings, diagnostics, plugin center and music player
- Two-screen SD file browser
- CINTENDO Music: local PCM WAV player with play, pause, resume and stop controls
- Persistent Dark Mode enabled by default, with a Light Mode option
- Homebrew library, Save Vault, storage dashboard and six themes
- Advanced Tools protected by the L+R+X+Y interface toggle
- Modular SD-card plugin key and script system
- Made by danielwav1

## Launcher interface

All launcher pages now use the same Citro2D graphical layout rather than switching between the graphical home screen and terminal-style console pages. The home dashboard contains nine selectable cards for Files, Homebrew, Save Vault, Themes, System, Settings, Plugins, About and Music. Other pages use matching cards, status panels and on-screen control hints.

The current tile icons are drawn from built-in vector-style shapes and short labels, so the app does not depend on external icon packs or downloaded artwork.



The Homebrew Library currently scans and displays `.3dsx` files from `sd:/3ds/`. Selecting an entry opens its details; launching another `.3dsx` directly from CINTENDO is not implemented yet and remains a separate development task.

## Plugin system

CINTENDO uses two separate files for each plugin:

- **Key**: a `.cplug` file in `sd:/3ds/CINTENDO/PluginKeys/`. It unlocks a feature.
- **Plugin code**: a matching `.cscript` file in `sd:/3ds/CINTENDO/Plugins/`. It contains the script metadata and an action for CINTENDO's built-in interpreter.

The key ID and script ID must match. CINTENDO scans both folders at startup and whenever the Plugin Key Center is opened. A key without a matching valid script is ignored. Removing the key removes the plugin from the list. Removing the script makes the key unusable.

### Included starter plugins

| Plugin | Action |
|---|---|
| Theme Next | Switch to the next CINTENDO theme |
| Random Theme | Select a random CINTENDO theme |
| Theme Previous | Switch to the previous CINTENDO theme |
| Plugin Engine Info | Display script engine information |

The source files are in `plugins/keys/` and `plugins/scripts/`. Copy the chosen key and its matching script to the corresponding SD folders. Open **Advanced Tools → Plugin Key Center**. Select a plugin and press **A** to enable or disable it; press **X** to open its details, then **X** to execute it.

### Script format

Keys use this format:

```ini
CINTENDO_KEY_V1
id=theme-next
name=Theme Next
```

Scripts use this format:

```ini
CINTENDO_SCRIPT_V1
id=theme-next
name=Theme Next
description=Switch to the next CINTENDO theme preset.
action=THEME_NEXT
```

The script interpreter intentionally supports a fixed allowlist of actions. It does not execute arbitrary C, native binaries, shell commands, or code from the SD card. To add a new action, its handler must first be implemented in the CINTENDO source and compiled into the app. Plugin keys are local feature-unlock files, not cryptographic DRM.

### Adding a plugin

1. Add a unique key file under `plugins/keys/`.
2. Add a matching script under `plugins/scripts/`.
3. Implement and allowlist its action in `source/plugin_engine.c`.
4. Build CINTENDO and copy both files to the matching SD folders.

## CINTENDO Music

Copy compatible uncompressed PCM WAV files into `sd:/3ds/CINTENDO/Music/`. Supports 8-bit or 16-bit PCM, mono or stereo, up to 8 MB per file. **A** plays, **X** pauses/resumes, **Y** stops, and **B** returns home. MP3, FLAC, playlists, shuffle, repeat and background playback are not implemented.

## Advanced Tools

Hold **L + R + X + Y** together to unlock Advanced Tools. The interface lock does not grant root, NAND, or elevated system privileges.

## Save Vault safety

Save Vault stores ordinary backup-file copies. It does not overwrite game saves or NAND data. Use Checkpoint or JKSM to restore backups to the correct game.

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
