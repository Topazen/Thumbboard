# Thumbboard — GUI design and animation plan

This doc explains *what Thumbboard looks like*, *what we're cribbing
from existing keyboards*, and *how the animation system is built*. It
sits between [ARCHITECTURE.md](ARCHITECTURE.md) (the *what*) and the
shaders in `data/shaders/` (the *how*).

---

## 1. Design DNA — what we're stealing, and from whom

We are deliberately *not* inventing a new design language. We pick the
best ideas from two reference keyboards and combine them.

### 1.1 From Steam (Big Picture / Steam Deck)

**What we take**

- **Heavy translucency over the focused app.** The keyboard is a
  semi-transparent slab anchored to the bottom of the screen with a
  subtle blur behind it. Steam's keyboard does this so you can still
  see the field you're typing into. Implementation: alpha + a one-pass
  Gaussian blur in `glow.frag` reading from the layer-shell
  background hint (M6).
- **Generous key padding and rounded corners.** Steam's keys are big
  rectangles with ~12 px corner radius. Easier to land on with a
  cursor that snaps cell-to-cell.
- **Glow ring on the focused key.** A soft outer glow (Steam-blue,
  configurable) marks the cursor's position. This is the *only* piece
  of UI that tells you where you are. It must always be the brightest
  thing on screen.
- **Press-squish animation.** When you commit a key, it scales to
  ~92% over 80 ms then springs back to 100% over 120 ms. Steam's
  feels heavier than that — we tune by feel.
- **Dark by default, never pure black.** Background is a dark
  desaturated blue, not `#000`. Pure black looks broken on OLED and
  draws attention.
- **One accent color.** Everything that means "this is interactive
  right now" is the same hue. No traffic-light palettes.

**What we don't take**

- Steam's *daisywheel* layout. We picked pointer-walk for v1.0
  ([ADR 0004](adr/0004-layout-style.md)).
- Steam's predictive bar. Needs `input_method_v2` and a corpus.
  Deferred.

### 1.2 From Maliit (Plasma Mobile, PinePhone)

**What we take**

- **Floating, anchored layout.** Maliit attaches to the bottom and
  reserves an exclusive zone so the focused app reflows. We do the
  same via `wlr-layer-shell`'s `set_exclusive_zone`.
- **Per-key "hold for alt-character" hint.** Maliit shows a small
  superscript on each key indicating what it produces with shift.
  Cheap to copy, helps users massively. Implementation: secondary
  glyph in the top-right corner of each key, rendered at 60% size.
- **Layer indicator.** Tiny pill in the corner showing which layer
  you're on (alpha / digits / symbols / emoji). Maliit does this
  cleanly. We render it in the same atlas pass as the keys.
- **Soft entrance animation.** Slides up from the bottom edge with a
  120 ms ease-out. Feels gentler than a hard pop.

**What we don't take**

- Maliit's swipe gestures. We're driven by sticks, not fingers.
- Maliit's GTK/Qt theming pipeline. We use our own JSON themes.
- Maliit's word prediction / spellcheck. Deferred.

---

## 2. Visual structure

```
┌──────────────────────────────────── output ────────────────────────────────────┐
│                                                                                │
│                  (the focused app — terminal, browser, etc.)                   │
│                                                                                │
│                                                                                │
│  ┌───────────────────── thumbboard surface ────────────────────────────────┐    │
│  │  [layer pill]                                          [output glyph]  │    │
│  │  ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐           │    │
│  │  │ q │ │ w │ │ e │ │ r │ │ t │ │ y │ │ u │ │ i │ │ o │ │ p │           │    │
│  │  └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘           │    │
│  │   ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐                │    │
│  │   │ a │ │ s │ │ d │ │ f │ │ g │ │ h │ │ j │ │ k │ │ l │                │    │
│  │   └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘                │    │
│  │  ┌─────┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌─────┐             │    │
│  │  │SHIFT│ │ z │ │ x │ │ c │ │ v │ │ b │ │ n │ │ m │ │ <-- │             │    │
│  │  └─────┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └───┘ └─────┘             │    │
│  │  ┌────┐ ┌────┐ ┌────────────────────────────┐ ┌────┐ ┌────┐            │    │
│  │  │ABC │ │ ,  │ │           SPACE            │ │ .  │ │ ↵  │            │    │
│  │  └────┘ └────┘ └────────────────────────────┘ └────┘ └────┘            │    │
│  └────────────────────────────────────────────────────────────────────────┘    │
└────────────────────────────────────────────────────────────────────────────────┘
```

