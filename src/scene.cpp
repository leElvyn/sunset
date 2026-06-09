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

void Scene::draw(GLint model_matrix_loc, GLint joint_matrices_loc, float time) {
    for (auto& obj : objects) {
        glUniformMatrix4fv(model_matrix_loc, 1, GL_FALSE,
                           glm::value_ptr(obj.model_matrix));

        if (!obj.raw_model.skins.empty()) {
            Animation* anim = nullptr;
            if (!obj.animations.empty()) {
                int idx = obj.anim_index < (int)obj.animations.size()
                              ? obj.anim_index : 0;
                anim = &obj.animations[idx];
            }
            process_animations(obj.raw_model, anim, time, joint_matrices_loc);
        }

        drawModel(obj.gl_model);
    }
}
