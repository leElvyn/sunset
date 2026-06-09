//
// Created by red on 6/8/26.
//

#ifndef BLACKRAY_ANIMATIONS_H
#define BLACKRAY_ANIMATIONS_H
#include <string>
#include <vector>
#include <glm/fwd.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/detail/type_quat.hpp>

#include "tinygltf.h"
#include "glad/glad.h"


struct NodeTransform {
    glm::vec3 translation = {0,0,0};
    glm::quat rotation    = {1,0,0,0};
    glm::vec3 scale       = {1,1,1};
};

struct AnimationSampler {
        std::vector<float> times;       // keyframe timestamps
        std::vector<glm::vec4> values;  // vec3 for T/S, vec4 for R (quaternion)
        std::string interpolation;      // "LINEAR", "STEP", "CUBICSPLINE"
    };

struct AnimationChannel {
    int nodeIndex;
    std::string path;           // "translation", "rotation", "scale"
    AnimationSampler sampler;
};

struct Animation {
    std::string name;
    std::vector<AnimationChannel> channels;
    float duration;
};

void process_animations(const tinygltf::Model& model, Animation* animation, float time_t, GLint uniforms);
std::vector<Animation> parse_animations(const tinygltf::Model& model);

#endif //BLACKRAY_ANIMATIONS_H
