#include "scene.h"

#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <iostream>

#include "animations.h"
#include "models.h"
#include "texture.h"   // stb_image (stbi_load)

void Scene::init_lens_flare(program* prog) {
    flare_prog = prog;
    glGenVertexArrays(1, &flare_vao);
}

void Scene::init_logo(program* prog, const char* png_path) {
    logo_prog = prog;
    glGenVertexArrays(1, &logo_vao); // VAO vide : sommets générés par gl_VertexID

    int channels = 0;
    stbi_set_flip_vertically_on_load(false); // on inverse V dans le shader
    unsigned char* data = stbi_load(png_path, &logo_w, &logo_h, &channels, 4);
    if (!data) {
        std::cout << "Logo: cannot load " << png_path << " (logo desactive)\n";
        logo_tex = 0;
        return;
    }

    glGenTextures(1, &logo_tex);
    glBindTexture(GL_TEXTURE_2D, logo_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, logo_w, logo_h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);
    std::cout << "Logo: " << png_path << " (" << logo_w << "x" << logo_h << ")\n";
}

bool Scene::add_object(const char* path, glm::mat4 transform, int anim_index) {
    Object obj;
    if (!loadModel(obj.raw_model, path))
        return false;

    obj.gl_model     = bindModel(obj.raw_model);
    obj.animations   = parse_animations(obj.raw_model);
    obj.model_matrix = transform;

    // Timeline par défaut : boucler sur l'animation demandée
    obj.timeline.then(anim_index, -1.0f, true);

    objects.push_back(std::move(obj));
    return true;
}

void Scene::draw(glm::mat4 view_projection, glm::vec3 light_dir, glm::vec3 light_color, float time) {
    for (auto& obj : objects) {
        bool skinned = !obj.raw_model.skins.empty();
        program* prog = skinned ? skinned_prog : static_prog;
        if (!prog) continue;

        glUseProgram(prog->prog_id);

        Animation* anim_a = nullptr;
        Animation* anim_b = nullptr;
        float time_a = time, time_b = 0.0f, blend = 0.0f;

        GLint vp_loc    = glGetUniformLocation(prog->prog_id, "u_view_projection");
        GLint light_loc = glGetUniformLocation(prog->prog_id, "u_light_direction");
        GLint lc_loc    = glGetUniformLocation(prog->prog_id, "u_light_color");
        GLint mm_loc    = glGetUniformLocation(prog->prog_id, "u_model_matrix");

        glUniformMatrix4fv(vp_loc, 1, GL_FALSE, glm::value_ptr(view_projection));
        glUniform3fv(light_loc, 1, glm::value_ptr(light_dir));
        glUniform3fv(lc_loc,    1, glm::value_ptr(light_color));
        glUniformMatrix4fv(mm_loc, 1, GL_FALSE, glm::value_ptr(obj.model_matrix));

        if (skinned) {
            GLint jm_loc = glGetUniformLocation(prog->prog_id, "u_joint_matrices");
            Animation* anim = nullptr;
            if (!obj.animations.empty()) {
                auto eval = obj.timeline.evaluate(obj.animations, time);
                anim_a = &obj.animations[eval.anim_index];
                time_a = eval.local_time;

                if (eval.next_anim_index >= 0) {
                    anim_b = &obj.animations[eval.next_anim_index];
                    time_b = eval.next_local_time;
                    blend  = eval.blend;
                }
            }
            std::vector<AnimLayer> layers;
            for (auto& tl : obj.extra_layers) {
              auto ev = tl.evaluate(obj.animations, time);
              if (ev.anim_index >= 0 && ev.anim_index < (int)obj.animations.size())
                layers.push_back({&obj.animations[ev.anim_index], ev.local_time});
            }
            process_animations(obj.raw_model,
                anim_a, time_a,
                anim_b, time_b, blend,
                layers,
                jm_loc);
        }

        drawModel(obj.gl_model);
    }

    // ── Lens flare — screen-space, drawn after all geometry ──────────────────
    if (flare_prog && flare_vao) {
        // Project a point very far in the light direction to get the sun's screen position
        glm::vec4 clip = view_projection * glm::vec4(light_dir * 1e6f, 1.0f);
        bool in_front  = clip.w > 0.0f;
        glm::vec2 ndc  = in_front ? glm::vec2(clip.x / clip.w, clip.y / clip.w)
                                  : glm::vec2(10.0f);

        float edge     = std::max(std::abs(ndc.x), std::abs(ndc.y));
        float strength = in_front ? std::max(0.0f, 1.0f - (edge - 0.6f) / 0.6f) : 0.0f;

        if (strength > 0.0f) {
            GLint viewport[4];
            glGetIntegerv(GL_VIEWPORT, viewport);
            float aspect = (float)viewport[2] / (float)viewport[3];

            glUseProgram(flare_prog->prog_id);
            glUniform2f(glGetUniformLocation(flare_prog->prog_id, "u_light_ndc"),
                        ndc.x, ndc.y);
            glUniform1f(glGetUniformLocation(flare_prog->prog_id, "u_flare_strength"),
                        strength);
            glUniform1f(glGetUniformLocation(flare_prog->prog_id, "u_aspect"),
                        aspect);

            glDepthMask(GL_FALSE);
            glDepthFunc(GL_ALWAYS);
            glBlendFunc(GL_ONE, GL_ONE);   // additive

            glBindVertexArray(flare_vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    // ── Logo en surimpression (fondu piloté par logo_alpha) ──────────────────
    if (logo_prog && logo_vao && logo_tex && logo_alpha > 0.0f) {
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        float screen_aspect = (float)viewport[2] / (float)viewport[3];
        float img_aspect    = (logo_h > 0) ? (float)logo_w / (float)logo_h : 1.0f;
        // demi-hauteur fixe ; largeur dérivée pour préserver l'aspect de l'image
        glm::vec2 half(logo_scale * img_aspect / screen_aspect, logo_scale);

        glUseProgram(logo_prog->prog_id);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, logo_tex);
        glUniform1i(glGetUniformLocation(logo_prog->prog_id, "u_tex"), 0);
        glUniform1f(glGetUniformLocation(logo_prog->prog_id, "u_alpha"),
                    std::min(1.0f, logo_alpha));
        glUniform2f(glGetUniformLocation(logo_prog->prog_id, "u_center"),
                    logo_center.x, logo_center.y);
        glUniform2f(glGetUniformLocation(logo_prog->prog_id, "u_halfsize"),
                    half.x, half.y);

        glDepthMask(GL_FALSE);
        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindVertexArray(logo_vao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }
}
