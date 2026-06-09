#version 450

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexcoord;
layout (location = 3) in ivec4 aJoints;
layout (location = 4) in vec4 aWeights;

uniform mat4 u_view_projection;
uniform mat4 u_model_matrix;
uniform vec3 u_light_direction; // normalized, points FROM scene TOWARD light
uniform mat4 u_joint_matrices[64];

out vec3 vertex_normal;
out vec3 light_dir;
out vec2 tex_coord;

void main() {
    mat4 skinMatrix = aWeights.x * u_joint_matrices[aJoints.x]
                    + aWeights.y * u_joint_matrices[aJoints.y]
                    + aWeights.z * u_joint_matrices[aJoints.z]
                    + aWeights.w * u_joint_matrices[aJoints.w];

    mat4 world_matrix = u_model_matrix * skinMatrix;
    vec3 world_pos    = vec3(world_matrix * vec4(aPosition, 1.0));

    // Normal matrix: inverse transpose of the world matrix (handles non-uniform scale)
    vertex_normal = normalize(mat3(transpose(inverse(world_matrix))) * aNormal);

    light_dir = u_light_direction; // same for every vertex — directional light
    tex_coord = aTexcoord;

    gl_Position = u_view_projection * vec4(world_pos, 1.0);
}
