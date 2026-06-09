#pragma once

#include <vector>
#include <glm/mat4x4.hpp>
#include "glad/glad.h"
#include "objects.h"

struct Scene {
    std::vector<Object> objects;

    bool add_object(const char* path,
                    glm::mat4 transform = glm::mat4(1.0f),
                    int anim_index = 0);

    void draw(GLint model_matrix_loc, GLint joint_matrices_loc, float time);
};
