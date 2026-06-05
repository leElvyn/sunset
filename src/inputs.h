//
// Created by red on 4/29/26.
//

#ifndef SUNSET_INPUTS_H
#define SUNSET_INPUTS_H


#include "camera.h"
#include <GLFW/glfw3.h>


void processInput(GLFWwindow *window, Camera &camera, float delta_time);

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn);

#endif //SUNSET_INPUTS_H
