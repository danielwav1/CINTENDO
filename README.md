# CINTENDO 1.5

**Your 3DS. Your way.**

CINTENDO is a two-screen graphical homebrew utility for the Nintendo 3DS family, built with devkitPro, devkitARM, libctru, Citro3D, and Citro2D. The project is designed to remain usable on original 2DS / Old 3DS hardware.

**Developer:** danielwav1
**Repository:** https://github.com/danielwav1/CINTENDO

## Current features

- Two-screen graphical launcher with nine application tiles.
- SD-card file browser.
- Recursive .3dsx discovery below sdmc:/3ds/.
- File details and copy-to-Save-Vault function.
- Save Vault as an ordinary SD-card backup archive.
- Six built-in theme presets.
- Persistent control-hint, dark-mode, and theme settings.
- SD storage dashboard.
- PCM WAV music playback (8-bit or 16-bit, mono or stereo; maximum 8 MB per file).
- Advanced Tools protected by the L + R + X + Y unlock sequence.
- Local plugin key center with a compiled-in action allowlist.
- Safe fallback behavior for missing or invalid configuration files.

## Important limitations

- The Homebrew Library currently scans and displays .3dsx files; it does not chain-load them. Start homebrew applications through the Homebrew Launcher.
- The Save Vault stores ordinary file copies. It does not restore game saves. Use an appropriate save manager for game-specific restore operations.
- Plugins are declarative metadata files, not arbitrary executable code. Only actions compiled into CINTENDO can run.
- The plugin scanner currently ignores malformed or mismatched plugin pairs; it does not yet show a detailed rejection reason for each invalid file.
- The project is not yet split into independent module source files. Most UI and application logic remains in source/main.c.
- The reported black-line issue has not been confirmed fixed on physical Old 2DS hardware.

## Controls

### Home launcher
- D-pad: move between tiles.
- A: open the selected page.
- START: exit CINTENDO.

### File browser and Homebrew Library
- Up / Down: select an entry.
- A: open a folder or show file details.
- B: go back.

### Themes and settings
- Up / Down: select.
- A: apply or toggle.
- B: return.

### Advanced Tools
Hold L + R + X + Y to unlock Advanced Tools. This is an interface lock only; it does not grant elevated privileges.
- Open the Advanced tile.
- Select Plugin Center or Reset.
- Reset requires two Y confirmations and can be cancelled with B.

### Music
- A: play selected WAV.
- X: pause / resume.
- Y: stop.
- B: return home.

## SD-card layout

CINTENDO creates and uses these directories:

    sdmc:/
    └── 3ds/
        └── CINTENDO/
            ├── CINTENDO.3dsx
            ├── CINTENDO.smdh
            ├── config.ini
            ├── Music/
            ├── Plugins/
            ├── PluginKeys/
            └── SaveVault/

The plugin source files in this repository are copied into Plugins/ and PluginKeys/ by the install-package workflow.

## Plugin system

The current plugin system uses two matching files:
- Key: PluginKeys/<name>.cplug
- Script metadata: Plugins/<id>.cscript

The key and script IDs must match. Scripts contain an action identifier checked against a fixed allowlist compiled into CINTENDO. Arbitrary native code, shell commands, or scripts from the SD card are never executed.

See docs/PLUGIN_SPEC.md for the exact v1 format and limits.

## Installation

1. Download the CINTENDO-Install artifact from a successful GitHub Actions build.
2. Extract the package.
3. Copy the included 3ds folder to the root of the SD card and merge folders if prompted.
4. Open the Homebrew Launcher and start CINTENDO.3dsx.

Installed application path: sdmc:/3ds/CINTENDO/CINTENDO.3dsx

## Build from source

Use a devkitPro environment with devkitARM, libctru, Citro3D, and Citro2D installed.

    make clean
    make

The build produces CINTENDO.3dsx and CINTENDO.smdh.

GitHub Actions uses the devkitpro/devkitarm container and uploads an installable SD-card package as the CINTENDO-Install artifact.

## Old 2DS compatibility

The renderer uses standard non-wide logical screen targets:
- Top: 400 × 240
- Bottom: 320 × 240

The Old 2DS does not support the New 3DS 800-pixel-wide top-screen mode. CINTENDO does not enable New-3DS-only CPU or cache features.

A successful CI build does not replace testing on a real Old 2DS. See CHANGELOG.md for the current known limitations and fixes.

## Credits

Made by **danielwav1**.

CINTENDO is an independent community homebrew project and is not affiliated with Nintendo.