#version 450

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexcoord;

uniform mat4 u_view_projection;
uniform mat4 u_model_matrix;
uniform vec3 u_light_direction;
uniform mat4 u_light_space_matrix;

out vec3 vertex_normal;
out vec3 light_dir;
out vec2 tex_coord;
out vec4 v_light_space_pos;

void main() {
    vec3 world_pos    = vec3(u_model_matrix * vec4(aPosition, 1.0));

    vertex_normal     = normalize(mat3(transpose(inverse(u_model_matrix))) * aNormal);
    light_dir         = u_light_direction;
    tex_coord         = aTexcoord;
    v_light_space_pos = u_light_space_matrix * vec4(world_pos, 1.0);

    gl_Position = u_view_projection * vec4(world_pos, 1.0);
}
