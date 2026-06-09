#version 450


layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexcoord;
layout (location = 3) in ivec4 aJoints;
layout (location = 4) in vec4 aWeights;


uniform vec3 u_color;
uniform mat4 u_view_projection;
uniform mat4 u_model_matrix;
uniform vec3 u_light_position;
uniform mat4 u_joint_matrices[64];

out vec3 vertex_normal;
out vec3 light_dir;
out vec4 vertex_position;
out vec2 tex_coord;

void main() {

    vertex_normal = aNormal;
    mat4 skinMatrix = aWeights.x * u_joint_matrices[aJoints.x]
        + aWeights.y * u_joint_matrices[aJoints.y]
        + aWeights.z * u_joint_matrices[aJoints.z]
        + aWeights.w * u_joint_matrices[aJoints.w];

    vec4 vertex_pos = u_view_projection * u_model_matrix * skinMatrix * vec4(aPosition, 1.0);
    vertex_position = vertex_pos;

    tex_coord = aTexcoord;
    light_dir = normalize(aPosition - u_light_position);
    gl_Position = vertex_position;
}
