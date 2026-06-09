#version 450

uniform sampler2D texture_sampler;
uniform vec3 u_light_color;

layout(location=0) out vec4 output_color;

in vec3 vertex_normal;
in vec3 light_dir;
in vec2 tex_coord;

void main() {
    float diffuse = max(dot(normalize(vertex_normal), normalize(light_dir)), 0.0);
    float ambient = 0.25;

    vec3 ambient_color = vec3(0.55, 0.7, 1.0); // cool blue shadow
    vec3 light = ambient * ambient_color + (1.0 - ambient) * diffuse * u_light_color;

    vec4 tex_color = texture(texture_sampler, tex_coord);
    if (tex_color.a < 0.1)
        discard;

    output_color = vec4(tex_color.rgb * light, tex_color.a);
}
