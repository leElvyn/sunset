#pragma once

#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include "glad/glad.h"
#include "objects.h"
#include "shaders.h"

struct Scene {
    program* skinned_prog = nullptr;
    program* static_prog  = nullptr;
    std::vector<Object> objects;

    bool add_object(const char* path,
                    glm::mat4 transform = glm::mat4(1.0f),
                    int anim_index = 0);

    void draw(glm::mat4 view_projection, glm::vec3 light_pos, float time);
};
