# Thumbboard — Roadmap

Milestones M0 → M7. Each milestone produces a runnable demo. We don't
move on until the previous one is green: tests pass, lint is clean, you
can show a friend the artifact described in the *Demo* line.

The point of slicing the work this thin is twofold. One, you stay
unblocked — every week you have something visible to show. Two, this
mirrors how real teams work: a sprint shipping a vertical slice beats
a quarter shipping nothing.

Time estimates are *relative*, not calendar weeks. They're rough ratios
to help you notice when something is taking 3× longer than expected
(usually a sign you should ask for help).

---

## M0 — Hello, Wayland (≈ 1.0×)

**Goal:** A blank window on the screen that we *own*.

- Set up the Meson build, get `pkg-config` finding `wayland-client`
  and `egl`.
- Wire up `wayland-scanner` to generate code from
  `wlr-layer-shell-unstable-v1.xml` and
  `virtual-keyboard-unstable-v1.xml`.
- Implement `WaylandClient`: connect to display, run registry
  handshake, bind globals.
- Implement `LayerSurface`: create a layer-shell surface anchored
  bottom, full width, 40% height.
- Implement `EglContext`: get an EGL context bound to that surface,
  call `glClearColor(0.05, 0.08, 0.12, 0.9)` and present.
- Handle SIGINT cleanly.

**Demo:** `thumbboard` runs on Hyprland or Sway, draws a translucent
dark slab at the bottom of the screen, exits cleanly on Ctrl-C.

**Acceptance**

- `meson compile -C build` succeeds with `-Wall -Wextra -Wpedantic
  -Werror`.
- `valgrind ./build/thumbboard` reports zero leaks on shutdown.
- The CI job is green.

---

## M1 — Hello, key (≈ 0.5×)

**Goal:** Prove the keystroke output path works. Beauty comes later.

- Implement `XkbContext`: build a US-English xkb keymap and serialize
  it to a memfd that `input_method_v2` can take.
- Implement `VirtualKeyboard`: bind the global, send the keymap,
  expose `press(keysym)` / `release(keysym)`.
- Read raw `evdev` from `/dev/input/event*` for *one* keyboard or
  controller — anything that has a button — and on any button press,
  send the letter `a` via `VirtualKeyboard`.
- Yes, we use evdev directly here, before SDL3 lands. Reason: this
  is throwaway code to validate the Wayland output path *in
  isolation*. It deletes itself in M3.

**Demo:** Open a terminal, focus it, run `thumbboard` in another
terminal, press any button on any input device, and `aaaaa` appears
in the focused terminal.

**Acceptance**

