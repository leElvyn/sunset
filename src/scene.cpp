#include "scene.h"

#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "animations.h"
#include "models.h"

bool Scene::add_object(const char* path, glm::mat4 transform, int anim_index) {
    Object obj;
    if (!loadModel(obj.raw_model, path))
        return false;

    obj.gl_model    = bindModel(obj.raw_model);
    obj.animations  = parse_animations(obj.raw_model);
    obj.model_matrix = transform;
    obj.anim_index  = anim_index;

    objects.push_back(std::move(obj));
    return true;
}

void Scene::draw(glm::mat4 view_projection, glm::vec3 light_pos, float time) {
    for (auto& obj : objects) {
        bool skinned = !obj.raw_model.skins.empty();
        program* prog = skinned ? skinned_prog : static_prog;
        if (!prog) continue;

        glUseProgram(prog->prog_id);

        GLint vp_loc    = glGetUniformLocation(prog->prog_id, "u_view_projection");
        GLint light_loc = glGetUniformLocation(prog->prog_id, "u_light_position");
        GLint mm_loc    = glGetUniformLocation(prog->prog_id, "u_model_matrix");

        glUniformMatrix4fv(vp_loc, 1, GL_FALSE, glm::value_ptr(view_projection));
        glUniform3fv(light_loc, 1, glm::value_ptr(light_pos));
        glUniformMatrix4fv(mm_loc, 1, GL_FALSE, glm::value_ptr(obj.model_matrix));

        if (skinned) {
            GLint jm_loc = glGetUniformLocation(prog->prog_id, "u_joint_matrices");
            Animation* anim = nullptr;
            if (!obj.animations.empty()) {
                int idx = obj.anim_index < (int)obj.animations.size()
                              ? obj.anim_index : 0;
                anim = &obj.animations[idx];
            }
            process_animations(obj.raw_model, anim, time, jm_loc);
        }

        drawModel(obj.gl_model);
    }
}
