# Thumbboard

> A lightweight, gamepad-driven virtual keyboard daemon for Wayland.

**Status:** pre-alpha — design phase. See [docs/ROADMAP.md](docs/ROADMAP.md).

Thumbboard lets you type on Linux without a real keyboard, using only
the controller already in your hands. It runs as a small daemon on top
of any Wayland compositor that supports `wlr-layer-shell` and
`input_method_v2` — that includes **Hyprland, Sway, river, labwc,
and KDE Plasma 6** — draws an on-screen keyboard with
hardware-accelerated GLES, and forwards keystrokes (US English,
Czech, Russian, German, and any layout you can describe in JSON +
xkb) to the focused application.

Pop the keyboard up with a controller button combo (default
**Start + Select**), no compositor keybind required.

## Why

Typing a Steam search, a URL, or a chat message from the couch on a
Linux HTPC currently means picking up a wireless keyboard or
suffering through console-style D-pad walking with no decent overlay.
Steam's keyboard is gated behind Big Picture mode and only types into
Steam itself. Maliit is built for touchscreens, not gamepads. There is
no good gamepad-native answer on Linux. Thumbboard is the answer.

## Targeted compositors

| Compositor       | Status         | Notes |
|------------------|----------------|-------|
| Hyprland         | primary target | the maintainer's daily driver |
| KDE Plasma 6     | primary target | Plasma 6 ships `wlr-layer-shell`. Plasma 5 is not supported. |
| Sway             | primary target ||
| river            | best-effort    ||
| labwc            | best-effort    ||
| GNOME (Mutter)   | not yet — see [ADR 0005](docs/adr/0005-surface-backend.md) | needs the planned `xdg-toplevel` fallback *and* `input-method-v2` |

## Languages and layouts

v1.0 ships built-in JSON layouts for **US English**, **Czech**
(QWERTZ with `č`, `š`, `ž`, `ů`, `á`…), and **Russian** (Cyrillic
ЙЦУКЕН). Multi-layout users have all alphabets active simultaneously
and switch with a single controller button. Adding a new layout is a
JSON pull request, no recompile. See
[ADR 0006](docs/adr/0006-internationalization.md) for the
internationalization design.

## What's settled, what isn't

The major architectural decisions are recorded as ADRs:

- [ADR 0001 — Wayland protocol: `input_method_v2`](docs/adr/0001-wayland-protocol.md)
- [ADR 0002 — Renderer: OpenGL ES 3.0 over EGL](docs/adr/0002-renderer.md)
- [ADR 0003 — Gamepad input: SDL3](docs/adr/0003-gamepad-library.md)
- [ADR 0004 — Layout: pointer-walk QWERTY](docs/adr/0004-layout-style.md)
- [ADR 0005 — Surface backend abstraction (KDE Plasma 6 support)](docs/adr/0005-surface-backend.md)
- [ADR 0006 — Internationalization (multi-layout xkb + UTF-8 layout JSON)](docs/adr/0006-internationalization.md)

The rest of the design — components, directory layout, threading,
config schema, IPC — lives in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). The visual design and
animation plan are in [docs/GUI.md](docs/GUI.md). Milestones and
acceptance criteria are in [docs/ROADMAP.md](docs/ROADMAP.md).

## Build (will work once M0 lands)

```sh
# Arch
sudo pacman -S meson ninja clang \
    wayland wayland-protocols libxkbcommon \
    sdl3 mesa freetype2 harfbuzz tomlplusplus catch2

# Ubuntu 24.04
sudo apt install meson ninja-build clang-format clang-tidy \
    libwayland-dev wayland-protocols libxkbcommon-dev \
    libsdl3-dev libegl1-mesa-dev libgles2-mesa-dev \
    libfreetype-dev libharfbuzz-dev libtomlplusplus-dev catch2

git clone https://github.com/Topazen/thumbboard.git
cd thumbboard
meson setup build
meson compile -C build
./build/thumbboard
```

## Use (will work once M5 lands)

```sh
# Run the daemon (typically autostarted by your compositor / DE)
thumbboard &
```

Once running, the keyboard is invisible by default. Summon it with
your gamepad — the default combo is **Start + Select** (configurable
in `~/.config/thumbboard/config.toml`). Press the same combo to
dismiss.

If you'd rather bind it to a compositor hotkey instead of a gamepad
combo:

```sh
# Hyprland (~/.config/hypr/hyprland.conf)
bind = SUPER, K, exec, thumbboardctl toggle

# Sway (~/.config/sway/config)
bindsym $mod+k exec thumbboardctl toggle

# KDE Plasma — System Settings → Shortcuts → Custom Shortcuts → New
#   Trigger: Meta+K
#   Action:  thumbboardctl toggle
```

## License

GNU General Public License v3.0. See [LICENSE](LICENSE).