- The keysym round-trips: `xev` (or your compositor's input logger)
  shows `KeyPress XK_a`.
- The focused app receives the character. This works in Hyprland,
  Sway, and at least one other wlroots compositor (river or labwc).

---

## M2 — Hello, GUI (≈ 1.5×)

**Goal:** A static QWERTY grid drawn with shaders. No input handling,
no cursor.

- Implement `ShaderProgram`, `Mesh`. Load
  `data/shaders/{key.vert,key.frag}`.
- Implement `FontAtlas`: rasterize glyphs `a-z`, `A-Z`, `0-9`, and
  punctuation with FreeType into one GL texture. Pack with a simple
  shelf-packing algorithm.
- Build the keyboard mesh from `data/layouts/qwerty.json`. One quad
  per key.
- Render: per-key fill rectangle + glyph from atlas.

**Demo:** A static QWERTY layout drawn at the bottom of your screen.
No interaction yet, but it looks like a keyboard.

**Acceptance**

- The frame takes <2 ms on integrated graphics
  (`GL_ARB_timer_query`).
- All keys are pixel-aligned (no fuzzy edges).
- Resizing the compositor's output doesn't break the layout.

---

## M3 — Cursor, SDL3, and the summon combo (≈ 1.2×)

**Goal:** A focused key that you can move with a controller — and a
gamepad-only way to bring the keyboard up and dismiss it.

- Add SDL3 dependency to Meson. Initialize with `SDL_INIT_GAMEPAD`
  only. (No video, no audio. We don't ship a window through SDL.)
- Implement `GamepadManager`: open the first connected gamepad, fan
  events into the main loop's poll set.
- Implement `CursorController`: convert left-stick analog axes into
  cell-snap movement with deadzone and hysteresis. Tunable via
  config.
- Add a `KeyboardState::cursor` field and a `MoveCursor` action in
  `InputMapper`.
- Render: the focused key gets a flat 2 px outline (no animation
  yet, see ADR — animation lands in M6).
- The "south" button on the controller commits the focused key
  through `VirtualKeyboard::press`.
- **Implement the summon combo** ([ARCHITECTURE.md §9](../docs/ARCHITECTURE.md#9-gamepad-summon-combo-always-on-listener)).
  Default `summon = ["start", "select"]`, 250 ms hold. The combo
  works *both* when the keyboard is hidden (shows it) and when it's
  visible (hides it). InputMapper checks visibility before applying
  any other binding.
- **Delete the M1 evdev shim.** All input now goes through SDL3.

**Demo:** Plug a controller. Hold Start+Select for a quarter
second — keyboard slides up. Move the cursor over a letter, press
A, the letter appears in the focused terminal. Hold Start+Select
again — keyboard slides away.

**Acceptance**

- Works with Xbox, PlayStation, and Switch Pro controllers.
- `gamepad_manager.cpp` is under 400 LoC.
- No pointer/cursor jitter at cell boundaries (the hysteresis is
  doing its job).
- Holding Start+Select while a game is in the foreground does
  **not** prevent the game from receiving its own button events
  (we don't grab the device).
- Brief accidental presses of Start+Select (under the configured
  hold duration) do not trigger the keyboard.

---

## M4 — Modifiers and layers (≈ 1.0×)

**Goal:** A keyboard you can actually type on.

- Add Shift, Backspace, Space, Enter handling. Shift is sticky for
  one keypress (one-shot), double-tap for caps lock.
- Implement layers: `abc` / `123` / `!#$`. Triggered by a layer-key
  on the keyboard *and* by shoulder buttons (configurable).
- Render the alt-glyph in the corner of each key.
- Render the layer pill in the top-left.

**Demo:** Type `Hello, World! 123` end to end with only the
controller.

**Acceptance**

- Backspace deletes one character per press.
- Shift behaves like a one-shot modifier (try
  `xev`/`wev` to verify keysym sequence).
- Layer change is instantaneous (we don't have animations yet).

---

## M5 — Daemon mode and IPC (≈ 0.7×)

**Goal:** Thumbboard is a background process you summon.

- Implement `Ipc`: Unix socket at `$XDG_RUNTIME_DIR/thumbboard.sock`,
  accept JSON commands.
- Build `tools/thumbboardctl/` — a tiny C++ CLI that opens the socket
  and writes one line.
- Commands: `show`, `hide`, `toggle`, `reload-config`.
- Hide-by-default: when daemon starts, surface is created but
  unmapped until `show` arrives.
- Document Hyprland/Sway keybind examples in README:
  `bind = SUPER, K, exec, thumbboardctl toggle`.

**Demo:** Bind `Super+K` in your compositor to `thumbboardctl
toggle`. Press it from anywhere, the keyboard appears or disappears.

**Acceptance**

- Stale socket is cleaned up on start (handle prior crash).
- IPC handles malformed JSON without dying.
- `thumbboardctl` exits non-zero if the daemon isn't running, with a
  helpful message.

---

## M6 — Animations (≈ 1.5×)

**Goal:** It feels like a real keyboard now.

This is the GUI.md §3 work. Implementation is the bulk of the
milestone; design is already done.

- Implement `AnimationSystem`: tween library, easing functions.
- Wire up entrance / exit slide.
- Wire up focus-ring fade-in / fade-out (replaces M3's hard outline).
- Wire up press squish.
- Wire up layer cross-fade.
- Implement frame-callback-driven render loop. Idle = zero CPU.
- Add backdrop blur shader (`blur.frag`) — gracefully no-op if the
  compositor doesn't expose backdrop hints.

**Demo:** A side-by-side video, M5 vs M6, makes the difference
obvious to anyone glancing at it.

**Acceptance**

- Idle keyboard uses 0% CPU on `top`.
- All four animation classes (entrance, focus, press, layer) ship.
- 60 fps holds on integrated graphics during a key-mash test.

---

## M7 — Configurability + internationalization (≈ 1.5×)

**Goal:** Users can theme it, remap it, and type in their own
language without rebuilding.

This milestone is two intertwined chunks: the config plumbing, and
the i18n payoff that uses it.

### M7a — Config plumbing

- Implement `Config`: `tomlplusplus`, schema validation, helpful
  errors.
- Implement theme loading from `data/themes/*.json` and from
  `~/.config/thumbboard/themes/`. Theme is selected by its `"name"`
  field (e.g. `theme = "Default"`), not its filename.
- Implement keybind remapping in TOML, including the **summon
  combo** added in M3.
- Add `inotify` watcher for live config reload.

### M7b — Multi-layout xkb + i18n layouts

This is where [ADR 0006](adr/0006-internationalization.md) lands.

- Implement `XkbContext` multi-group keymap (`xkb_keymap_new_from_names`
  with `layout = "us,cz,ru"`).
- Implement layout JSON loading. v1.0 ships three built-in layouts:
  `data/layouts/us.json`, `cz.json` (Czech QWERTZ with `č š ž ů á
  é í ý`), `ru.json` (Russian Cyrillic ЙЦУКЕН).
- Wire layout switching to the `layer_next` and `language_next`
  bindings (cycle vs. jump-by-language).
- Update `FontAtlas` to pre-rasterize the union of glyphs across
  all enabled layouts at startup. Verify the bundled font (Inter
  by default) covers Latin Extended-A and Cyrillic; document
  Noto Sans as the recommended drop-in.
- Document the layout JSON schema and ship a tiny
  `tools/layout-validator/` (post-v1.0 nice-to-have, not required
  to ship M7) so contributors can submit new layouts.
- Verify Czech `Compose c v → č` and German `Compose a " → ä`
  pathways work end-to-end on the supported compositors. We don't
  *implement* compose; we just send the right keysyms and trust
  the user's `XCOMPOSEFILE`.

**Demo:** With `layouts = ["us", "cz", "ru"]` in config, type the
sentence *"Hello, ahoj, привет!"* end-to-end with only the
controller, switching language twice.

**Acceptance**

- All hardcoded constants from M0-M6 are now config-driven.
- Bad config produces a clear error, not a crash.
- README documents the config schema with an example file.
- All three built-in languages render correctly (no `□` boxes for
  missing glyphs).
- Czech `č` and German `ä` reach the focused application as the
  correct UTF-8 codepoint (verified with `wev` or by typing into
  Firefox).

---

## v1.0 release checklist

After M7 is green:

- [ ] README has install instructions for Arch and Ubuntu, a GIF, and
      compositor compatibility table.
- [ ] CHANGELOG.md exists and documents M0-M7.
- [ ] CI builds on Ubuntu 24.04 *and* Arch (via Docker).
- [ ] An AUR PKGBUILD is staged.
- [ ] An issue template and a CONTRIBUTING.md exist.
- [ ] You've recorded a 60-second demo video.
- [ ] You've posted to r/unixporn, r/wayland, r/hyprland with the
      video. *This step matters.* It's how you find out if anyone
      besides you wants this. Real software ships with marketing.

---

## Deferred to v2.0+

These are good ideas. They're not v1.0.

- `input-method-v2` backend (preedit, surrounding-text, auto-show).
- `xdg-toplevel` surface fallback for GNOME/Mutter
  ([ADR 0005](adr/0005-surface-backend.md)).
- Predictive text / word completion.
- Long-press popup for diacritic alternates (hold `c` → `č ć ç`)
  ([ADR 0006](adr/0006-internationalization.md)).
- More built-in layouts (Polish, Ukrainian, Greek, Arabic, Hebrew).
  v1.0 ships US + CZ + RU; the rest land as JSON pull requests.
- Daisywheel layout (the Steam Big Picture style).
- Multi-monitor handling (which output to draw on).
- Touch input (so the same daemon works on touchscreens).
- Gyro-driven cursor (Steam Deck flick-to-type).
- Voice input integration.
- Remembered per-app layouts (terminal gets vim-key layout, etc.).

---

## How to know you're stuck

If a milestone has been "almost done" for >2× its estimate, you are
stuck, even if it doesn't feel like it. The recovery move:

1. Write down, in one paragraph, what you tried and what didn't
   work.
2. Reduce the milestone's scope. M2 doesn't need ligature-aware text
   shaping; M2 needs `a-zA-Z` rendered visibly. Cut the parts you're
   stuck on, ship the milestone, file follow-up tasks.
3. Ask. The Hyprland and Sway IRC/Discord channels answer wlroots
   questions in minutes. Mesa folks live on Mesa's IRC. SDL3
   questions are on the libsdl Discord. Asking is the senior move,
   not the junior one.
