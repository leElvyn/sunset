#version 450

// Quad écran piloté par gl_VertexID (triangle strip, 4 sommets) — pas de VBO.
uniform vec2 u_center;    // centre du quad en NDC [-1,1]
uniform vec2 u_halfsize;  // demi-taille en NDC (corrigée de l'aspect côté CPU)

out vec2 uv;

void main() {
    // gl_VertexID 0..3 -> (0,0)(1,0)(0,1)(1,1)
    vec2 c = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));
    uv = vec2(c.x, 1.0 - c.y);          // V inversé : stb_image charge top-down
    vec2 p = u_center + (c * 2.0 - 1.0) * u_halfsize;
    gl_Position = vec4(p, 0.0, 1.0);
}
