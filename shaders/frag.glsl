#version 450

uniform sampler2D texture_sampler;
//uniform sampler2D lighting_sampler;
//uniform sampler2D normalmap_sampler;

layout(location=0) out vec4 output_color;

in vec4 vertex_color;
in vec3 light_dir;
in vec4 vertex_position;
in vec3 vertex_normal;
in vec2 tex_coord;

void main() {
    float light_intensity = clamp((dot(vertex_normal, normalize(light_dir))), 0, 1);
    output_color = texture(texture_sampler, tex_coord);
    if (output_color.a < 0.1)
        discard;          // fragment is thrown away entirely
//    output_color = vec4(1, 0,0,1);
}