- The **surface itself** is anchored to the bottom edge, full output
  width, ~40% of output height (configurable).
- The **layer pill** in the top-left shows current layer
  (`abc` / `123` / `!#$`).
- The **output glyph** in the top-right is a tiny indicator of what
  controller is driving us (xbox / sony / generic / nothing).
- The **focused key** is drawn with a glow ring; it's the cursor.

---

## 3. Animation system

### 3.1 Why we need one

A keyboard with no animations looks broken. The press needs to *feel*
like something happened. Real input devices have mechanical
feedback — for a virtual one, motion *is* the feedback.

We have to animate at least:

- **Entrance / exit** — the keyboard slides up when shown, slides
  down when hidden.
- **Focus glow** — when the cursor lands on a new key, the previous
  key's glow fades out and the new one's fades in.
- **Press squish** — the focused key briefly shrinks and bounces back
  on commit.
- **Layer transitions** — when you flip from `abc` to `123`, glyphs
  cross-fade.

That's already four overlapping per-key animations. We need a real
system, not ad-hoc timers.

### 3.2 Architecture

The animation system is a small, opinionated tween library tailored to
our use case. It lives in `src/render/animation.{hpp,cpp}` and looks
like this conceptually:

```cpp
// src/render/animation.hpp  (sketch only - not final code)

namespace thumbboard::render {

enum class Easing {
    Linear,
    EaseOutCubic,    // entrance slide
    EaseOutBack,     // press squish bounce-back
    EaseInOutQuad,   // layer cross-fade
};

class Tween {
public:
    Tween(float from, float to, float duration_s, Easing curve);
    float sample(float t_s) const;     // t = seconds since start
    bool  done(float t_s) const;
};

// One-shot tweens, indexed by what they animate.
class AnimationSystem {
public:
    void update(float dt_s);

    // Per-key animated values.
    float focus_glow(KeyId k) const;     // 0..1, brightness multiplier
    float press_scale(KeyId k) const;    // 0.92..1.0, multiply key size
    float layer_alpha(LayerId l) const;  // 0..1, current layer opacity

    // Per-surface animated values.
    float entrance_offset() const;       // 0 = visible, 1 = fully hidden

    // Triggers - called from KeyboardState transitions.
    void on_focus_changed(KeyId from, KeyId to);
    void on_key_committed(KeyId k);
    void on_layer_changed(LayerId from, LayerId to);
    void on_show();
    void on_hide();
};

} // namespace thumbboard::render
```

The system holds a small `std::vector<ActiveTween>`, ticks them every
frame, and prunes finished ones. There's no scene graph, no
declarative DSL. Forty animations on screen at once is a generous
upper bound; this is fine.

### 3.3 The frame loop

Once per frame:

```
1. AnimationSystem::update(dt_s)        — advance all tweens
2. For each visible key:
     scale = AnimationSystem::press_scale(key.id)
     glow  = AnimationSystem::focus_glow(key.id)
     draw key with (scale, glow)
3. eglSwapBuffers()
4. wl_surface_frame() callback registers wakeup for next frame
```

We don't render when nothing is animating *and* no input has changed.
That means an idle keyboard sitting on screen is using zero CPU and
zero GPU. The frame loop only ticks when:

