#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <stdio.h>

#include "animations.h"
#include "camera.h"
#include "inputs.h"
#include "models.h"
#include "objects.h"
#include "shaders.h"
#include "texture.h"
#include "scene.h"


// ── Main ──────────────────────────────────────────────────────────────────────

// --- Initialization functions ---

#define TEST_OPENGL_ERROR()                                                             \
    do {									\
    GLenum err = glGetError();					                        \
    if (err != GL_NO_ERROR) std::cerr << "OpenGL ERROR!: " << err << " " << __LINE__ << std::endl;      \
    } while(0)


GLFWwindow *init_glfw() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *win = glfwCreateWindow(800, 600,
                                       "“A sword wields no strength unless the hand that holds it has courage.”", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(win, mouse_callback);
    return win;
}

void init_gl() {
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    // glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

struct Uniforms {
    GLint view_projection, color, light_position, res, joint_matrices, model_matrix;
};

Uniforms init_uniforms(GLuint prog) {
    glUseProgram(prog);
    Uniforms u;

    u.view_projection = glGetUniformLocation(prog, "u_view_projection");
    u.color = glGetUniformLocation(prog, "u_color");
    u.light_position = glGetUniformLocation(prog, "u_light_position");
    u.res = glGetUniformLocation(prog, "u_res");
    u.joint_matrices = glGetUniformLocation(prog, "u_joint_matrices");
    u.model_matrix = glGetUniformLocation(prog, "u_model_matrix");
    std::cout <<  glGetUniformLocation(prog, "texture_sampler") << std::endl;
    return u;
}

void set_uniforms(Uniforms u) {
    glUniform3f(u.light_position, 0, -20000, 0);
}

Camera init_camera(GLFWwindow *win, const Uniforms &u) {
    Camera camera({2.0f, 3.0f, 10.0f});
    return camera;
}

void update_camera(Camera& camera, Uniforms u, float w, float h) {
    glUniformMatrix4fv(u.view_projection, 1, false, glm::value_ptr(camera.get_mvp(w, h)));
}

GLsizei arrays_size = 0;

std::pair<GLuint, GLuint> init_object() {
    GLuint VBO, VAO;

    glGenBuffers(1, &VBO);
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    Cylinder cylinder(glm::vec3(0, 0, 0), 3, 4);

    auto vertices = cylinder.to_mesh(20);

    arrays_size += vertices.size() * 3;
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(triangle), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3* sizeof(float)));
    glEnableVertexAttribArray(1);

    return std::make_pair(VAO, VBO);
}
// --- Main loop ---

void render_loop(GLFWwindow *win,
                 const Uniforms &u, Camera &camera) {
    double time = glfwGetTime();

    Scene scene;
    //scene.add_object("res/models/zelda/Untitled.gltf",
    //                 glm::translate(glm::mat4(1.0f), glm::vec3(75.0f, 0.0f, 0.0f)), 2);
    scene.add_object("res/models/stage/Untitled.gltf",
                     glm::mat4(1.0f), 0);

    // Pour ajouter d'autres objets :
    std::cout << "added canon" << std::endl;

    while (!glfwWindowShouldClose(win)) {
        double new_time = glfwGetTime();
        double delta = new_time - time;
        time = new_time;

        glfwPollEvents();

        int w, h;
        glfwGetFramebufferSize(win, &w, &h);
        glViewport(0, 0, w, h);

        glUniform2f(u.res, (float) w, (float) h);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        scene.draw(u.model_matrix, u.joint_matrices, (float)time);

        glfwSwapBuffers(win);

        processInput(win, camera, delta);
        update_camera(camera, u, w, h);
    }
}

// --- Entry point ---

int main() {
    GLFWwindow *win = init_glfw();
    init_gl();
    program* prog = program::make_program("shaders/vertex.glsl", "shaders/frag.glsl");

    std::cout << prog->get_log();
    prog->use();

    Uniforms u = init_uniforms(prog->prog_id);
    set_uniforms(u);

    Camera camera = init_camera(win, u);

    glfwSetWindowUserPointer(win, &camera);

    render_loop(win, u, camera);
}

const char* __asan_default_options() { return "detect_leaks=0"; }
