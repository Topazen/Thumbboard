# ADR 0006 — Internationalization: multi-layout xkb keymaps + UTF-8 layout JSON

- **Status:** Accepted
- **Date:** 2026-04-26
- **Deciders:** Maksym
- **Supersedes:** the "US-only keymap" caveat in [ADR 0001 §Cons](0001-wayland-protocol.md)

## Context

We need to type more than US English. Two languages are explicitly
in scope: **Czech** (with `č`, `š`, `ž`, `ů`, `á`, `é`, `í`, `ó`,
`ú`, `ý` and capitals) and **Russian / Ukrainian** (Cyrillic). Other
European languages with diacritics (German `äöüß`, French `àéèç`,
Polish `ąćęłńóśźż`, etc.) should fall out of the same design with
no architecture changes — only data changes.

The problem decomposes into four sub-problems, each with its own
technology:

1. **What keysym does the compositor receive when I press a key?**
   The wire format of `input_method_v2` is xkb keysyms plus a
   serialized xkb keymap. So this is an xkb-keymap question.
2. **What glyph do I draw on the on-screen key?** This is a font
   atlas + Unicode question.
3. **How do I switch between alphabets at runtime?** UI/UX
   question — answered by the layer system that already exists for
   `abc / 123 / !#$`.
4. **How do I produce `č` when the user wants the dead-key /
   compose path?** xkb has built-in dead-key and Compose mechanisms
   we can lean on.

## Decision

Four pieces, one per sub-problem:

### Piece 1 — One multi-layout xkb keymap, indexed by group

We build *one* xkb keymap at startup that contains every layout the
user has enabled, as separate **groups** (xkb's term for
"alphabets"). Switching alphabet is `xkb_state_update_mask` with a
new group index — the same way physical keyboards do `Ctrl+Shift`
layout switching.

The `XkbContext` class accepts a list of layout names from config:

```toml
[i18n]
layouts = ["us", "cz", "ru"]
default = "us"
```

It uses `xkb_keymap_new_from_names` with the `layout =
"us,cz,ru"` rules, then serializes the result for
`input_method_v2`'s `keymap` request.

### Piece 2 — Per-layout JSON files in `data/layouts/`

Each layout gets its own JSON describing the visual on-screen
arrangement *for that alphabet*. Czech layouts put `ě` where `=`
sits on US, etc.

```
data/layouts/
├── us.json
├── cz.json          # Czech QWERTZ
├── cz-qwerty.json   # Czech QWERTY
├── ru.json          # Russian ЙЦУКЕН
├── ua.json          # Ukrainian
├── de.json          # German QWERTZ
└── pl.json          # Polish QWERTY
```

The JSON references xkb keysyms by name (`U010D` for `č`, `Cyrillic_a`
for `а`). At runtime, `Layout::resolve(KeyId)` looks up the keysym
in the active xkb group and emits it through `VirtualKeyboard`.
The renderer reads the *label* field from JSON for the visible glyph.

### Piece 3 — Layout switching is just another layer transition

The user's existing layer switch (shoulder buttons or a key) cycles
through `[us:abc] → [us:123] → [us:!#$] → [cz:abc] → [cz:123] →
…`. Visually it's the same cross-fade as the layer change. Under
the hood, switching to a layout in a different alphabet calls
`xkb_state_update_mask` to change the group, then loads a different
`Layout` JSON.

### Piece 4 — Diacritics via xkb's dead-key + Compose

For accented letters that aren't on a primary key we lean on xkb's
existing mechanisms instead of reinventing them:

- **Dead keys.** Press the dead-acute key, then `e`, get `é`. xkb
  handles this entirely on the compositor side once we send the
  right keysyms (`dead_acute` followed by `e`). Our keyboard just
  needs visible dead-key keys on the right layouts.
