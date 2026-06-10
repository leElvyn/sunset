#include "animations.h"

#include <iostream>
#include <glm/common.hpp>
#include <glm/vec3.hpp>
#include <glm/detail/type_quat.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "tinygltf.h"
#include "glad/glad.h"


std::vector<Animation> parse_animations(const tinygltf::Model& model) {
    std::vector<Animation> animations;

    int i = 0;
    for (auto& anim : model.animations) {
        Animation a;
        a.name = anim.name;
        std::cout << "name: " << anim.name << " id : " << i << std::endl;

        i++;

        for (auto& channel : anim.channels) {
            AnimationChannel ch;
            ch.nodeIndex = channel.target_node;
            ch.path      = channel.target_path;

            auto& sampler = anim.samplers[channel.sampler];

            auto& inputAcc  = model.accessors[sampler.input];
            auto& inputView = model.bufferViews[inputAcc.bufferView];
            auto& inputBuf  = model.buffers[inputView.buffer];
            const float* times = reinterpret_cast<const float*>(
                inputBuf.data.data() + inputView.byteOffset + inputAcc.byteOffset);
            ch.sampler.times.assign(times, times + inputAcc.count);

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
            if (!ch.sampler.times.empty())
                a.duration = std::max(a.duration, ch.sampler.times.back());
            a.channels.push_back(ch);
        }
        animations.push_back(a);
    }
    return animations;
}

static glm::vec4 sample_channel(const AnimationSampler& sampler, float t) {
    auto& times  = sampler.times;
    auto& values = sampler.values;

    if (t <= times.front()) return values.front();
    if (t >= times.back())  return values.back();

    int next = 0;
    while (next < (int)times.size() && times[next] <= t) next++;
    int prev = next - 1;

    float factor = (t - times[prev]) / (times[next] - times[prev]);

    if (sampler.interpolation == "STEP")
        return values[prev];

    // LINEAR et CUBICSPLINE (fallback linéaire pour cubicspline)
    return glm::mix(values[prev], values[next], factor);
}

// Applique les channels d'une animation sur des transforms existants.
// Seuls les os ciblés par l'animation sont modifiés — les autres sont inchangés.
static void apply_animation(
    std::vector<NodeTransform>& transforms,
    const Animation& animation,
    float time_t)
{
    for (auto& channel : animation.channels) {
        glm::vec4 val = sample_channel(channel.sampler, time_t);
        NodeTransform& nt = transforms[channel.nodeIndex];

        if (channel.path == "rotation")
            nt.rotation = glm::normalize(glm::quat(val.w, val.x, val.y, val.z));
        else if (channel.path == "translation")
            nt.translation = glm::vec3(val);
        else if (channel.path == "scale")
            nt.scale = glm::vec3(val);
    }
}

static std::vector<NodeTransform> compute_node_transforms(
    const tinygltf::Model& model,
    Animation* animation,
    float time_t)
{
    std::vector<NodeTransform> transforms(model.nodes.size());

    for (size_t i = 0; i < model.nodes.size(); i++) {
        auto& node = model.nodes[i];
        auto& nt   = transforms[i];
        if (!node.translation.empty())
            nt.translation = {(float)node.translation[0], (float)node.translation[1], (float)node.translation[2]};
        if (!node.rotation.empty())
            nt.rotation = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
        if (!node.scale.empty())
            nt.scale = {(float)node.scale[0], (float)node.scale[1], (float)node.scale[2]};
    }

    if (animation)
        apply_animation(transforms, *animation, time_t);

    return transforms;
}

static void blend_transforms(
    std::vector<NodeTransform>& a,
    const std::vector<NodeTransform>& b,
    float factor)
{
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        a[i].translation = glm::mix(a[i].translation, b[i].translation, factor);
        a[i].scale       = glm::mix(a[i].scale,       b[i].scale,       factor);

        glm::quat qa = a[i].rotation;
        glm::quat qb = b[i].rotation;
        if (glm::dot(qa, qb) < 0.0f) qb = -qb;
        a[i].rotation = glm::normalize(glm::slerp(qa, qb, factor));
    }
}

