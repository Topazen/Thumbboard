# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Reading Order & Context

Before working on any task:
1. Read `README.md` or `docs/ARCHITECTURE.md` strictly for high-level context.
2. Ignore secondary `.md` files unless explicitly asked.
3. Always analyze project structure and existing code before creating/modifying files.

## CLI and Terminal Rules

**DO:**
- Use auto-confirm flags (`-y`, `--yes`) for all interactive operations
- Run persistent servers/watchers in the background with `&`
- Truncate large outputs using `head`, `tail`, or `grep` — never flood the terminal

**DO NOT:**
- Use interactive commands (`vim`, `nano`, `htop`, `less`, etc.)
- Ask the user to manually input during script execution
- Show untruncated output from large files or commands

## Strict Search Limits

When using `grep`, `find`, or searching the codebase:
- **Explicitly exclude compiled folders, binaries, and dependencies:** `.git/`, `build/`, `dist/`, `node_modules/`, `.log`, and other artifacts
- **Never `cat` or output files larger than 200 lines.** Use `grep`, read in chunks with `head`/`tail`/`offset`, or use the `Read` tool with `limit` and `offset` parameters
- Example: `grep -r "pattern" src/ --exclude-dir=.git --exclude-dir=build` or `find src/ -name "*.cpp" -not -path "*/build/*"`

## Code Output Rules

- Provide **surgical edits only** — do NOT output unmodified file sections
- When a command fails, **analyze the error** and adjust approach — do NOT blindly retry the exact same command
- Use the `Edit` tool for targeted changes; use `Write` only for new files or complete rewrites

## Project Overview

**Thumbboard** is a lightweight, gamepad-driven virtual keyboard daemon for Wayland compositors (Hyprland, Sway, KDE Plasma 6, river, labwc). It lets users type on Linux without a physical keyboard using only a gamepad controller. The project is currently in **pre-alpha** (design phase); M0 is the first implementation milestone.

**Key facts:**
- Language: C++20
- Build system: Meson + Ninja
- Rendering: OpenGL ES 3.0 via EGL
- Input: SDL3 GameController API
- Wayland protocol: `input_method_v2` and `wlr-layer-shell`
- Status: Design phase. See [docs/ROADMAP.md](docs/ROADMAP.md) for milestones.

## Build Commands

```bash
# First-time setup
meson setup build

# Compile
meson compile -C build

# Run the daemon (will fail on pre-M0 builds — expected)
./build/thumbboard

# Run tests (if enabled)
meson test -C build

# Run tests with verbose output
meson test -C build -v

# Clean build artifacts
rm -rf build
```

### Build Options

Configure with `meson setup build -Doption=value`:

- `-Dtests=true` (default): Build unit and integration test binaries
- `-Dsanitizers=address` or `address+undefined`: Enable runtime sanitizers (debug only)
- `-Dlogging=spdlog`: Use spdlog backend instead of stderr (default: stderr)

Example: `meson setup build -Dtests=true -Dsanitizers=address`

## Code Style and Formatting

The codebase follows LLVM style (C++20, 100-column limit, 4-space indents). Use `clang-format` to format all changes:

```bash
clang-format -i $(git ls-files '*.cpp' '*.hpp' '*.h')
```

Code style is enforced in `.clang-format` and linted with `.clang-tidy`. Both are checked in CI.

## Architecture at a Glance

Thumbboard follows a single-threaded, event-driven architecture. One main event loop polls three file descriptors:
1. Wayland display server socket (compositor events)
2. SDL gamepad input (via evdev)
3. Frame-pacing timer (60/120 Hz render cadence)

All state is owned by the `Daemon` class. Major subsystems:

| Module | Purpose |
|--------|---------|
| `app/` | Process glue: `Daemon` (event loop), `Config` (TOML parsing), `Logger`, `Ipc` (Unix socket for `thumbboardctl`) |
| `core/` | Pure logic (no I/O): `KeyboardState`, `Layout`, `Key`, `Theme`. Fully testable without Wayland or gamepad. |
| `input/` | Gamepad → intent: `GamepadManager` (SDL3), `InputMapper` (event translation), `CursorController` (smoothing). |
| `wayland/` | Compositor integration: `WaylandClient`, `LayerSurface`, `VirtualKeyboard`, `XkbContext`. |
| `render/` | Pixels: `EglContext`, `Renderer`, `ShaderProgram`, `Mesh`, `FontAtlas`, `AnimationSystem`. |

**Read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full breakdown**, including threading model, error handling philosophy, and configuration schema.

## Directory Layout

```
thumbboard/
├── docs/
│   ├── ARCHITECTURE.md        ← How Thumbboard is built; single source of truth
│   ├── GUI.md                 ← Visual design and animations
│   ├── ROADMAP.md             ← M0 → M7 milestones and blockers
│   └── adr/                   ← Architecture Decision Records (why we chose each major dependency)
├── src/
│   ├── app/                   ← Daemon, config, logging, IPC
│   ├── core/                  ← Pure logic (testable without Wayland)
│   ├── input/                 ← Gamepad input handling
│   ├── wayland/               ← Wayland protocol integration
│   └── render/                ← OpenGL ES rendering
├── data/
│   ├── layouts/               ← Keyboard layouts (JSON: US, CZ, RU)
│   ├── themes/                ← Color schemes and animation curves (JSON)
│   └── shaders/               ← Vertex and fragment shaders
├── tests/
│   ├── unit/                  ← Catch2 tests for core/ and input/
│   └── integration/           ← Screenshot and keysym tests against headless Sway
├── protocols/                 ← Wayland protocol XML (layer-shell, input-method-v2)
└── meson.build, meson_options.txt, .clang-format, .clang-tidy
```

