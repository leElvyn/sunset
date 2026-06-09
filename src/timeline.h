#pragma once
#include <vector>
#include "animations.h"

struct TimelineClip {
    int   anim_index = 0;
    float duration   = -1.0f; // -1 = durée naturelle de l'animation
    bool  loop       = false; // looper ce clip individuellement
    float blend_out  = 0.2f;  // durée du fondu vers le clip suivant (secondes)
};

struct Timeline {
    std::vector<TimelineClip> clips;
    bool loop_all = true;

    struct Eval {
        int   anim_index      = 0;
        float local_time      = 0.0f;
        int   next_anim_index = -1;   // -1 = pas de blend
        float next_local_time = 0.0f;
        float blend           = 0.0f; // 0 = 100% courant, 1 = 100% suivant
    };

    Eval evaluate(const std::vector<Animation>& anims, float time) const;

    Timeline& then(int anim_index, float duration = -1.0f,
                   bool loop = false, float blend_out = 0.2f) {
        clips.push_back({anim_index, duration, loop, blend_out});
        return *this;
    }
};
