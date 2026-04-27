# ADR 0003 — Use SDL3's GameController API for gamepad input

- **Status:** Accepted
- **Date:** 2026-04-26
- **Deciders:** Maksym

## Context

Thumbboard's whole reason to exist is gamepad-driven typing. Reading a
gamepad on Linux means going through one of:

1. **`/dev/input/event*` directly via libevdev** — raw kernel input
   subsystem.
2. **libinput** — what compositors use. Higher-level than evdev.
3. **SDL3 GameController** — game-engine-grade abstraction with
   community-maintained mappings for thousands of controllers.
4. **Custom wrapper around evdev + udev** — what SDL3 is internally.

## Decision

**Use SDL3's GameController API.** Specifically `SDL_OpenGamepad`,
`SDL_PollEvent`, `SDL_GetGamepadAxis`, and friends.

## Considered alternatives

### Option A — SDL3 (chosen)

**Pros**

- Out-of-the-box mappings for Xbox, PlayStation (DualSense + DualShock
  4), Switch Pro, Steam Controller, 8BitDo, generic XInput. The
  community-maintained `gamecontrollerdb.txt` is industry-standard.
- Hot-plug works correctly. Plug a controller mid-session and SDL
  raises an event.
- Handles the worst part of gamepad input: vendor-specific quirks
  (PS5's button order is different, Switch Pro reports gyro on
  different axes). We don't write any of that.
- API is event-driven and integrates cleanly with our `epoll` loop —
  `SDL_WaitEventTimeout` blocks on a poll-able fd internally.
- Battery info, rumble, gyro, touchpad — all available if we want
  them later (Steam Deck-style flick-to-type with the gyro is a fun
  v2.0).

**Cons**

- ~10 MB binary footprint. For a 100 KB keyboard daemon this matters.
  We can shrink it with build flags (`-DSDL_VIDEO=OFF`,
  `-DSDL_AUDIO=OFF`, `-DSDL_RENDER=OFF`). We genuinely only need
  `SDL_INIT_GAMEPAD`.
- Adds a runtime dependency. Most distros ship SDL3 in 2025+; older
  distros need a backport.

### Option B — libevdev directly

**Pros**

- Zero dependencies. Pure Linux kernel API.
- You learn how Linux input *actually works*. evdev structs, EV_ABS
  axes, KEY_BTN_THUMB, the whole thing.
- Tiny: a few KB of code.

**Cons**

- We become responsible for every controller's mapping. PS5 vs Xbox vs
  Switch Pro all expose different axis numbers and button codes.
  Maintaining this is a part-time job.
- Hot-plug requires writing a `udev` monitor ourselves. Doable,
  ~100 LoC, but it's another fd in the loop and another place to get
  it wrong.
- No gyro/rumble unless we implement HID feature reports manually.

### Option C — libinput

**Pros**

- It's what the compositor itself uses. Coherent with the ecosystem.
- Higher-level than evdev — already groups events by device, handles
  permissions.

**Cons**

- libinput's gamepad story is *weak*. It treats gamepads as raw
  pointer/key devices. The "this stick axis is ABS_X on this
  controller and ABS_RX on that one" problem is on us.
- libinput is meant for being driven by a session manager that has
  root-ish capabilities. Running it as a regular user is awkward.

### Option D — Roll our own evdev+udev wrapper

We'd be writing a worse version of SDL's gamepad code. Rejected.

## Consequences

- We add SDL3 as a dependency. CI installs `libsdl3-dev` on Ubuntu,
  `sdl3` on Arch.
- We initialize SDL with **only** `SDL_INIT_GAMEPAD`. We never call any
  video, audio or render APIs of SDL — those are off.
- `src/input/gamepad_manager.cpp` is the only place that includes
  `<SDL3/SDL.h>`. The rest of the codebase consumes our internal
  `Action` enum.
- Future gyro/rumble/touchpad features are essentially free.

## How we'll know this was right

- A user plugs in any controller from the last 10 years and it just
  works.
- `gamepad_manager.cpp` stays under 400 LoC.

## How we'll know this was wrong

- SDL3 isn't packaged on a target distro and packaging it ourselves is
  worse than just talking to evdev. Unlikely in 2026 but possible on
  conservative distros.