static void compute_global_transforms(
    const tinygltf::Model& model,
    int nodeIdx,
    const glm::mat4& parentMat,
    const std::vector<NodeTransform>& local,
    std::vector<glm::mat4>& global)
{
    auto& nt = local[nodeIdx];
    glm::mat4 m = glm::translate(glm::mat4(1.0f), nt.translation)
                * glm::mat4_cast(nt.rotation)
                * glm::scale(glm::mat4(1.0f), nt.scale);

    global[nodeIdx] = parentMat * m;

    for (int child : model.nodes[nodeIdx].children)
        compute_global_transforms(model, child, global[nodeIdx], local, global);
}

// Applies a layer animation with masked overwrite: a channel is written only if
// its value meaningfully deviates from the rest pose.  This correctly handles
// animations exported with "static" channels for every bone — those channels
// read as rest-pose values and are skipped, so the base animation is preserved
// for any bone the layer doesn't actually animate.
static void apply_animation_layer(
    std::vector<NodeTransform>& transforms,
    const std::vector<NodeTransform>& rest,
    const Animation& animation,
    float time_t)
{
    for (auto& channel : animation.channels) {
        glm::vec4 val = sample_channel(channel.sampler, time_t);
        NodeTransform& nt       = transforms[channel.nodeIndex];
        const NodeTransform& rt = rest[channel.nodeIndex];

        if (channel.path == "rotation") {
            glm::quat q = glm::normalize(glm::quat(val.w, val.x, val.y, val.z));
            if (glm::abs(glm::dot(q, rt.rotation)) > 0.f) continue;
            nt.rotation = q;
        } else if (channel.path == "translation") {
            glm::vec3 t = glm::vec3(val);
            if (glm::length(t - rt.translation) < 1e-4f) continue;
            nt.translation = t;
        } else if (channel.path == "scale") {
            glm::vec3 s = glm::vec3(val);
            if (glm::length(s - rt.scale) < 1e-4f) continue;
            nt.scale = s;
        }
    }
}

static void upload_joint_matrices(
    const tinygltf::Model& model,
    const std::vector<NodeTransform>& transforms,
    GLint uniforms)
{
    auto& skin = model.skins[0];

    std::vector<glm::mat4> global(model.nodes.size(), glm::mat4(1.0f));
    for (int root : model.scenes[0].nodes)
        compute_global_transforms(model, root, glm::mat4(1.0f), transforms, global);

    auto& acc  = model.accessors[skin.inverseBindMatrices];
    auto& view = model.bufferViews[acc.bufferView];
    auto& buf  = model.buffers[view.buffer];
    const glm::mat4* ibms = reinterpret_cast<const glm::mat4*>(
        buf.data.data() + view.byteOffset + acc.byteOffset);

    std::vector<glm::mat4> joints(skin.joints.size());
    for (size_t i = 0; i < skin.joints.size(); i++)
        joints[i] = global[skin.joints[i]] * ibms[i];

    glUniformMatrix4fv(uniforms, (GLsizei)joints.size(), GL_FALSE,
                       glm::value_ptr(joints[0]));
}

void process_animations(const tinygltf::Model& model,
                        Animation* anim_a, float time_a,
                        Animation* anim_b, float time_b, float blend,
                        const std::vector<AnimLayer>& layers,
                        GLint uniforms)
{
    if (model.skins.empty()) return;

    auto transforms = compute_node_transforms(model, anim_a, time_a);

    if (anim_b && blend > 0.0f) {
        auto transforms_b = compute_node_transforms(model, anim_b, time_b);
        blend_transforms(transforms, transforms_b, blend);
    }

    if (!layers.empty()) {
        auto rest = compute_node_transforms(model, nullptr, 0.0f);
        for (auto& layer : layers)
            if (layer.anim) apply_animation_layer(transforms, rest, *layer.anim, layer.time);
    }

    upload_joint_matrices(model, transforms, uniforms);
}
