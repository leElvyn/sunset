//
// Created by red on 5/29/26.
//

#ifndef SUNSET_MODELS_H
#define SUNSET_MODELS_H

#include <glm/fwd.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include "glad/glad.h"
#include "ext/tiny_gltf.h"


struct GLPrimitive {
    GLuint vao = 0;
    GLuint ebo = 0;
    GLsizei indexCount = 0;
    GLenum indexType = GL_UNSIGNED_INT;
    GLenum drawMode = GL_TRIANGLES;
    GLuint texture = 0;

    std::vector<GLuint> buffers; // owns all VBO/EBOs
};

struct GLModel {
    std::vector<GLPrimitive> primitives;

    void destroy() {
        for (auto& p : primitives) {
            if (p.vao)
                glDeleteVertexArrays(1, &p.vao);

            if (!p.buffers.empty()) {
                glDeleteBuffers(
                    static_cast<GLsizei>(p.buffers.size()),
                    p.buffers.data()
                );
            }
        }

        primitives.clear();
    }
};

void drawModel(const GLModel& model);

void drawModelNodes(const std::pair<GLuint, std::map<int, GLuint>>& vaoAndEbos,
                    tinygltf::Model &model, tinygltf::Node &node);


void drawMesh(const std::map<int, GLuint>& vbos,
              tinygltf::Model &model, tinygltf::Mesh &mesh);


GLModel bindModel(tinygltf::Model& model);
void bindMesh(std::map<int, GLuint>& vbos,
              tinygltf::Model &model, tinygltf::Mesh &mesh);


bool loadModel(tinygltf::Model &model, const char *filename);

#endif //SUNSET_MODELS_H
