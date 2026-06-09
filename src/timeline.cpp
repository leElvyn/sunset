#include "timeline.h"
#include <algorithm>
#include <cmath>

Timeline::Eval Timeline::evaluate(const std::vector<Animation>& anims, float time) const {
    if (clips.empty() || anims.empty())
        return {};

    float total = 0.0f;
    for (auto& clip : clips) {
        int idx = std::min(clip.anim_index, (int)anims.size() - 1);
        float dur = (clip.duration > 0.0f) ? clip.duration : anims[idx].duration;
        total += dur;
    }

    if (total <= 0.0f)
        return {};

    float t = loop_all ? fmodf(time, total) : std::min(time, total);

    for (int ci = 0; ci < (int)clips.size(); ci++) {
        auto& clip = clips[ci];
        int   idx  = std::min(clip.anim_index, (int)anims.size() - 1);
        float dur  = (clip.duration > 0.0f) ? clip.duration : anims[idx].duration;

        if (t >= dur) {
            t -= dur;
            continue;
        }

        float anim_dur  = anims[idx].duration;
        float local     = clip.loop ? fmodf(t, anim_dur) : std::min(t, anim_dur - 1e-4f);

        Eval eval;
        eval.anim_index = idx;
        eval.local_time = local;

        // Détection de la zone de blend : dernières blend_out secondes du clip
        bool has_next = loop_all || ci < (int)clips.size() - 1;
        if (clip.blend_out > 0.0f && has_next && (dur - t) <= clip.blend_out) {
            int   next_ci  = (ci + 1) % (int)clips.size();
            int   next_idx = std::min(clips[next_ci].anim_index, (int)anims.size() - 1);
            float time_in_blend = clip.blend_out - (dur - t); // 0 → blend_out

            eval.blend           = time_in_blend / clip.blend_out; // 0 → 1
            eval.next_anim_index = next_idx;
            eval.next_local_time = time_in_blend;
        }

        return eval;
    }

    // Fin de séquence (loop_all == false)
    auto& last = clips.back();
    int idx = std::min(last.anim_index, (int)anims.size() - 1);
    return {idx, anims[idx].duration - 1e-4f};
}
