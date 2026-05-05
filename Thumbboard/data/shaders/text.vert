#version 300 es
// Glyph quad — pixel-space position and atlas UV.

precision highp float;

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;

uniform vec2 u_resolution;

out vec2 v_uv;

void main() {
    vec2 ndc = (a_pos / u_resolution) * 2.0 - 1.0;
    ndc.y = -ndc.y;
    gl_Position = vec4(ndc, 0.0, 1.0);
    v_uv = a_uv;
}
