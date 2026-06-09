#version 450

uniform vec2  u_light_ndc;     // light position in NDC [-1,1]
uniform float u_flare_strength;
uniform float u_aspect;        // width / height

layout(location=0) out vec4 out_color;
in vec2 uv;

// Aspect-corrected distance
float adist(vec2 p, vec2 c) {
    float dx = (p.x - c.x) / u_aspect;
    float dy =  p.y - c.y;
    return sqrt(dx*dx + dy*dy);
}

// Gaussian glow — soft, for the main sun halo
float glow(vec2 p, vec2 c, float r) {
    float d = adist(p, c);
    return exp(-d * d * 4.0 / (r * r));
}

// Quadratic disc — slightly harder edge, for ghost flares
float disc(vec2 p, vec2 c, float r) {
    float t = 1.0 - adist(p, c) / r;
    return max(0.0, t) * max(0.0, t);
}

void main() {
    if (u_flare_strength <= 0.0) discard;

    // Axis goes from the light toward (and past) the screen center
    vec2 axis = -u_light_ndc;

    vec3 color = vec3(0.0);

    // ── Sun glow ──────────────────────────────────────────────────────────────
    color += glow(uv, u_light_ndc, 0.40) * 0.35 * vec3(1.0, 0.75, 0.4);
    color += glow(uv, u_light_ndc, 0.10) * 1.20 * vec3(1.0, 0.95, 0.85);

    // ── Anamorphic horizontal streak ──────────────────────────────────────────
    float sy = (uv.y - u_light_ndc.y) * u_aspect * 22.0;
    float sx = abs(uv.x - u_light_ndc.x) * 0.5;
    color += exp(-sy * sy) * exp(-sx) * 0.25 * vec3(0.55, 0.72, 1.0);

    // ── Ghost flares along the flare axis ─────────────────────────────────────
    // Small warm orange
    color += disc(uv, u_light_ndc + 0.20 * axis, 0.030) * 0.80 * vec3(1.0,  0.6,  0.2);
    // Medium blue-purple
    color += disc(uv, u_light_ndc + 0.42 * axis, 0.060) * 0.45 * vec3(0.6,  0.5,  1.0);
    // Thin ring on the blue ghost
    float ring_d = adist(uv, u_light_ndc + 0.42 * axis);
    color += (1.0 - smoothstep(0.0, 0.012, abs(ring_d - 0.063))) * 0.25 * vec3(0.3, 0.9, 1.0);
    // Tiny green
    color += disc(uv, u_light_ndc + 0.62 * axis, 0.018) * 0.65 * vec3(0.3,  1.0,  0.5);
    // Medium blue
    color += disc(uv, u_light_ndc + 0.88 * axis, 0.075) * 0.35 * vec3(0.45, 0.55, 1.0);
    // Small gold
    color += disc(uv, u_light_ndc + 1.18 * axis, 0.038) * 0.45 * vec3(1.0,  0.8,  0.25);
    // Large faint blue, far past screen center
    color += disc(uv, u_light_ndc + 1.52 * axis, 0.110) * 0.18 * vec3(0.4,  0.6,  1.0);

    float lum = max(max(color.r, color.g), color.b);
    if (lum < 0.001) discard;

    // Additive blend: output rgb, alpha unused (GL_ONE, GL_ONE)
    out_color = vec4(color * u_flare_strength, 1.0);
}
