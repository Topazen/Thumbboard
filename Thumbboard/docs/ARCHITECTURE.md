# Thumbboard — Architecture

> A lightweight, gamepad-driven virtual keyboard daemon for Wayland (wlroots).

This document is the single source of truth for *how* Thumbboard is built.
If anything in the source disagrees with this doc, we update one of them
in the same commit. Read [ROADMAP.md](ROADMAP.md) for *when* each piece
lands, [GUI.md](GUI.md) for *how it looks*, and [adr/](adr/) for *why*
each major dependency was chosen.

---

## 1. Goals and non-goals

**Thumbboard is**:

- A daemon process that draws a floating on-screen keyboard on a
  wlroots-based Wayland compositor (Hyprland, Sway, river, …) and
  forwards the keys you type to whatever app currently has keyboard
  focus.
- Driven by a gamepad. The whole point is that you can sit on the couch
  with a controller and type a Steam search, a URL, a chat message —
  without a real keyboard.
- Written in modern C++20.
- Hardware-accelerated. No flicker, smooth animations, low CPU.
- Configurable via a TOML file under `~/.config/thumbboard/`.

**Thumbboard is not**:

- A compositor. It is a *Wayland client*. It runs *on top of* a
  compositor that already exists.
- An IME (input method editor). v1.0 does not do candidate windows,
  preedit, or text prediction. That's a future v2.0 feature gated on
  switching to `input-method-v2`.
- An X11 program. No XWayland fallback. Wayland-only is a feature.
- A GTK/Qt application. We render with raw OpenGL ES on a layer-shell
  surface — no toolkit.

---

## 2. Locked-in technology choices

These six decisions are settled. Each has a full ADR explaining the
trade-offs.

| Layer            | Choice                                          | ADR |
|------------------|-------------------------------------------------|-----|
| Keystroke output | `input_method_v2`                               | [0001](adr/0001-wayland-protocol.md) |
| Rendering        | OpenGL ES 3.0 over EGL                          | [0002](adr/0002-renderer.md) |
| Gamepad input    | SDL3 GameController API                         | [0003](adr/0003-gamepad-library.md) |
| Layout style     | Pointer-walk QWERTY (Steam Deck-ish)            | [0004](adr/0004-layout-style.md) |
| Surface backend  | `wlr-layer-shell` via a `Surface` interface     | [0005](adr/0005-surface-backend.md) |
| Internationalization | Multi-layout xkb keymap + per-layout JSON   | [0006](adr/0006-internationalization.md) |

Other deliberate choices (no separate ADR, but worth calling out):

- **Build system: Meson + Ninja.** What wlroots, Sway, Hyprland and most
  Wayland projects use. Easier to read than CMake, and recruiters in
  this niche read Meson fluently.
- **Surface protocol: `wlr-layer-shell-unstable-v1`.** The standard way
  for non-compositor Wayland clients to draw an always-on-top overlay.
  Hyprland, Sway, river, labwc and **KDE Plasma 6** all support it.
  GNOME's Mutter does not; that's the planned `xdg-toplevel` fallback
  in [ADR 0005](adr/0005-surface-backend.md).
- **Keysym handling: `libxkbcommon`.** Encodes characters into the
  keysyms that `input_method_v2` requires. Same library every
  Wayland compositor uses internally. We build a *multi-layout*
  keymap to support Czech, Russian, etc. simultaneously — see
  [ADR 0006](adr/0006-internationalization.md).
- **Text: FreeType + HarfBuzz, our own glyph atlas.** Heavy for a
  keyboard, but lets us animate text and theme it. No FontConfig at
  runtime — config picks one font.
- **Logging: stderr by default, optional spdlog backend.** `journalctl
  --user -u thumbboard` captures stderr cleanly.
- **C++ standard: C++20.** We use `concepts`, `std::span`,
  `std::ranges`, designated initializers. C++23 only where Clang 17 and
  GCC 13 both support it.

---

## 3. Process model and threading

**One process, one thread, one event loop.**

This is intentional. A keyboard is fundamentally an interactive UI
glued to three external event sources:

1. The Wayland display server's socket (events about focus, output
   geometry, surface configure).
