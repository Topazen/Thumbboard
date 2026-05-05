#version 300 es
// Per-key rounded-rect fill — vertex stage.
// Input quads are pre-tessellated CPU-side; we only convert pixel coords
// to clip space and forward the local SDF coordinate.

precision highp float;

layout(location = 0) in vec2  a_pos;        // pixel position on surface
layout(location = 1) in vec2  a_local;      // -1..+1 across the key quad
layout(location = 2) in vec2  a_size_px;    // key width/height in pixels
layout(location = 3) in float a_key_idx;    // key index (packed as float)

uniform vec2 u_resolution;                  // surface size in pixels

out vec2 v_local;
out vec2 v_size_px;
flat out int v_key_idx;

void main() {
    vec2 ndc = (a_pos / u_resolution) * 2.0 - 1.0;
    ndc.y = -ndc.y;                         // GL bottom-up → screen top-down
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_local    = a_local;
    v_size_px  = a_size_px;
    v_key_idx  = int(a_key_idx);
}
