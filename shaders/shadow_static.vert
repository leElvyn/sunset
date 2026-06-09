#version 450

layout(location=0) in vec3 aPosition;

uniform mat4 u_light_space_matrix;
uniform mat4 u_model_matrix;

void main() {
    gl_Position = u_light_space_matrix * u_model_matrix * vec4(aPosition, 1.0);
}
