# CINTENDO Plugin Format v1

## Security model

CINTENDO plugins are declarative metadata files. They are not native executables, shell scripts, bytecode, or arbitrary C code. The interpreter accepts only action identifiers compiled into CINTENDO. A file on the SD card cannot add a new native action.

The current implementation uses two matching files to preserve the existing key/unlock design:

- A key file: `sdmc:/3ds/CINTENDO/PluginKeys/<name>.cplug`
- A script metadata file: `sdmc:/3ds/CINTENDO/Plugins/<id>.cscript`

The repository keeps starter files in `plugins/keys/` and `plugins/scripts/`.

## Key file

A key file uses UTF-8-compatible plain ASCII metadata:

```ini
CINTENDO_KEY_V1
id=theme-next
name=Theme Next
```

The required identifier contains 1–40 lowercase ASCII letters, digits, or hyphens. The current parser uses the identifier to locate the matching script file. The key's name is informational.

## Script metadata file

```ini
CINTENDO_SCRIPT_V1
id=theme-next
name=Theme Next
description=Switch to the next CINTENDO theme preset.
action=THEME_NEXT
```

Required fields are `id`, `name`, `description`, and `action`. The key ID and script ID must match exactly.

## Limits and validation

- Each metadata file must be a regular file.
- Each metadata file must be between 1 and 2048 bytes.
- Lines must fit the parser's bounded line buffers.
- IDs are limited to 40 characters and the documented character set.
- The plugin name, description, and action are copied into fixed-size destination buffers.
- Missing, malformed, oversized, or mismatched files are ignored by the current scanner.
- At most `CINTENDO_PLUGIN_LIMIT` plugins are loaded.
- Enabled state is stored as a small `.active` marker beside the key file.

The current UI does not yet display a diagnostic entry for every rejected plugin; rejected pairs are omitted from the visible list.

## Built-in actions

The interpreter currently supports only these compiled-in actions:

- `THEME_NEXT`
- `THEME_PREVIOUS`
- `THEME_RANDOM`
- `SHOW_ABOUT`

Unknown actions are rejected. Adding an action requires implementing and compiling its handler in `source/plugin_engine.c`.

## Adding a plugin

1. Choose a unique ID using lowercase letters, digits, and hyphens.
2. Add a matching `.cplug` key file and `.cscript` metadata file.
3. Use an action already supported by the interpreter.
4. Copy the key into `PluginKeys/` and the matching script into `Plugins/`.
5. Open CINTENDO → Advanced Tools → Plugin Key Center.

## Compatibility note

This is the existing v1 format, not a general-purpose plugin ABI. Fields such as `api_version`, `author`, and `unlocks` are not currently parsed. A future format may consolidate the metadata and define explicit API-version negotiation, but it must retain the non-executable allowlist security model.
