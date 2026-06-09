#include "scene.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>

#include "animations.h"
#include "models.h"

void Scene::init_lens_flare(program* prog) {
    flare_prog = prog;
    glGenVertexArrays(1, &flare_vao);
}

void Scene::init_shadow_map(program* skinned, program* stat, int size) {
    shadow_skinned_prog = skinned;
    shadow_static_prog  = stat;
    shadow_size         = size;

    glGenTextures(1, &shadow_map);
    glBindTexture(GL_TEXTURE_2D, shadow_map);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, size, size, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glGenFramebuffers(1, &shadow_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_map, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Scene::add_object(const char* path, glm::mat4 transform, int anim_index) {
    Object obj;
    if (!loadModel(obj.raw_model, path))
        return false;

    obj.gl_model     = bindModel(obj.raw_model);
    obj.animations   = parse_animations(obj.raw_model);
    obj.model_matrix = transform;

    obj.timeline.then(anim_index, -1.0f, true);

    objects.push_back(std::move(obj));
    return true;
}

// Evaluate and upload joint matrices for one object into the currently-bound program.
static void upload_object_joints(const Object& obj,
                                  GLint jm_loc,
                                  float time)
{
    if (obj.raw_model.skins.empty() || obj.animations.empty()) return;

    Animation* anim_a = nullptr, *anim_b = nullptr;
    float time_a = time, time_b = 0.0f, blend = 0.0f;

    auto eval = obj.timeline.evaluate(obj.animations, time);
    anim_a = const_cast<Animation*>(&obj.animations[eval.anim_index]);
    time_a = eval.local_time;
    if (eval.next_anim_index >= 0) {
        anim_b = const_cast<Animation*>(&obj.animations[eval.next_anim_index]);
        time_b = eval.next_local_time;
        blend  = eval.blend;
    }

    std::vector<AnimLayer> layers;
    for (auto& tl : obj.extra_layers) {
        auto ev = tl.evaluate(obj.animations, time);
        if (ev.anim_index >= 0 && ev.anim_index < (int)obj.animations.size())
            layers.push_back({const_cast<Animation*>(&obj.animations[ev.anim_index]), ev.local_time});
    }

    process_animations(obj.raw_model, anim_a, time_a, anim_b, time_b, blend, layers, jm_loc);
}

void Scene::draw(glm::mat4 view_projection, glm::vec3 light_dir, glm::vec3 light_color, float time) {
    // ── Light space matrix (orthographic sun projection) ─────────────────────
    glm::vec3 up = (glm::abs(light_dir.y) > 0.99f) ? glm::vec3(1,0,0) : glm::vec3(0,1,0);
    glm::mat4 light_view  = glm::lookAt(light_dir * 400.0f, glm::vec3(0.0f), up);
    glm::mat4 light_proj  = glm::ortho(-250.0f, 250.0f, -250.0f, 250.0f, 1.0f, 800.0f);
    glm::mat4 light_space = light_proj * light_view;

    // ── Shadow pass ──────────────────────────────────────────────────────────
    if (shadow_fbo && shadow_skinned_prog && shadow_static_prog) {
        GLint saved_vp[4];
        glGetIntegerv(GL_VIEWPORT, saved_vp);

        glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
        glViewport(0, 0, shadow_size, shadow_size);
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.0f, 4.0f);
        glDisable(GL_BLEND);

        for (auto& obj : objects) {
            bool skinned = !obj.raw_model.skins.empty();
            program* prog = skinned ? shadow_skinned_prog : shadow_static_prog;
            glUseProgram(prog->prog_id);

            glUniformMatrix4fv(glGetUniformLocation(prog->prog_id, "u_light_space_matrix"),
                               1, GL_FALSE, glm::value_ptr(light_space));
            glUniformMatrix4fv(glGetUniformLocation(prog->prog_id, "u_model_matrix"),
                               1, GL_FALSE, glm::value_ptr(obj.model_matrix));

            if (skinned)
                upload_object_joints(obj,
                    glGetUniformLocation(prog->prog_id, "u_joint_matrices"), time);

            drawModel(obj.gl_model);
        }

        glDisable(GL_POLYGON_OFFSET_FILL);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(saved_vp[0], saved_vp[1], saved_vp[2], saved_vp[3]);
    }

    // ── Sky gradient ─────────────────────────────────────────────────────────
    if (sky_prog && flare_vao) {
        glUseProgram(sky_prog->prog_id);
        glDepthMask(GL_FALSE);
        glDepthFunc(GL_ALWAYS);
        glDisable(GL_BLEND);

        glBindVertexArray(flare_vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // ── Bind shadow map to texture unit 1 for main pass ──────────────────────
    if (shadow_map) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadow_map);
        glActiveTexture(GL_TEXTURE0);
    }

    // ── Main pass ────────────────────────────────────────────────────────────
    for (auto& obj : objects) {
        bool skinned = !obj.raw_model.skins.empty();
        program* prog = skinned ? skinned_prog : static_prog;
        if (!prog) continue;

        glUseProgram(prog->prog_id);

        glUniformMatrix4fv(glGetUniformLocation(prog->prog_id, "u_view_projection"),
                           1, GL_FALSE, glm::value_ptr(view_projection));
        glUniform3fv(glGetUniformLocation(prog->prog_id, "u_light_direction"),
                     1, glm::value_ptr(light_dir));
        glUniform3fv(glGetUniformLocation(prog->prog_id, "u_light_color"),
                     1, glm::value_ptr(light_color));
        glUniformMatrix4fv(glGetUniformLocation(prog->prog_id, "u_model_matrix"),
                           1, GL_FALSE, glm::value_ptr(obj.model_matrix));
        glUniformMatrix4fv(glGetUniformLocation(prog->prog_id, "u_light_space_matrix"),
                           1, GL_FALSE, glm::value_ptr(light_space));
        glUniform1i(glGetUniformLocation(prog->prog_id, "u_shadow_map"), 1);

        if (skinned)
            upload_object_joints(obj,
                glGetUniformLocation(prog->prog_id, "u_joint_matrices"), time);

        drawModel(obj.gl_model);
    }

    // ── Lens flare ───────────────────────────────────────────────────────────
    if (flare_prog && flare_vao) {
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
            glUniform2f(glGetUniformLocation(flare_prog->prog_id, "u_light_ndc"),  ndc.x, ndc.y);
            glUniform1f(glGetUniformLocation(flare_prog->prog_id, "u_flare_strength"), strength);
            glUniform1f(glGetUniformLocation(flare_prog->prog_id, "u_aspect"),     aspect);

            glDepthMask(GL_FALSE);
            glDepthFunc(GL_ALWAYS);
            glBlendFunc(GL_ONE, GL_ONE);

            glBindVertexArray(flare_vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }
}
