#version 450

uniform sampler2D u_tex;
uniform float     u_alpha;   // opacité globale (fondu)

in vec2 uv;
layout(location = 0) out vec4 out_color;

void main() {
    vec4 t = texture(u_tex, uv);
    float a = t.a * u_alpha;
    if (a <= 0.0) discard;
    out_color = vec4(t.rgb, a);   // alpha-blend classique (SRC_ALPHA, 1-SRC_ALPHA)
}
