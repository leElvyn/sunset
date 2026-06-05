//
// Created by red on 5/29/26.
//

#include "models.h"
#include <iostream>

#include "texture.h"
#include "ext/tiny_gltf.h"

#define BUFFER_OFFSET(i) ((char *)NULL + (i))


bool loadModel(tinygltf::Model &model, const char *filename) {
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool res = loader.LoadASCIIFromFile(&model, &err, &warn, filename);
    if (!warn.empty()) {
        std::cout << "WARN: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cout << "ERR: " << err << std::endl;
    }

    if (!res)
        std::cout << "Failed to load glTF: " << filename << std::endl;
    else
        std::cout << "Loaded glTF: " << filename << std::endl;

    return res;
}


GLPrimitive bindPrimitive(
    tinygltf::Model& model,
    tinygltf::Primitive& primitive)
{
    GLPrimitive result;

    glGenVertexArrays(1, &result.vao);
    glBindVertexArray(result.vao);

    // Upload all buffer views
    std::unordered_map<int, GLuint> gpuBuffers;

    for (size_t i = 0; i < model.bufferViews.size(); ++i) {
        const auto& bufferView = model.bufferViews[i];

        if (bufferView.target == 0)
            continue;

        const auto& buffer =
            model.buffers[bufferView.buffer];

        GLuint bufferId;
        glGenBuffers(1, &bufferId);

        gpuBuffers[(int)i] = bufferId;
        result.buffers.push_back(bufferId);

        glBindBuffer(bufferView.target, bufferId);

        glBufferData(
            bufferView.target,
            bufferView.byteLength,
            buffer.data.data() + bufferView.byteOffset,
            GL_STATIC_DRAW
        );
    }

    // Vertex attributes
    for (auto& attrib : primitive.attributes) {

        const auto& accessor =
            model.accessors[attrib.second];

        const auto& bufferView =
            model.bufferViews[accessor.bufferView];

        glBindBuffer(
            GL_ARRAY_BUFFER,
            gpuBuffers[accessor.bufferView]
        );

        GLint location = -1;

        if (attrib.first == "POSITION")
            location = 0;
        else if (attrib.first == "NORMAL")
            location = 1;
        else if (attrib.first == "TEXCOORD_0")
            location = 2;

        if (location < 0)
            continue;

        const GLint componentCount =
            accessor.type == TINYGLTF_TYPE_SCALAR
                ? 1
                : accessor.type;

        const GLsizei stride =
            accessor.ByteStride(bufferView);

        glEnableVertexAttribArray(location);

        glVertexAttribPointer(
            location,
            componentCount,
            accessor.componentType,
            accessor.normalized,
            stride,
            reinterpret_cast<void*>(
                accessor.byteOffset
            )
        );
    }

    // Index buffer
    if (primitive.indices >= 0) {

        const auto& accessor =
            model.accessors[primitive.indices];

        result.indexCount =
            static_cast<GLsizei>(accessor.count);

        result.indexType =
            accessor.componentType;

        result.drawMode =
            primitive.mode;

        result.ebo =
            gpuBuffers[accessor.bufferView];

        glBindBuffer(
            GL_ELEMENT_ARRAY_BUFFER,
            result.ebo
        );
    }

    extractTextures(primitive, model);

    glBindVertexArray(0);

    return result;
}

GLModel bindModel(tinygltf::Model& model)
{
    GLModel gpuModel;

    for (auto& mesh : model.meshes) {
        for (auto& primitive : mesh.primitives) {

            gpuModel.primitives.push_back(
                bindPrimitive(model, primitive)
            );
        }
    }

    return gpuModel;
}

void drawMesh(const std::map<int, GLuint> &vbos,
              tinygltf::Model &model, tinygltf::Mesh &mesh) {
    for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        tinygltf::Primitive primitive = mesh.primitives[i];
        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));

        glDrawElements(primitive.mode, indexAccessor.count,
                       indexAccessor.componentType,
                       BUFFER_OFFSET(indexAccessor.byteOffset));
    }
}

// recursively draw node and children nodes of model
void drawModelNodes(const std::pair<GLuint, std::map<int, GLuint> > &vaoAndEbos,
                    tinygltf::Model &model, tinygltf::Node &node) {
    if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
        drawMesh(vaoAndEbos.second, model, model.meshes[node.mesh]);
    }
    for (size_t i = 0; i < node.children.size(); i++) {
        drawModelNodes(vaoAndEbos, model, model.nodes[node.children[i]]);
    }
}

void drawModel(const GLModel& model)
{
    for (const auto& primitive
         : model.primitives)
    {
        glBindVertexArray(primitive.vao);

        glDrawElements(
            primitive.drawMode,
            primitive.indexCount,
            primitive.indexType,
            nullptr
        );
    }

    glBindVertexArray(0);
}

void dbgModel(tinygltf::Model &model) {
    for (auto &mesh: model.meshes) {
        std::cout << "mesh : " << mesh.name << std::endl;
        for (auto &primitive: mesh.primitives) {
            const tinygltf::Accessor &indexAccessor =
                    model.accessors[primitive.indices];

            std::cout << "indexaccessor: count " << indexAccessor.count << ", type "
                    << indexAccessor.componentType << std::endl;

            tinygltf::Material &mat = model.materials[primitive.material];
            for (auto &mats: mat.values) {
                std::cout << "mat : " << mats.first.c_str() << std::endl;
            }

            for (auto &image: model.images) {
                std::cout << "image name : " << image.uri << std::endl;
                std::cout << "  size : " << image.image.size() << std::endl;
                std::cout << "  w/h : " << image.width << "/" << image.height
                        << std::endl;
            }

            std::cout << "indices : " << primitive.indices << std::endl;
            std::cout << "mode     : "
                    << "(" << primitive.mode << ")" << std::endl;

            for (auto &attrib: primitive.attributes) {
                std::cout << "attribute : " << attrib.first.c_str() << std::endl;
            }
        }
    }
}
