#pragma once

#include <format>
#include <string>
#include <vector>
#include <glm/vec3.hpp>

#include "glad/glad.h"

// ########## Scene data #############

template<typename TCPU, typename TGPU>
void uploadSSBO(GLuint ssbo, const std::vector<TCPU>& objects) {
    std::vector<TGPU> gpuData;
    gpuData.reserve(objects.size());
    for (const auto& obj : objects)
        gpuData.push_back(toGPU(obj));

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 gpuData.size() * sizeof(TGPU),
                 gpuData.data(),
                 GL_DYNAMIC_DRAW);
}



void uploadScene(GLuint ssbos[]);

void updateUniforms(GLuint uNumSpheres, GLuint uNumLights);