2. The kernel's evdev devices (gamepad input, surfaced through SDL3's
   internal udev/evdev plumbing).
3. A frame-pacing timer (we redraw at the compositor's frame callback
   cadence, typically 60 or 120 Hz).

All three are file descriptors. We poll them in one place using
`epoll(7)` (or `poll(2)` if we're being portable). Threads buy us
nothing here and would make the Wayland event handling harder, since
`wl_display_dispatch` is not thread-safe by default.

```
                  ┌──────────────────────────┐
                  │       main loop          │
                  │  epoll_wait on:          │
                  │    - wl_display_get_fd() │
                  │    - SDL_WaitEventTimeout│
                  │    - frame_timer_fd      │
                  │    - ipc_socket_fd       │
                  └────────┬─────────────────┘
                           │
       ┌───────────────────┼─────────────────────────┐
       ▼                   ▼                         ▼
  Wayland events      Gamepad events            Frame tick
  - configure         - axis moved              - update animation
  - focus changed     - button pressed          - render frame
  - output info       - device connected        - swap buffers
       │                   │                         │
       └───────┬───────────┘                         │
               ▼                                     │
       ┌───────────────┐                             │
       │ KeyboardState │ ◄───────────────────────────┘
       └───────┬───────┘
               │
   ┌───────────┴───────────┐
   ▼                       ▼
 Renderer            VirtualKeyboard
 (draws frame)       (sends keysym to compositor)
```

If we ever need real concurrency (e.g., async config reload, network
features) we add a worker thread that posts results back to the main
loop via an `eventfd`. We don't need it yet.

---

## 4. Component breakdown

### 4.1 `app/` — process glue

| Class    | Responsibility |
|----------|----------------|
| `Daemon` | Owns the event loop. Holds every other subsystem. Handles SIGINT/SIGTERM cleanly. |
| `Config` | Parses `~/.config/thumbboard/config.toml`. Hot-reload via `inotify` (M7). |
| `Logger` | Thin wrapper. Default backend writes to stderr; spdlog backend is opt-in via Meson option. |
| `Ipc`    | Unix socket at `$XDG_RUNTIME_DIR/thumbboard.sock`. Accepts JSON commands like `{"cmd":"show"}` from `thumbboardctl`. (M5) |

### 4.2 `core/` — pure logic, no I/O

This module *must* be testable without a Wayland compositor or a
gamepad. If `core/` ever depends on `wayland-client.h` or `SDL.h`,
something is wrong.

| Class            | Responsibility |
|------------------|----------------|
| `KeyboardState`  | Cursor position (in key-grid coordinates), pressed modifiers, current layer (alpha/numeric/symbol), visibility. |
| `Layout`         | Abstract base. Maps grid coordinates → `Key`. |
| `QwertyLayout`   | Concrete layout. Loaded from JSON in `data/layouts/`. |
| `Key`            | A struct: keysym, label, optional alt-label, position, size. |
| `Theme`          | Color palette, key shape parameters, font name, animation curves. Loaded from `data/themes/*.json`. |

### 4.3 `input/` — gamepad → intent

The job here is to turn raw analog noise into clean *intents*: "move
cursor", "commit key", "shift", "hide". The renderer and the Wayland
side never see analog axes.

| Class             | Responsibility |
|-------------------|----------------|
| `GamepadManager`  | SDL3 wrapper. Enumerates controllers, handles hot-plug, fans events into `InputMapper`. |
| `InputMapper`     | Translates SDL events into `Action` values: `MoveCursor(dx, dy)`, `CommitKey`, `ToggleShift`, `Backspace`, `HideKeyboard`. Bindings come from config. |
| `CursorController`| Smooths analog stick input: deadzone, exponential acceleration curve, hysteresis to prevent cursor jitter at cell boundaries. |

### 4.4 `wayland/` — talking to the compositor

| Class             | Responsibility |
|-------------------|----------------|
| `WaylandClient`   | Owns `wl_display`, runs the registry handshake, binds globals. |
| `LayerSurface`    | Wraps `zwlr_layer_surface_v1`. Anchors the keyboard to the bottom of the screen, sets exclusive zone, handles configure events. |
| `VirtualKeyboard` | Wraps `zwp_input_method_v2`. Owns the xkb keymap that's shared with the compositor. Sends `key_press` / `key_release`. |
| `XkbContext`      | Builds and serializes the keymap; converts our internal `KeySymbol` to xkb keycodes. |

### 4.5 `render/` — pixels

The render module is intentionally narrow. It takes a `KeyboardState` +
a `Theme` and draws one frame.

| Class           | Responsibility |
|-----------------|----------------|
| `EglContext`    | Creates the EGL display/context bound to our `wl_surface`. |
| `Renderer`      | The frame function. Walks current `Layout`, queries `AnimationSystem` for per-key state, issues GL calls. |
| `ShaderProgram` | RAII wrapper around a GL program. Compiles `*.vert` + `*.frag` from `data/shaders/`. |
| `Mesh`          | VAO/VBO holding the keyboard geometry. Rebuilt on configure (resize), not per frame. |
| `FontAtlas`     | Pre-rasterizes glyphs with FreeType into a single GL texture. HarfBuzz handles shaping for non-Latin scripts (M7). |
| `AnimationSystem`| Per-key tween state: press squish, focus glow, slide-in entrance. Driven by frame delta time. See [GUI.md §3](GUI.md). |

---

## 5. Directory layout

```
thumbboard/
├── meson.build
├── meson_options.txt
├── README.md
├── LICENSE
├── .clang-format
├── .clang-tidy
├── .editorconfig
├── .gitignore
├── .github/workflows/ci.yml
│
├── docs/
│   ├── ARCHITECTURE.md          ← this file
│   ├── GUI.md                   ← visual design + animations
│   ├── ROADMAP.md               ← M0 → M7 milestones
│   └── adr/
│       ├── 0001-wayland-protocol.md
│       ├── 0002-renderer.md
│       ├── 0003-gamepad-library.md
│       └── 0004-layout-style.md
│
├── protocols/                   ← Wayland protocol XML
│   ├── virtual-keyboard-unstable-v1.xml
│   └── wlr-layer-shell-unstable-v1.xml
│
├── include/thumbboard/           ← public headers (only if we ship libthumbboard)
│
├── src/
│   ├── main.cpp
│   ├── app/
│   ├── core/
│   ├── input/
│   ├── wayland/
│   └── render/
│
├── data/
│   ├── layouts/qwerty.json
│   ├── themes/{steam,maliit,default}.json
│   └── shaders/{key.vert,key.frag,glow.frag}
│
├── tests/
│   ├── unit/
│   └── integration/
│
└── tools/
    └── thumbboardctl/            ← CLI client for the IPC socket
```

---

## 6. Dependencies (system packages)

```
build:    meson, ninja, pkg-config, clang-format, clang-tidy
wayland:  wayland-client, wayland-protocols, wayland-scanner
input:    libxkbcommon, sdl3
render:   egl (mesa), glesv2 (mesa), freetype2, harfbuzz
config:   tomlplusplus  (header-only)
optional: spdlog, catch2
```

On Arch:

```
sudo pacman -S meson ninja clang \
    wayland wayland-protocols libxkbcommon \
    sdl3 mesa freetype2 harfbuzz \
    tomlplusplus catch2
```

On Ubuntu 24.04:

```
sudo apt install meson ninja-build clang-format clang-tidy \
    libwayland-dev wayland-protocols libxkbcommon-dev \
    libsdl3-dev libegl1-mesa-dev libgles2-mesa-dev \
    libfreetype-dev libharfbuzz-dev \
    libtomlplusplus-dev catch2
```

---

## 7. Configuration

`~/.config/thumbboard/config.toml`:

```toml
[general]
theme   = "Default"               # display name; resolves to data/themes/default.json
auto_show_on_focus = false        # needs input-method-v2; v2.0

[i18n]
layouts  = ["us", "cz", "ru"]     # all active simultaneously, one xkb keymap
default  = "us"                   # which layout is active on startup

[geometry]
height_pct = 40                   # % of output height
margin_px  = 24

[input]
gamepad_priority = ["sony", "xbox", "any"]
deadzone_pct     = 12
acceleration     = 1.6            # 1.0 = linear

[bindings]
# Bindings while the keyboard is *visible*.
move          = "left_stick"
commit        = "south"           # A on Xbox, X on Sony
shift         = "west"            # X on Xbox, square on Sony
backspace     = "east"            # B on Xbox, circle on Sony
space         = "north"           # Y on Xbox, triangle on Sony
hide          = "start"
layer_next    = "right_shoulder"  # cycles abc→123→!#$→cz:abc→ru:abc→…
layer_prev    = "left_shoulder"
language_next = "right_thumbstick"  # press R3 to jump straight to next language

# The summon combo. Active *whether the keyboard is visible or not*,
# so the user can pop it up from any application without a compositor
# keybind. See §9 for the always-on listener design.
summon        = ["start", "select"]   # any combo of buttons; held simultaneously

---

## 8. Internationalization

This section is a summary; the full reasoning is in [ADR 0006](adr/0006-internationalization.md).

Thumbboard supports multiple alphabets active *simultaneously*. A
user with `layouts = ["us", "ru", "cz"]` can type English URLs, then
flip to Czech to chat with `ahoj!`, then to Cyrillic, all in one
sentence, without restart.

The implementation is four pieces:

1. **`XkbContext` builds one multi-group xkb keymap.** The keymap
   contains all enabled layouts as xkb groups (`xkb_keymap_new_from_names`
   with `layout = "us,cz,ru"`). Switching the active language is
   `xkb_state_update_mask` with a new group index.
2. **Each layout ships as JSON in `data/layouts/`.** v1.0 ships
   `us.json`, `cz.json` (Czech QWERTZ), `ru.json` (ЙЦУКЕН). Each
   key carries a `keysym` name (e.g. `"ccaron"`) plus a `label`
   (the UTF-8 glyph drawn on the key).
3. **Layer switching is uniform.** From the user's POV there's one
   layer cycle: `us:abc → us:123 → us:!#$ → cz:abc → cz:123 → ru:abc
   → …`. The `language_next` button jumps straight to the next
   language's `abc` layer for speed.
4. **Diacritics use xkb's existing dead-key + Compose machinery.**
   We don't reimplement compose; we send the right keysyms and the
   compositor (which links libxkbcommon too) honors `XCOMPOSEFILE`
   for the user. Czech `č` via `Compose c v`, German `ä` via
   `Compose a "`, etc.

The `FontAtlas` pre-rasterizes the union of glyphs across all
enabled layouts at startup. With US + CZ + RU that's ~200 glyphs and
fits in a 1024² atlas. The bundled / recommended font is **Inter**
(Latin Extended-A coverage) with **Noto Sans** as the documented
fallback for any script.

Adding a new language is a JSON pull request — no recompile.

---

## 9. Gamepad summon combo (always-on listener)

The keyboard is invisible by default. The user must be able to bring
it up from anywhere without going to a terminal or relying on the
compositor.

Two paths exist and we ship both:

**Path A — Compositor keybind → IPC.** A user with Hyprland binds
`Super+K` to `thumbboardctl toggle`. Works today. See §10.

**Path B — Gamepad combo.** A user defines a button combo in
`~/.config/thumbboard/config.toml`; pressing it on the gamepad
toggles the keyboard. Default: **Start + Select** held together.
This is the path that matters from the couch — your hands are on
the controller, not the keyboard.

### How Path B is wired

The crux: when the keyboard is hidden, we still need to read the
gamepad to detect the summon combo. SDL3 reads `/dev/input/event*`
through evdev and **does not grab** the device — multiple processes
can read the same controller events at the same time. So a game
running in the foreground gets its inputs while Thumbboard
simultaneously watches for our combo. No conflict, no exclusivity
problem.

Architecturally:

```
┌─────────────────────────────────────────────────────────┐
│  GamepadManager  (SDL3, INIT_GAMEPAD only, never grabs) │
│      │                                                  │
│      ▼                                                  │
│  InputMapper                                            │
│   ├─ when hidden: only the SUMMON combo matters         │
│   │     → on match, fire show()                         │
│   └─ when visible: full action mapping (move/commit/…)  │
└─────────────────────────────────────────────────────────┘
```

`InputMapper::on_gamepad_event` checks visibility *first*. While
hidden, every event except a summon-combo match is dropped on the
floor. While visible, every event flows into the normal action
pipeline.

### Combo detection rules

- A combo is a **set** of buttons that must all be currently held.
  Order doesn't matter.
- To prevent false positives during gameplay (Start+Select is also
  a "open Steam menu" combo many people use), the user can require
  a **hold duration**, default 250 ms.
- The combo fires on *transition* (rising edge of the last button),
  not on every frame the buttons are held — pressing once shows the
  keyboard, pressing again hides it.
- Single-button summons are allowed (`summon = ["touchpad"]` for
  PS5, for example) but discouraged — too easy to trigger by
  accident.

### Privacy / passthrough note

Because Thumbboard listens to the gamepad even when not visible,
anyone reading this code should understand:

- **We do not log gamepad input.** Even at trace level. The
  `GamepadManager` will have an explicit code review line item
  saying "no logging of inputs ever."
- **We never read keyboards or mice.** Only `SDL_GAMEPAD_*`
  events. SDL is initialized with `SDL_INIT_GAMEPAD` exclusively.

---

## 10. IPC — `thumbboardctl`

The daemon listens on a Unix socket. A tiny CLI client called
`thumbboardctl` writes one-line JSON commands to it.

```
$ thumbboardctl show
$ thumbboardctl hide
$ thumbboardctl toggle
$ thumbboardctl reload-config
$ thumbboardctl set-theme Default
$ thumbboardctl set-language ru
```

This is how we'll bind keyboard show/hide to a window-manager keybind
in Hyprland or Sway *before* `input-method-v2` (which would let the
compositor itself ask us to show up when a text field is focused).

---

## 11. Threading model summary

- **Main thread** does Wayland, SDL events, rendering. Period.
- **No worker threads** until we have a concrete reason. Async config
  reload (`inotify`) goes through a single fd in the main poll loop.
- **All shared state is owned by `Daemon`.** Subsystems hold non-owning
  references. No raw `new`/`delete` — `std::unique_ptr` everywhere.

---

## 12. Error handling philosophy

- **Wayland and EGL failures are fatal.** If we can't bind
  `input_method_v2` we log and exit, because we can't do our job.
  The compositor probably needs a different config; we tell the user
  which protocol is missing.
- **Gamepad disconnect is *not* fatal.** Hide the keyboard, wait for a
  controller to come back. Log at INFO level.
- **Config errors are recoverable.** Bad TOML → log a warning, fall
  back to baked-in defaults.
- **No exceptions in hot paths.** We use `std::expected<T, Error>` for
  fallible operations (Wayland binds, file loads). Exceptions only
  cross from `main()` to the OS; everything inside `Daemon::run()`
  catches and translates them to error returns.

---

## 13. Testing strategy (preview — see [ROADMAP.md M0](ROADMAP.md))

- `core/` is unit-tested with Catch2. Coverage target: >80%.
- `input/` is unit-tested by feeding canned `SDL_Event` streams.
- `wayland/` and `render/` are *integration*-tested against a headless
  Sway instance running in CI. We assert on screenshots and on the
  keysyms the compositor receives.
- No mocks of the Wayland protocol. We test against the real thing.

---

## 14. Decided Questions (Historical)

These were questions settled during development:

1. **Multi-output / multi-monitor setups.** Punted to v2.0. v1.0 picks
   the first output it sees.
2. **Should layouts be loaded from `data/` at install time, or
   compiled in?** Both. Core layouts (US, CZ, RU) will be compiled in
   via a generated C++ header (e.g., using an `xxd`-like script) for
   a robust fallback. User overrides and additional layouts will be
   loaded from `data/layouts/` on disk.
3. **Predictive text (M6+).** The project has adopted the GPL-3 license,
   which means we are license-compatible with the `presage` library.
   We will integrate `presage` for predictive text instead of writing
   our own bigram model.
4. **Ukrainian / Polish / German layouts as built-ins?** These are
   deferred to v1.1. v1.0 ships with US, CZ, and RU as defined in ADR 0006.
