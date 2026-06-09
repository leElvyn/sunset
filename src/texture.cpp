//
// Created by red on 4/30/26.
//

#include "texture.h"


unsigned int loadCubemap(std::vector<std::string> faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data
            );
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap tex failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}
GLuint extractTextures(const tinygltf::Primitive& prim, const tinygltf::Model& model) {
    if (prim.material < 0) return -1; // primitive has no material
    const tinygltf::Material& mat = model.materials[prim.material];

    // 2. Access PBR textures
    const tinygltf::PbrMetallicRoughness& pbr = mat.pbrMetallicRoughness;
    if (pbr.baseColorTexture.index >= 0) {
        const tinygltf::Texture& tex = model.textures[pbr.baseColorTexture.index];
        const tinygltf::Image&   img = model.images[tex.source];

        for (int i = 3; i < img.image.size(); i+=4) {
        }
        // img.image    → std::vector<unsigned char>, raw RGBA pixels
        // img.width    → int
        // img.height   → int
        // img.component → int (number of channels, usually 4)
        // img.bits     → int (bits per channel, usually 8)

        return uploadTexture(img, tex.sampler, model); // see below
    }

}

GLuint uploadTexture(const tinygltf::Image& img, int samplerIdx, const tinygltf::Model& model) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    // --- Sampler / wrap / filter ---
    if (samplerIdx >= 0) {
        const tinygltf::Sampler& sampler = model.samplers[samplerIdx];

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
            sampler.wrapS != -1 ? sampler.wrapS : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
            sampler.wrapT != -1 ? sampler.wrapT : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            sampler.minFilter != -1 ? sampler.minFilter : GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
            sampler.magFilter != -1 ? sampler.magFilter : GL_LINEAR);
    } else {
        // glTF spec defaults
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,   GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,   GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // --- Determine format from channel count ---
    GLenum format = GL_RGBA;
    if      (img.component == 1) format = GL_RED;
    else if (img.component == 2) format = GL_RG;
    else if (img.component == 3) format = GL_RGB;
    else if (img.component == 4) format = GL_RGBA;

    // --- Upload pixels ---
    glTexImage2D(GL_TEXTURE_2D, 0, format,
                 img.width, img.height, 0,
                 format, GL_UNSIGNED_BYTE,
                 img.image.data());

    glGenerateMipmap(GL_TEXTURE_2D);

    return texID;
}