## Dependencies

### Required System Packages

**Arch:**
```bash
sudo pacman -S meson ninja clang \
    wayland wayland-protocols libxkbcommon \
    sdl3 mesa freetype2 harfbuzz tomlplusplus catch2
```

**Ubuntu 24.04:**
```bash
sudo apt install meson ninja-build clang-format clang-tidy \
    libwayland-dev wayland-protocols libxkbcommon-dev \
    libsdl3-dev libegl1-mesa-dev libgles2-mesa-dev \
    libfreetype-dev libharfbuzz-dev libtomlplusplus-dev catch2
```

### Header-Only / Optional
- `tomlplusplus`: TOML config parsing (header-only, auto-vendored by Meson)
- `spdlog`: Structured logging (optional; use `-Dlogging=spdlog` to enable)

## Key Documentation Files

**Start here:**
- [README.md](README.md) — Quick overview, build instructions, targeted compositors
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — **The architecture bible.** Kept in sync with code. Explains every subsystem, the event loop, threading model, config schema, IPC design.

**Decision history:**
- [docs/adr/](docs/adr/) — Architecture Decision Records: why Wayland, why OpenGL ES, why SDL3, why multi-layout JSON + xkb, etc.

**Project roadmap:**
- [docs/ROADMAP.md](docs/ROADMAP.md) — Milestones M0 (first working build) through M7. Blockers, acceptance criteria, current bottlenecks.

**Visual design:**
- [docs/GUI.md](docs/GUI.md) — On-screen layout, key shapes, animations, color palette.

## Testing

**Unit tests** (core/ and input/ logic, no I/O):
```bash
meson test -C build -v
```

**Test coverage goal:** >80% for `core/`. Tests are written with **Catch2**.

**Integration tests** (planned M4): Screenshot and keysym assertions against a headless Sway instance in CI.

## Configuration

User config lives at `~/.config/thumbboard/config.toml`. See [docs/ARCHITECTURE.md § 7](docs/ARCHITECTURE.md) for the full schema. Key sections:

```toml
[general]
theme = "Default"
auto_show_on_focus = false  # v2.0 feature

[i18n]
layouts = ["us", "cz", "ru"]
default = "us"

[geometry]
height_pct = 40
margin_px = 24

[input]
gamepad_priority = ["sony", "xbox", "any"]
deadzone_pct = 12
acceleration = 1.6

[bindings]
move = "left_stick"
commit = "south"
shift = "west"
# ... many more
```

Config hot-reload via `inotify` is planned for M7.

## IPC and `thumbboardctl`

The daemon listens on a Unix socket. The `thumbboardctl` CLI client sends JSON commands:

```bash
thumbboardctl show
thumbboardctl hide
thumbboardctl toggle
thumbboardctl set-theme Default
thumbboardctl set-language ru
thumbboardctl reload-config
```

See [docs/ARCHITECTURE.md § 10](docs/ARCHITECTURE.md) for details.

## Internationalization

Thumbboard supports **multiple alphabets simultaneously**. Users with `layouts = ["us", "ru", "cz"]` can type English, Cyrillic, and Czech in one sentence without restart. Layouts are loaded from `data/layouts/` as JSON; adding a new language is a JSON PR, no recompile. See [docs/ARCHITECTURE.md § 8](docs/ARCHITECTURE.md) and [ADR 0006](docs/adr/0006-internationalization.md).

## Known Constraints

1. **Pre-alpha.** The build and run work *after M0 lands*. Currently in design phase.
2. **Single-threaded.** One main event loop, no worker threads (yet). All Wayland I/O is synchronous.
3. **No exceptions in hot paths.** Use `std::expected<T, Error>` for fallible operations.
4. **core/ must not depend on Wayland or SDL.** If you're editing `core/`, make sure it stays pure logic, testable without a compositor or gamepad.
5. **Gamepad input is never logged.** Privacy by design. See [docs/ARCHITECTURE.md § 9](docs/ARCHITECTURE.md).

## Development Tips

- **Before editing**, skim the relevant section in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). It's the authoritative guide.
- **Code style**: Run `clang-format -i` on your changes.
- **Testing**: Unit tests live in `tests/unit/`. Add one when you write a `core/` class.
- **Logging**: Default backend is stderr. Use `log::info()`, `log::warn()`, `log::error()`. Avoid logging gamepad input.
- **Commit messages**: Reference the ADR or section number when applicable (e.g., "input: Add curve smoothing per ADR 0003").
- **Blocking I/O**: All Wayland operations are synchronous on the main thread. If a compositor request hangs, it blocks the whole UI. Keep Wayland calls fast; use `input_method_v2` v2 surface roles when available.

## Contact / Issues

Issues and PRs live on [GitHub](https://github.com/Topazen/thumbboard). The maintainer is the original author and primary user.