- a tween is in progress, **or**
- input has moved the cursor, **or**
- the compositor sent a configure event, **or**
- the layer changed.

This is critical for battery life on handhelds.

### 3.4 GPU effects

Three shaders cover everything in v1.0:

1. **`key.vert` / `key.frag`** — draws a rounded-rect key with a flat
   fill and a 1 px highlight on the top edge (Steam-style). The vertex
   shader applies the per-key `press_scale` from the animation system.
2. **`glow.frag`** — additive pass for the focus ring. Reads the key's
   bounding box, computes a soft falloff, multiplies by `focus_glow`.
3. **`text.frag`** — samples the FreeType-generated glyph atlas. Uses
   alpha-blended SDF (signed distance field) text so glyphs scale
   nicely during press-squish.

Background blur (M6) adds:

4. **`blur.frag`** — two-pass Gaussian (separable horizontal + vertical)
   over the background hint. Only enabled if the compositor supports
   surface backdrop hints. Otherwise we fall back to a flat tint.

---

## 4. Theming

Themes are JSON files in `data/themes/*.json`. They define a fixed set
of *tokens*; the renderer reads tokens, never hex codes from C++.

```json
{
    "name": "Default",
    "color": {
        "background":     "#0E1721E0",
        "key":            "#1A2533",
        "key_pressed":    "#26384D",
        "key_focus":      "#1F2C3D",
        "glyph":          "#E6EDF3",
        "glyph_alt":      "#7E8C9F",
        "accent":         "#1A9FFF",
        "accent_glow":    "#1A9FFF80"
    },
    "geometry": {
        "key_radius_px":  12,
        "key_gap_px":      8,
        "key_top_highlight_alpha": 0.08
    },
    "animation": {
        "entrance_ms":            120,
        "focus_glow_in_ms":        90,
        "focus_glow_out_ms":      140,
        "press_squish_ms":         80,
        "press_release_ms":       120,
        "layer_crossfade_ms":     180
    },
    "font": {
        "family": "Inter",
        "size_px": 24,
        "weight": "Medium"
    }
}
```

The canonical theme that ships with v1.0 is **Default** —
`data/themes/default.json`, with `"name": "Default"`. It mixes the
Steam-style focus glow and rounded-rect key vocabulary from §1.1
with the calmer, less-saturated palette ideas from Maliit, so the
out-of-the-box look is neutral and lives well on top of any
wallpaper or app.

Additional themes ship as optional examples / starting points users
can copy and edit:

- `steam.json` — the unmodified Steam aesthetic from §1.1, brighter
  accent, more contrast.
- `maliit.json` — lighter, flatter, the Plasma Mobile look.
- `highcontrast.json` — accessibility preset (see §5).

Users drop their own JSON in `~/.config/thumbboard/themes/`. A theme
is selected by its `name` field, not its filename, e.g.
`theme = "Default"` in config.toml.

---

## 5. Accessibility considerations

- **High-contrast token preset.** A `theme = "highcontrast"` swap that
  bumps focus-ring brightness to white, key fills to #000, glyphs to
  #FFF.
- **No color-only signaling.** The focused key has a glow *and* a 2 px
  outline. The pressed key squishes *and* changes color. Nothing is
  conveyed by hue alone — important for color-blind users and for low
  brightness OLEDs.
- **Configurable animation speed**, including a `0` setting that turns
  all tweens off for users with vestibular sensitivity.

---

## 6. Roadmap fit

Animation work is intentionally back-loaded. See
[ROADMAP.md](ROADMAP.md):

- M2 — static keyboard rendered, no animation.
- M4 — focus ring (no animation, just a hard outline).
- M6 — full animation system. **This is when GUI.md §3 lands.**
- M7 — theming pipeline. **This is when GUI.md §4 lands.**

Building a beautiful animated keyboard is the second half of the
project, not the first half. Get keystrokes into Firefox before you
make them pretty.
