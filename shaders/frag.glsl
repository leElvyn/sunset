#version 450

uniform sampler2D texture_sampler;
uniform sampler2D u_shadow_map;
uniform vec3      u_light_color;

layout(location=0) out vec4 output_color;

in vec3 vertex_normal;
in vec3 light_dir;
in vec2 tex_coord;
in vec4 v_light_space_pos;

// PCF 3x3 — returns 1.0 = fully lit, 0.0 = fully shadowed
float shadow_factor() {
    vec3 proj = v_light_space_pos.xyz / v_light_space_pos.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0) return 1.0;

    // Slope-scale bias to reduce acne on grazing surfaces
    float bias = max(0.005 * (1.0 - dot(normalize(vertex_normal), normalize(light_dir))), 0.001);

    float shadow = 0.0;
    vec2 texel = 1.0 / textureSize(u_shadow_map, 0);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            float closest = texture(u_shadow_map, proj.xy + vec2(x, y) * texel).r;
            shadow += (proj.z - bias > closest) ? 0.0 : 1.0;
        }
    }
    return shadow / 9.0;
}

void main() {
    float diffuse = max(dot(normalize(vertex_normal), normalize(light_dir)), 0.0);
    float ambient = 0.4;

    float shadow = shadow_factor();
    vec3 ambient_color = vec3(1.0, 0.80, 0.45);
    vec3 light = ambient * ambient_color + (1.0 - ambient) * diffuse * u_light_color * shadow;

    vec4 tex_color = texture(texture_sampler, tex_coord);
    if (tex_color.a < 0.7)
        discard;

    output_color = vec4(tex_color.rgb * light, tex_color.a);
}
