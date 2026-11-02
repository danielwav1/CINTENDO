# Changelog

## Old-2DS stability branch

### BUG-006 — navigation and module integration
- The Home launcher now labels the Advanced Tools tile accurately.
- Advanced Tools is gated by the existing L + R + X + Y unlock sequence.
- The Advanced Tools menu now distinguishes Plugin Center from Reset.
- Reset requires two Y confirmations and can be cancelled with B.
- Plugin enable/disable feedback now reports the resulting state correctly.
- The broader module split into independent init/update/draw/exit modules remains outstanding; the current application is still centered in `source/main.c`.

### BUG-005 — settings persistence
- Settings loading now validates all three stored values and rejects trailing non-whitespace data.
- Invalid configuration is moved to `config.ini.corrupt` and defaults are loaded.
- Settings are written to a temporary file and renamed into place only after a successful write and close.

### BUG-004 — plugin input validation
- Plugin metadata files must be regular files with a size from 1 to 2048 bytes.
- Overlong lines are rejected rather than silently parsed in fragments.
- Null plugin arrays and non-positive scan capacities are rejected.
- The existing key-plus-script format and built-in action allowlist are retained.
- The UI still omits invalid plugin pairs rather than showing a per-file rejection reason.

### BUG-001 / BUG-001b — renderer and text safety
- 3D output is explicitly disabled for the 2DS-oriented renderer.
- Citro3D, Citro2D, render-target, and text-buffer initialization failures are checked.
- The text drawing helper now safely ignores null buffers and null/empty strings.
- The existing text buffer is cleared once before each graphical page render.
- No raw framebuffer, pitch, pixel-format, or rotation code exists in this project; rendering is performed through Citro2D screen targets at the standard 400×240 top-screen and 320×240 bottom-screen logical resolutions.
- The exact cause of the previously reported black lines has not been isolated from source inspection alone. A physical Old 2DS test is still required.

### BUG-002 — .3dsx launching
- Direct .3dsx chain-loading is not implemented.
- `envSetNextLoad()` is a Nintendo Switch/libnx API, not a Nintendo 3DS/libctru API. The 3DS APT interface exposes title-ID chainloader functions, not a path-based `.3dsx` loader.
- The Homebrew Library remains a scanner/details view. It must not claim to launch apps until a real 3DSX loader integration is available and tested.

### BUG-003 — Old-2DS compatibility
- The current renderer uses standard non-wide 400×240 top and 320×240 bottom targets and does not request New-3DS clock or L2-cache features.
- Audio and renderer allocations now have initialization/allocation checks where used.
- Explicit APT hardware reporting and a heap/linear-memory debug overlay remain outstanding.

## Verification status

A successful GitHub Actions build and a real Old 2DS hardware test are required before considering this branch stable. A successful compile does not prove that the black-line rendering issue is fixed.
