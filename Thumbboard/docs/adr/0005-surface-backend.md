# ADR 0005 — Surface backend abstraction (KDE Plasma 6 support)

- **Status:** Accepted
- **Date:** 2026-04-26
- **Deciders:** Maksym
- **Supersedes:** none
- **Affects:** [ADR 0001](0001-wayland-protocol.md), [ARCHITECTURE.md §4.4](../ARCHITECTURE.md)

## Context

[ADR 0001](0001-wayland-protocol.md) chose `input_method_v2` for
sending keystrokes. That decision is unchanged.

The *surface* the keyboard draws on is a separate question.
[ARCHITECTURE.md §2](../ARCHITECTURE.md) originally locked in
`wlr-layer-shell-unstable-v1` and listed only wlroots-based
compositors as targets. After feedback, KDE Plasma — which uses
KWin, not wlroots — is now an explicit primary target. KDE is the
most-used Linux desktop and skipping it leaves the project niche.

Two facts changed our calculus:

1. **KWin (Plasma 6) added `wlr-layer-shell-unstable-v1` support**
   in 2024. As of Plasma 6.x it implements the same protocol our
   wlroots backends do, with minor edge-cases around exclusive zones.
   See: [KWin merge request adding wlr-layer-shell](https://invent.kde.org/plasma/kwin)
   and the Plasma 6 release notes. (Verify against the Plasma version
   you're testing on — protocol support is checked at runtime anyway.)
2. **A small minority of Wayland compositors still don't expose
   layer-shell.** GNOME's Mutter is the obvious example. Anyone
   running labwc or river also has it, so this affects only Mutter
   and a handful of niche compositors.

So we have a "primary path supports 99% of users" reality plus a
"fallback path keeps the door open" desire.

## Decision

**Introduce a `Surface` abstraction with one concrete implementation
in v1.0 and one planned for v2.0.**

```cpp
// src/wayland/surface.hpp  (sketch)
namespace thumbboard::wayland {

class Surface {
public:
    virtual ~Surface() = default;
    virtual void show() = 0;
    virtual void hide() = 0;
    virtual void on_configure(uint32_t serial, int w, int h) = 0;
    virtual wl_surface* wl() = 0;       // for EGL
    virtual bool exclusive_zone() const = 0;
};

class LayerShellSurface : public Surface { /* v1.0 */ };
class XdgToplevelSurface : public Surface { /* v2.0, fallback */ };

}
```

At startup, `WaylandClient` checks the registry for
`zwlr_layer_shell_v1`. If present, it instantiates
`LayerShellSurface`. If absent (and once we ship the v2.0 fallback),
it instantiates `XdgToplevelSurface` and logs a warning that
auto-anchoring won't work.

## Considered alternatives

### Option A — Single `LayerShellSurface`, document GNOME as unsupported

What we had until this ADR.

**Pros**: simpler, less code.

**Cons**: closes the door on GNOME and Mutter forever. A future
contributor who wants to add it has to refactor `wayland/` first.
And — this is the actual reason — **the abstraction is cheap to
write today and very expensive to retrofit later**, when other code
is already coupled to the layer-shell API.

### Option B — `Surface` interface with both implementations in v1.0

Most ambitious. Ships GNOME support on day one.

**Pros**: maximum reach.

**Cons**: GNOME doesn't expose `input_method_v2` either —
`input-method-v2` is the only path there. So even with
`XdgToplevelSurface` we still wouldn't have a working keyboard on
GNOME. The whole effort delivers nothing user-visible until we also
ship the input-method-v2 backend ([deferred to v2.0 in ADR
0001](0001-wayland-protocol.md)). Doing all three at once is what
sinks projects.

### Option C — Surface interface in v1.0, only the layer-shell
implementation. (chosen)

The interface costs almost nothing today — three virtual functions —
and the cost of adding `XdgToplevelSurface` later is bounded to one
file. This is the disciplined version of Option A.

## Consequences

- `src/wayland/` grows a `surface.hpp` interface and a
  `layer_shell_surface.cpp` implementation that conforms to it.
  Internally `Renderer` and `LayerSurface` (now renamed to
  `LayerShellSurface`) interact through the abstract `Surface` only.
- README target table updates: KDE Plasma 6 is a primary target
  with the same features as Hyprland/Sway. We keep a "Plasma 6.0+
  required" caveat — older Plasma 5 lacks layer-shell.
- We add a runtime check at startup that prints a clear, actionable
  error if `zwlr_layer_shell_v1` is absent: "Your compositor does
  not expose wlr-layer-shell. Thumbboard's xdg-toplevel fallback
  ships in v2.0; until then, please use Hyprland, Sway, river,
  labwc, or Plasma 6+."
- v2.0 (or earlier if a contributor steps up) ships
  `XdgToplevelSurface`. It's a regular floating window the user
  drags into place once. Less polished than layer-shell, but
  unblocks GNOME.

## How we'll know this was right

- v1.0 runs on Hyprland, Sway, *and* Plasma 6 with no compositor-
  specific code in `Renderer` or anywhere outside `src/wayland/`.
- Adding a new surface backend later involves zero changes outside
  `src/wayland/`.

## How we'll know this was wrong

- Plasma 6 turns out to need compositor-specific quirks that bleed
  past the `Surface` interface — for example, a special configure
  protocol or non-standard exclusive-zone behavior. If that happens,
  we either patch KWin upstream or wrap the quirks inside
  `LayerShellSurface` itself, not bleed them into the rest of the
  code.
