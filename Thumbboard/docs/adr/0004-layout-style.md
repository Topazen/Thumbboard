# ADR 0004 — Pointer-walk QWERTY layout for v1.0

- **Status:** Accepted
- **Date:** 2026-04-26
- **Deciders:** Maksym

## Context

The "layout" — the rule that maps thumbs on a controller to letters on
a screen — is the heart of the user experience. Three shapes are
established in the wild:

1. **Pointer-walk QWERTY** (Steam Deck virtual keyboard, Maliit). A
   familiar QWERTY grid. A cursor moves over keys; you commit with a
   button. The stick can either move the cursor *one cell at a time*
   (D-pad-style) or *as a free-floating pointer*.
2. **Daisywheel** (Steam Big Picture / Steam Controller). 8 radial
   petals of 4 letters each. Left stick angle picks a petal; right
   stick angle picks the letter inside it. Two flicks per character,
   no walking.
3. **Predictive grid** (PlayStation, Xbox dashboard). Hybrid grid with
   an autocomplete bar that captures most input. Needs a language
   model.

## Decision

**Implement pointer-walk QWERTY only for v1.0.** The architecture leaves
a `Layout` interface in place so a daisywheel layout can be added in
v2.0 without changing `core/`, `input/`, or `wayland/`.

## Considered alternatives

### Option A — Pointer-walk QWERTY (Chosen)

**Pros** 

- Familiar to every user. Zero learning curve.
- Easiest to implement: 2D grid + cursor + commit button.
- Easy to test: feed `MoveCursor` actions, assert cursor position.
- Matches the user's stated preference.

**Cons**

- Slow. ~15-25 WPM ceiling for most users. You walk to every letter.
- Not differentiated. Steam Deck has had this for years; this isn't a
  novel project on the strength of the input model alone.
- The "value proposition" of Thumbboard becomes about *being a daemon
  for arbitrary wlroots compositors*, not about typing speed. The
  README has to lead with that framing.

### Option B — Daisywheel as primary

**Pros**

- Up to 40 WPM with practice. Genuinely fast.
- Distinctive. People actively look for this on Linux because Steam's
  isn't accessible outside Big Picture.
- Forces good architecture (radial-selection logic, hysteresis,
  acceleration curves).

**Cons**

- Steeper user learning curve. Not the fit for "type a URL once a week".
- More implementation work — angle math, hysteresis, visual feedback
  for the radial selection.
- The user judged the design choice and preferred QWERTY.

### Option C — Both, switchable at runtime (Maybe implemented for 2.0)

The most ambitious option. Architecturally clean (forces a real
`Layout` plugin system) but doubles v1.0 scope.

We get the architectural benefit *for free* even with Option A — a
sensible interface today means Option B drops in later. So we don't
need to build both up front to get the design discipline.

## Consequences

- `Layout` is an abstract base class from day one, even though only
  one subclass exists in v1.0. That's not over-engineering; it's a
  cheap insurance policy that lets the daisywheel land later as a pure
  addition.
- Layout *data* (key positions, labels, sizes) lives in
  `data/layouts/qwerty.json`, not hardcoded in C++. This means a user
  can tweak the layout without rebuilding.
- The README leads with "for any wlroots compositor" — distribution
  reach — rather than "fastest typing". We're not lying about being
  fast, we're choosing the honest pitch.
- `CursorController` (in `src/input/`) implements *cell-snap* movement
  rather than free-floating pointer. The stick gradually tilts the
  cursor to the next cell with hysteresis to prevent jitter. This
  feels much better than free-pointer for keyboards (it's what Steam
  Deck does).

## How we'll know this was right

- The first user demo is "I sat on my couch and typed
  github.com/thumbboard into Firefox in 8 seconds without touching a
  keyboard." That's enough.

## How we'll know this was wrong

- Users immediately ask for daisywheel. Fine — that's a feature
  request that fits cleanly into the existing `Layout` interface,
  scheduled for v2.0.
