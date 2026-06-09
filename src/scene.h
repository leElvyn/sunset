#pragma once

#include <format>
#include <string>
#include <vector>
#include <glm/vec3.hpp>

#include "glad/glad.h"

// ########## Scene data #############


void uploadScene(GLuint ssbos[]);

void updateUniforms(GLuint uNumSpheres, GLuint uNumLights);
