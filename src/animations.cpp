//
// Created by red on 6/8/26.
//

#include "animations.h"

#include <iostream>
#include <glm/common.hpp>
#include <glm/fwd.hpp>
#include <glm/vec3.hpp>
#include <glm/detail/type_quat.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "tinygltf.h"
#include "glad/glad.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/io.hpp>


std::vector<Animation> parse_animations(const tinygltf::Model& model) {
    std::vector<Animation> animations;

    for (auto& anim : model.animations) {
        Animation a;
        a.name = anim.name;

        for (auto& channel : anim.channels) {
            AnimationChannel ch;
            ch.nodeIndex = channel.target_node;
            ch.path      = channel.target_path;

            auto& sampler = anim.samplers[channel.sampler];

            // --- Read input (times) ---
            auto& inputAcc = model.accessors[sampler.input];
            auto& inputView = model.bufferViews[inputAcc.bufferView];
            auto& inputBuf  = model.buffers[inputView.buffer];
            const float* times = reinterpret_cast<const float*>(
                inputBuf.data.data() + inputView.byteOffset + inputAcc.byteOffset);
            ch.sampler.times.assign(times, times + inputAcc.count);

            // --- Read output (values) ---
            auto& outputAcc  = model.accessors[sampler.output];
            auto& outputView = model.bufferViews[outputAcc.bufferView];
            auto& outputBuf  = model.buffers[outputView.buffer];
            const float* vals = reinterpret_cast<const float*>(
                outputBuf.data.data() + outputView.byteOffset + outputAcc.byteOffset);

            int components = (ch.path == "rotation") ? 4 : 3;
            for (int i = 0; i < outputAcc.count; i++) {
                glm::vec4 v(0.0f);
                for (int c = 0; c < components; c++)
                    v[c] = vals[i * components + c];
                ch.sampler.values.push_back(v);
            }

            ch.sampler.interpolation = sampler.interpolation;
            a.duration = std::max(a.duration, ch.sampler.times.back());
            a.channels.push_back(ch);
        }
        animations.push_back(a);
    }
    return animations;
}

glm::vec4 sample_channel(const AnimationSampler& sampler, float t, const std::string& path) {
    auto& times  = sampler.times;
    auto& values = sampler.values;

    if (t <= times.front()) return values.front();
    if (t >= times.back())  return values.back();

    int next = 0;
    while (next < times.size() && times[next] <= t) next++;
    int prev = next - 1;

    float delta  = times[next] - times[prev];
    float factor = (t - times[prev]) / delta;

    if (sampler.interpolation == "STEP") {
        return values[prev];
    }
    else if (sampler.interpolation == "LINEAR") {
        if (path == "rotation") {
            glm::quat q1 = glm::quat(values[prev].w, values[prev].x, values[prev].y, values[prev].z);
            glm::quat q2 = glm::quat(values[next].w, values[next].x, values[next].y, values[next].z);

            if (glm::dot(q1, q2) < 0.0f)
                q2 = -q2;

            glm::quat result = glm::normalize(glm::slerp(q1, q2, factor));
            return glm::vec4(result.x, result.y, result.z, result.w);
        }
        return glm::mix(values[prev], values[next], factor);
    }

    // CUBICSPLINE fallback
    return glm::mix(values[prev], values[next], factor);
}

void compute_global_transforms(const tinygltf::Model& model,
                              int nodeIdx,
                              const glm::mat4& parentMat,
                              const std::vector<NodeTransform>& localTransforms,
                              std::vector<glm::mat4>& globalMatrices)
{
    auto& nt = localTransforms[nodeIdx];

    glm::mat4 local = glm::translate(glm::mat4(1.0f), nt.translation)
                    * glm::mat4_cast(nt.rotation)
                    * glm::scale(glm::mat4(1.0f), nt.scale);

    globalMatrices[nodeIdx] = parentMat * local;

    for (int child : model.nodes[nodeIdx].children)
        compute_global_transforms(model, child, globalMatrices[nodeIdx], localTransforms, globalMatrices);
}



void process_animations(tinygltf::Model model, Animation& animation, float time_t, GLint uniforms) {
    float animTime = fmod(time_t, animation.duration);


    std::vector<NodeTransform> nodeTransforms(model.nodes.size());

    for (auto& channel : animation.channels) {
        glm::vec4 val = sample_channel(channel.sampler, animTime, channel.path);
        NodeTransform& nt = nodeTransforms[channel.nodeIndex];

        if      (channel.path == "translation") nt.translation = glm::vec3(val);
        else if (channel.path == "scale")       nt.scale       = glm::vec3(val);
        else if (channel.path == "rotation")    nt.rotation    = glm::quat(val.w, val.x, val.y, val.z);
    }

    std::vector<glm::mat4> globalMatrices(model.nodes.size(), glm::mat4(1.0f));

    for (int root : model.scenes[0].nodes)
        compute_global_transforms(model, root, glm::mat4(1.0f), nodeTransforms, globalMatrices);

    // For each skin
    auto& skin = model.skins[0];
    std::vector<glm::mat4> jointMatrices(skin.joints.size());

    auto& inputAcc = model.accessors[skin.inverseBindMatrices];
    auto& inputView = model.bufferViews[inputAcc.bufferView];
    auto& inputBuf  = model.buffers[inputView.buffer];

    const glm::mat4* ibms = reinterpret_cast<glm::mat4*>(
        inputBuf.data.data() + inputView.byteOffset + inputAcc.byteOffset);;


    for (int i = 0; i < skin.joints.size(); i++) {
        int jointNode = skin.joints[i];
        jointMatrices[i] = globalMatrices[jointNode] * ibms[i];
    }

    // Upload to shader
    glUniformMatrix4fv(uniforms, jointMatrices.size(), GL_FALSE,
                       glm::value_ptr(jointMatrices[0]));
}