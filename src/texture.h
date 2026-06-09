//
// Created by red on 4/30/26.
//

#ifndef SUNSET_TEXTURE_H
#define SUNSET_TEXTURE_H
#include <iostream>
#include <string>
#include <vector>

#include "glad/glad.h"
#include "tinygltf.h"
#include "stb_image.h"


unsigned int loadCubemap(std::vector<std::string> faces);
GLuint uploadTexture(const tinygltf::Image& img, int samplerIdx, const tinygltf::Model& model);

GLuint extractTextures(const tinygltf::Primitive& prim, const tinygltf::Model& model);

#endif //SUNSET_TEXTURE_H
