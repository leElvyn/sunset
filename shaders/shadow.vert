#version 450

layout(location=0) in vec3 aPosition;
layout(location=3) in ivec4 aJoints;
layout(location=4) in vec4  aWeights;

uniform mat4 u_light_space_matrix;
uniform mat4 u_model_matrix;
uniform mat4 u_joint_matrices[64];

void main() {
    mat4 skinMatrix = aWeights.x * u_joint_matrices[aJoints.x]
                    + aWeights.y * u_joint_matrices[aJoints.y]
                    + aWeights.z * u_joint_matrices[aJoints.z]
                    + aWeights.w * u_joint_matrices[aJoints.w];
    gl_Position = u_light_space_matrix * u_model_matrix * skinMatrix * vec4(aPosition, 1.0);
}
