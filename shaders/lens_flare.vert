#version 450

out vec2 uv;

void main() {
    // Fullscreen triangle — no VBO needed, driven by gl_VertexID
    float x = (gl_VertexID == 1) ? 3.0 : -1.0;
    float y = (gl_VertexID == 2) ? 3.0 : -1.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
    uv = vec2(x, y);
}
