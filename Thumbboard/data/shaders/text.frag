#version 300 es
// Sample an 8-bit alpha glyph atlas. Color comes from a uniform; the
// atlas only encodes coverage.

precision mediump float;

in vec2 v_uv;

uniform sampler2D u_atlas;
uniform vec4      u_color;

out vec4 frag_color;

void main() {
    float coverage = texture(u_atlas, v_uv).r;
    frag_color = vec4(u_color.rgb, u_color.a * coverage);
}
