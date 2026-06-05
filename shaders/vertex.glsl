#version 450


layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout(location = 2) in vec2 in_texcoord;

uniform vec3 u_color;
uniform mat4 u_mvp;
uniform vec3 u_light_position;

out vec3 vertex_normal;
out vec3 light_dir;
out vec4 vertex_position;
out vec2 tex_coord;

void main() {

    vertex_normal = normal;
    vec4 vertex_pos = u_mvp * vec4(position * vec3(1), 1.0);
    vertex_position = vertex_pos;

    tex_coord = in_texcoord;
    light_dir = normalize(position - u_light_position);
    gl_Position = vertex_position;
}
