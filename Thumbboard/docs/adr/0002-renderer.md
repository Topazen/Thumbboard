# ADR 0002 — Render with OpenGL ES 3.0 over EGL

- **Status:** Accepted
- **Date:** 2026-04-26
- **Deciders:** Maksym

## Context

Thumbboard is a visible, animated overlay. We have to put pixels on the
screen, ~60 times a second, smoothly, while consuming as little CPU and
battery as possible (this might run on a Steam Deck-like handheld).

A Wayland client gets pixels onto a `wl_surface` in one of three ways:

1. **Shared-memory buffers (`wl_shm`)** — software rendering. The
   client mallocs a buffer, draws into it on the CPU (typically with
   Cairo or pixman), and hands it to the compositor.
2. **DMA-BUF / EGL** — hardware rendering. The client owns a GPU
   context (EGL), draws with OpenGL ES or Vulkan, and presents.
3. **Custom Vulkan + DMA-BUF** — same idea as 2 but with a steeper
   learning curve and a much bigger code-up-front cost.

A fourth option is to embed an existing toolkit (GTK, Qt) and let it
choose. We rejected toolkits in `ARCHITECTURE.md §2`.

## Decision

**Render with OpenGL ES 3.0, surfaced via EGL on the `wl_surface`.**

## Considered alternatives

### Option A — OpenGL ES 3.0 + EGL (chosen)

**Pros**

- Hardware-accelerated from day one. 60 fps is free; 144 fps is
  trivial. Animations are smooth without engineering effort.
- Negligible CPU cost. A typical frame is a handful of triangles, a
  font-atlas sample, and a glow shader. The GPU sleeps between frames.
- Industry standard for embedded/mobile/Wayland clients. Same skills
  apply to Android, mobile games, gfx jobs at Valve/AMD/Mesa.
- Mesa's GLES driver is everywhere. Works on Intel, AMD, NVIDIA,
  Raspberry Pi, Steam Deck.
- Forces us to learn shaders, vertex buffers, draw calls — directly
  transferable knowledge.

**Cons**

- Steeper learning curve than Cairo. The first triangle is harder than
  the first `cairo_fill()`.
- Boilerplate: EGL config selection, context creation, frame-callback
  scheduling. Roughly 200 LoC of one-time setup.
- Text rendering needs us to build a glyph atlas (FreeType +
  manual packing). This is a project unto itself.

### Option B — Cairo + pixman, software rendered into `wl_shm`

**Pros**

- 50 lines and you have a rectangle on the screen.
- Beautiful 2D primitives (gradients, rounded rects, text) for free.
  Designed exactly for UIs like ours.
- No GPU dependencies; works in headless / weird hardware.

**Cons**

- Animations are CPU-bound. A keyboard with 40 keys, each glowing and
  squishing on press, plus an entrance slide, is tens of millions of
  pixels redrawn per second. On low-power handhelds the fan spins up
  and the battery drains.
- Hits a wall when we want shader effects (glow, blur, distortion).
  We'd be reimplementing GPU effects on the CPU.
- Migration to GLES later is *not* a trivial refactor — every drawing
  call moves from Cairo to a GL primitive. Effectively a rewrite of
  `render/`.

### Option C — Cairo first, GLES later

This is the "ship now, refactor later" path. We rejected it because
the user explicitly wants animation polish, and "we'll migrate
later" historically means "we'll never migrate".

### Option D — Vulkan

Rejected as overkill. Vulkan's win is multi-threaded command
submission, which Thumbboard does not need. The boilerplate cost is
~3× GLES.

## Consequences

- We write our own shaders: `key.vert`, `key.frag`, `glow.frag`. They
  live in `data/shaders/` and are loaded at startup.
- We need a glyph atlas. v1.0 ships a single rasterized atlas built at
  startup from one font. M7 adds dynamic re-atlasing for non-Latin.
- The first 2 weeks (M0) feel slow because we're learning EGL. This is
  expected and budgeted.
- We get to put "OpenGL ES, GLSL shaders, EGL/Wayland integration" on
  the resume — high-signal for graphics/systems roles.

## How we'll know this was right

- A frame takes <0.5 ms of GPU time on integrated graphics.
- Adding a new animation (e.g. ripple on press) is a shader edit, not
  a re-architecture.

## How we'll know this was wrong

- We're still wrestling with EGL setup at the end of week 4 with no
  visible progress. If that happens, drop to Cairo for v0.1, accept
  the migration debt, and ship.