- **Compose sequences.** xkb's Compose file (`/usr/share/X11/locale/
  en_US.UTF-8/Compose` and friends) defines sequences like `Compose
  c v → č`. If the user has `XCOMPOSEFILE` set, the compositor
  honors it. We put a dedicated `Compose` key on layouts that need
  it.
- **Long-press popup (deferred to post-v1.0).** Nicer UX where
  holding `c` shows `č ć ç` as alternates. Worth doing eventually,
  not v1.0. Cell-snap cursor + a small popup is a clean addition.

### Piece 5 — Glyph atlas covers what's used

`FontAtlas` ([GUI.md §3](../GUI.md)) pre-rasterizes the union of
glyphs across all enabled layouts at startup. With three layouts
(US + CZ + RU) that's roughly 200 glyphs — fits in a 1024² atlas
easily with room for italic/bold variants later. The font we ship
must cover Latin Extended-A and Cyrillic. **Inter** does, as does
**Noto Sans**.

## Considered alternatives

### Option A — All four pieces above (chosen)

**Pros**

- Reuses the platform's xkb implementation. Years of dead-key /
  compose / locale work, free.
- Clean data/code separation: adding a new language is dropping a
  JSON file, no recompile.
- Multi-language users can have all their alphabets active
  simultaneously and toggle between them.

**Cons**

- xkb is not the friendliest API. Multi-group keymaps are an
  intermediate-level xkb topic.
- We have to ensure the bundled font has full coverage of every
  alphabet we ship.

### Option B — One layout at a time, restart to switch language

The simple version. User sets `layout = "cz"` in TOML and that's
the only language the daemon knows.

**Pros**: easy to implement.

**Cons**: real users mix languages — typing English URLs while
chatting in Russian. Requiring a daemon restart kills the UX.

### Option C — Roll our own keysym → byte sequence mapping

Bypass xkb groups; just send raw UTF-8 wherever possible.

**Cons**: `input_method_v2` doesn't accept UTF-8; it accepts
xkb keycodes against an xkb keymap. We can't bypass xkb without
also bypassing the protocol. Rejected.

### Option D — Switch to `input-method-v2` and use its
`commit_string` text path

`input-method-v2` lets you commit literal UTF-8 strings, sidestepping
xkb keymap juggling.

**Pros**: simpler text path.

**Cons**: That's [a whole separate ADR (0001)](0001-wayland-protocol.md)
that we explicitly deferred. Doing it just for i18n is a tail
wagging the dog.

## Consequences

- `XkbContext` becomes one of the more complex classes in the
  codebase. We allocate a multi-group keymap once at startup or on
  config reload.
- Config grows an `[i18n]` section listing enabled layouts and the
  default.
- `data/layouts/` ships at least three layouts in v1.0: `us`, `cz`,
  `ru`. German and Ukrainian are stretch goals — same architecture,
  more JSON.
- The bundled (or default-recommended) font is **Inter** with Noto
  Sans as the explicitly-supported fallback for users who want
  scripts beyond Latin + Cyrillic.
- The README adds a "Supported layouts" table.
- We document the JSON layout schema in [ARCHITECTURE.md](../ARCHITECTURE.md)
  and ship a `tools/layout-validator/` (post-v1.0) so contributors
  can submit new layouts confidently.

## Layout JSON schema (v1)

```json
{
  "name": "cz-qwertz",
  "display_name": "Czech (QWERTZ)",
  "xkb_layout": "cz",
  "xkb_variant": "",
  "compose_key": true,
  "rows": [
    {
      "y": 0,
      "keys": [
        {"x": 0,  "w": 1, "label": "ě", "alt": "2", "keysym": "ecaron"},
        {"x": 1,  "w": 1, "label": "š", "alt": "3", "keysym": "scaron"},
        {"x": 2,  "w": 1, "label": "č", "alt": "4", "keysym": "ccaron"},
        {"x": 3,  "w": 1, "label": "ř", "alt": "5", "keysym": "rcaron"},
        {"x": 4,  "w": 1, "label": "ž", "alt": "6", "keysym": "zcaron"},
        {"x": 5,  "w": 1, "label": "ý", "alt": "7", "keysym": "yacute"},
        {"x": 6,  "w": 1, "label": "á", "alt": "8", "keysym": "aacute"},
        {"x": 7,  "w": 1, "label": "í", "alt": "9", "keysym": "iacute"},
        {"x": 8,  "w": 1, "label": "é", "alt": "0", "keysym": "eacute"}
      ]
    }
  ]
}
```

`keysym` values are the canonical xkb keysym names (see
`xkbcommon-keysyms.h`). `label` is the UTF-8 string we render.
`alt` is the shifted glyph drawn small in the corner. The renderer
never inspects `keysym`; only `XkbContext` does.

## How we'll know this was right

- A user with `layouts = ["us", "cz"]` types `Hello, ahoj!` by
  hitting the layer switch once mid-sentence and sees both
  alphabets render correctly in the focused app.
- A new layout (e.g., Polish) ships as a single JSON pull request
  with no C++ changes.

## How we'll know this was wrong

- Users want preedit / suggestion popups for accented characters
  badly enough that dead-keys feel archaic. That's the
  `input-method-v2` migration trigger from ADR 0001.
- xkb's multi-group switching has bugs we trip over on real
  compositors. Mitigation: we own the keymap entirely; we can
  fall back to one keymap per active layout and rebind on switch.
