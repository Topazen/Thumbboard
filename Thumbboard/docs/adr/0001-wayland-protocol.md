# ADR 0001 — Use `virtual_keyboard_unstable_v1` for keystroke output

- **Status:** Accepted
- **Date:** 2026-04-26
- **Deciders:** Maksym

## Context

Thumbboard needs to send keystrokes to whatever app currently has Wayland
keyboard focus. Wayland has, broadly, three protocols that can do this:

1. `virtual_keyboard_unstable_v1` — pretends to be a hardware keyboard.
   The client sends raw keysym/keycode events plus a serialized xkb
   keymap. The compositor forwards them to the focused surface as if a
   USB keyboard had been plugged in.
2. `input_method_unstable_v2` — proper IME path. Supports preedit text
   (the underlined "you're typing this but haven't committed" state),
   surrounding-text queries, and is what real on-screen keyboards on
   Plasma Mobile use.
3. `wlr_virtual_pointer_unstable_v1` — irrelevant; that's for mouse
   input. Listed only so future readers don't wonder.

Both `input_method_v2` and `input_method_v2` are widely supported on
wlroots compositors (Hyprland, Sway, river), which is our target.

## Decision

**Use `input_method_v2` for v1.0.** Plan a clean
abstraction inside `src/wayland/` so we can add `input_method_v2` as a
second backend later without touching `core/`.

## Considered alternatives

### Option A — `input_method_v2` only (chosen)

**Pros**

- ~3× less protocol surface. We send `key_press(time, keycode, state)`
  and that's it.
- The "pretend to be a keyboard" model maps directly to how a beginner
  thinks about a keyboard. No mental tax.
- Doesn't require focus tracking or text-field detection.

**Cons**

- No preedit. We can't show "you're typing 'helo' — want 'hello'?"
  suggestions inline.
- The compositor cannot ask us to *show up* when a text field is
  focused. The user has to bring us up manually (via a keybind that
  invokes `thumbboardctl show`).
- We have to ship our own xkb keymap. If the user's locale is e.g.
  German, our US keymap will produce wrong characters until M7.

### Option B — `input_method_v2`

**Pros**

- Proper IME-grade integration. Predictions, candidate windows,
  auto-show on focus.
- This is what every "serious" on-screen keyboard ends up using
  eventually.

**Cons**

- Significantly more protocol surface and lifecycle to learn.
  `zwp_input_method_v2` has `activate`, `deactivate`,
  `surrounding_text`, `text_change_cause`, `content_type` — all of
  which need to be wired up before keys flow.
- Forces us to think about predictive text early, which inflates v1.0
  scope.
- Slightly worse error story: if the compositor doesn't expose this
  global we have to fall back, doubling our code path.

### Option C — Both at once

Build an internal `IKeystrokeSink` interface with both backends shipped
in v1.0.

Rejected as scope inflation. We *will* design `IKeystrokeSink` so
adding `input_method_v2` later is mechanical, but we don't implement
both today.

## Consequences

- `src/wayland/virtual_keyboard.cpp` is the only file that knows the
  protocol details.
- `src/wayland/keystroke_sink.hpp` defines an abstract interface. Today
  it has one implementation; tomorrow it has two.
- v1.0 README has to honestly say: "needs a keybind to show; auto-show
  on focus is v2.0".
- We accept that German/French/Cyrillic users will have wrong layouts
  in v1.0. M7 ships configurable keymaps; until then it's a known
  limitation in the README.

## How we'll know this was right

- We can write the "send keystroke" code path in <300 lines of C++.
- Switching to `input_method_v2` later doesn't require changes outside
  `src/wayland/`.

## How we'll know this was wrong

- We end up reimplementing IME features (preedit, surrounding-text)
  manually inside `core/` because users keep asking for them. If we hit
  that point, we abort and migrate to `input_method_v2` for v1.5.
