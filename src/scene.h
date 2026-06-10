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
    program* flare_prog   = nullptr;
    GLuint   flare_vao    = 0;
    std::vector<Object> objects;

    // ── Logo plein écran (ex. titre TLogo) ───────────────────────────────────
    program*  logo_prog   = nullptr;
    GLuint    logo_vao    = 0;
    GLuint    logo_tex    = 0;
    int       logo_w      = 0, logo_h = 0;       // taille image (aspect)
    float     logo_alpha  = 0.0f;                // opacité, pilotée par image
    glm::vec2 logo_center = {0.0f, 0.35f};       // centre NDC
    float     logo_scale  = 0.35f;               // demi-hauteur NDC

    bool add_object(const char* path,
                    glm::mat4 transform = glm::mat4(1.0f),
                    int anim_index = 0);

    void init_lens_flare(program* prog);
    void init_logo(program* prog, const char* png_path); // charge le PNG
    void draw(glm::mat4 view_projection, glm::vec3 light_dir, glm::vec3 light_color, float time);
};
