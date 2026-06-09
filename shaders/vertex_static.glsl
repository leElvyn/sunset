#version 450


layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexcoord;


uniform vec3 u_color;
uniform mat4 u_view_projection;
uniform mat4 u_model_matrix;
uniform vec3 u_light_position;

out vec3 vertex_normal;
out vec3 light_dir;
out vec4 vertex_position;
out vec2 tex_coord;

void main() {
    vertex_normal = aNormal;
    vec4 vertex_pos = u_view_projection * u_model_matrix * vec4(aPosition, 1.0);
    vertex_position = vertex_pos;

    tex_coord = aTexcoord;
    light_dir = normalize(aPosition - u_light_position);
    gl_Position = vertex_position;
}
