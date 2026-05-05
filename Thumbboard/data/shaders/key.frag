#version 300 es
// Rounded-rect key fill with a faint top highlight (Steam-style).
// Uses a 2D box SDF to produce sub-pixel corner antialiasing.
// Cursor key gets a flat 2 px outline drawn inside its border.
// Shift key gets a tinted fill when shift is armed or locked.

precision mediump float;

in vec2 v_local;       // -1..+1 across the quad
in vec2 v_size_px;     // key dimensions in pixels
flat in int v_key_idx;

uniform vec4  u_fill;
uniform float u_radius_px;
uniform float u_top_highlight_alpha;
uniform int   u_cursor_key_idx;    // -1 = no cursor
uniform vec4  u_outline_color;     // cursor outline RGBA
uniform int   u_shift_key_idx;     // -1 = no shift-key highlight
uniform vec4  u_shift_fill;        // fill used when shift key is active

out vec4 frag_color;

float box_sdf(vec2 p, vec2 half_extent, float r) {
    vec2 q = abs(p) - (half_extent - vec2(r));
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec2 half_size = v_size_px * 0.5;
    vec2 p = v_local * half_size;

    float d = box_sdf(p, half_size, u_radius_px);
    float alpha = 1.0 - smoothstep(-1.0, 0.0, d);
    if (alpha <= 0.0) {
        discard;
    }

    // Swap in the shift fill when this key is the shift key and shift is active.
    vec4 fill = (v_key_idx == u_shift_key_idx && u_shift_key_idx >= 0) ? u_shift_fill : u_fill;

    // Top 35% of the key gets a soft additive highlight on the top edge.
    float t = clamp((-v_local.y - 0.30) / 0.70, 0.0, 1.0);
    vec3 rgb = fill.rgb + vec3(u_top_highlight_alpha) * t;

    // Cursor key: replace the inner 2-3 px ring with the outline colour.
    if (v_key_idx == u_cursor_key_idx && u_cursor_key_idx >= 0 && d > -3.0) {
        rgb = u_outline_color.rgb;
    }

    frag_color = vec4(rgb, fill.a * alpha);
}
