#version 450

layout(location=0) out vec4 out_color;
in vec2 uv; // NDC [-1,1]

void main() {
    float t = uv.y * 0.5 + 0.5; // [0 = bottom, 1 = top]

    vec3 horizon = vec3(1.00, 0.55, 0.02); // deep orange at horizon
    vec3 mid     = vec3(0.98, 0.72, 0.05); // bright golden yellow
    vec3 zenith  = vec3(0.85, 0.40, 0.03); // dark burnt orange at top

    vec3 sky = mix(horizon, mid,    smoothstep(0.0,  0.55, t));
    sky      = mix(sky,     zenith, smoothstep(0.50, 1.0,  t));

    // Bright glow band right at the horizon
    float glow = exp(-abs(t - 0.20) * 12.0);
    sky += glow * 0.35 * vec3(1.0, 0.85, 0.3);

    out_color = vec4(sky, 1.0);
}
