#include "stb.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>

// Format documenté dans JSystem/JStudio (stb-data.h, stb-data-parse.cpp,
// fvb-data.h, jstudio-object.cpp). Tout est big-endian.

namespace {

constexpr float kFps = 30.0f; // les "wait N frames" de la séquence sont à 30 fps

// ── Lecture big-endian ───────────────────────────────────────────────────────

uint16_t rd_u16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }
uint32_t rd_u32(const uint8_t* p) {
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
           (uint32_t)p[2] << 8  | (uint32_t)p[3];
}
float rd_f32(const uint8_t* p) {
    uint32_t u = rd_u32(p);
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
size_t align4(size_t n) { return (n + 3) & ~(size_t)3; }

// parseVariableUInt_16_32_following (JGadget/binary.cpp) :
// taille de contenu + type de paragraphe, sur 4 ou 8 octets.
bool varint(const uint8_t* d, size_t off, size_t end,
            uint32_t& size, uint32_t& type, size_t& data_off) {
    if (off + 4 > end)
        return false;
    uint16_t first = rd_u16(d + off);
    if (!(first & 0x8000)) {
        size     = first;
        type     = rd_u16(d + off + 2);
        data_off = off + 4;
        return true;
    }
    if (off + 8 > end)
        return false;
    size     = ((uint32_t)(first & 0x7FFF) << 16) | rd_u16(d + off + 2);
    type     = rd_u32(d + off + 4);
    data_off = off + 8;
    return true;
}

// ── Types de blocs / propriétés ──────────────────────────────────────────────

constexpr uint32_t BLK_JACT = 0x4A414354;
constexpr uint32_t BLK_JCMR = 0x4A434D52;
constexpr uint32_t BLK_JFVB = 0x4A465642;

// Acteur (TObject_actor::do_paragraph)
enum {
    PROP_TRANSLATION_X = 9,  PROP_TRANSLATION_Y = 10, PROP_TRANSLATION_Z = 11,
    PROP_TRANSLATION_XYZ = 12,
    PROP_ROTATION_X = 13,    PROP_ROTATION_Y = 14,    PROP_ROTATION_Z = 15,
    PROP_ROTATION_XYZ = 16,
    PROP_SCALING_X = 17,     PROP_SCALING_Y = 18,     PROP_SCALING_Z = 19,
    PROP_SCALING_XYZ = 20,
    PROP_ANIMATION = 58,
};

// Caméra (TObject_camera::do_paragraph)
enum {
    PROP_EYE_X = 21,    PROP_EYE_Y = 22,    PROP_EYE_Z = 23,    PROP_EYE_XYZ = 24,
    PROP_TARGET_X = 25, PROP_TARGET_Y = 26, PROP_TARGET_Z = 27, PROP_TARGET_XYZ = 28,
    PROP_ROLL = 38,     PROP_FOVY = 39,
};

// Opérations (para_type & 0x1F)
enum {
    OP_SET       = 0x02,
    OP_FVR_NAME  = 0x10,
    OP_IMMEDIATE = 0x11,
    OP_FVR_INDEX = 0x12,
    OP_REF_NAME  = 0x18,
    OP_REF_INDEX = 0x19,
};

// ── FVB embarqué (bloc JFVBFVB) ──────────────────────────────────────────────

void parse_fvb(const uint8_t* d, size_t start, size_t end,
               std::vector<FvbTrack>& tracks,
               std::map<std::string, int>& by_name) {
    if (start + 16 > end || std::memcmp(d + start, "FVB", 3) != 0)
        return;

    uint32_t block_count = rd_u32(d + start + 12);
    size_t   blk = start + 16;

    for (uint32_t i = 0; i < block_count && blk + 8 <= end; i++) {
        uint32_t blk_size = rd_u32(d + blk);
        uint16_t btype    = rd_u16(d + blk + 4);
        uint16_t id_size  = rd_u16(d + blk + 6);
        size_t   blk_end  = std::min(end, blk + blk_size);
        if (blk_size < 8)
            break;

        FvbTrack track;
        track.btype = btype;

        if (id_size && blk + 8 + id_size <= blk_end) {
            std::string name((const char*)d + blk + 8, id_size);
            name.resize(std::strlen(name.c_str()));
            if (!name.empty())
                by_name[name] = (int)tracks.size();
        }

        // Paragraphes du bloc FVB : 0x12 = range, 0x01 = données de clés.
        size_t p = blk + 8 + align4(id_size);
        uint32_t csize, ptype;
        size_t   data;
        while (p < blk_end && varint(d, p, blk_end, csize, ptype, data)) {
            p = data + align4(csize);
            if (ptype == 0)
                break;

            if (ptype == 0x12 && csize == 8) {
                track.range_end = rd_f32(d + data + 4);
            } else if (ptype == 0x16 && csize >= 4) { // interpolate enum
                track.interp = (int)rd_u32(d + data);
            } else if (ptype == 0x01 && csize >= 4) {
                uint32_t head = rd_u32(d + data);
                if (btype == 6) { // hermite : stride<<28 | count
                    int stride = (int)(head >> 28);
                    uint32_t count = head & 0x0FFFFFFF;
                    if ((stride == 3 || stride == 4) && count &&
                        4 + count * stride * 4 <= csize) {
                        track.stride = stride;
                        track.keys.reserve(count * stride);
                        for (uint32_t k = 0; k < count * stride; k++)
                            track.keys.push_back(rd_f32(d + data + 4 + k * 4));
                    }
                } else if (btype == 5) { // list_parameter : paires (param, val)
                    uint32_t count = head;
                    if (count && 4 + count * 8 <= csize) {
                        track.stride = 2;
                        track.keys.reserve(count * 2);
                        for (uint32_t k = 0; k < count * 2; k++)
                            track.keys.push_back(rd_f32(d + data + 4 + k * 4));
                    }
                }
            }
        }

        tracks.push_back(std::move(track));
        blk = blk + blk_size;
    }
}

// ── Bake d'une séquence JACT / JCMR ──────────────────────────────────────────

struct ChannelSet {
    CutsceneChannel* x = nullptr;
    CutsceneChannel* y = nullptr;
    CutsceneChannel* z = nullptr;
};

void bind_const(CutsceneChannel* ch, float t, float v) {
    if (ch)
        ch->segs.push_back({t, false, v, -1});
}

void bind_track(CutsceneChannel* ch, float t, int track) {
    if (ch)
        ch->segs.push_back({t, true, 0.0f, track});
}

struct Baker {
    Cutscene&  cs;
    const std::map<std::string, int>& fvb_by_name;

    CutsceneActor*  actor  = nullptr; // un seul des deux est non-nul
    CutsceneCamera* camera = nullptr;

    float t = 0.0f;          // temps cutscene courant (secondes)
    int   anim_index = -1;   // dernière ANIMATION vue
    float anim_start = 0.0f;

    // Canaux X/Y/Z visés par une propriété ; pour un scalaire seul x est rempli.
    ChannelSet channels(uint32_t prop) {
        if (camera) {
            switch (prop) {
            case PROP_EYE_X:      return {&camera->eye_x};
            case PROP_EYE_Y:      return {&camera->eye_y};
            case PROP_EYE_Z:      return {&camera->eye_z};
            case PROP_EYE_XYZ:    return {&camera->eye_x, &camera->eye_y, &camera->eye_z};
            case PROP_TARGET_X:   return {&camera->tgt_x};
            case PROP_TARGET_Y:   return {&camera->tgt_y};
            case PROP_TARGET_Z:   return {&camera->tgt_z};
            case PROP_TARGET_XYZ: return {&camera->tgt_x, &camera->tgt_y, &camera->tgt_z};
            case PROP_ROLL:       return {&camera->roll};
            case PROP_FOVY:       return {&camera->fovy};
            }
            return {};
        }
        switch (prop) {
        case PROP_TRANSLATION_X:   return {&actor->tx};
        case PROP_TRANSLATION_Y:   return {&actor->ty};
        case PROP_TRANSLATION_Z:   return {&actor->tz};
        case PROP_TRANSLATION_XYZ: return {&actor->tx, &actor->ty, &actor->tz};
        case PROP_ROTATION_X:      return {&actor->rx};
        case PROP_ROTATION_Y:      return {&actor->ry};
        case PROP_ROTATION_Z:      return {&actor->rz};
        case PROP_ROTATION_XYZ:    return {&actor->rx, &actor->ry, &actor->rz};
        case PROP_SCALING_X:       return {&actor->sx};
        case PROP_SCALING_Y:       return {&actor->sy};
        case PROP_SCALING_Z:       return {&actor->sz};
        case PROP_SCALING_XYZ:     return {&actor->sx, &actor->sy, &actor->sz};
        }
        return {};
    }

    void set_animation(int index) {
        if (!actor || index == anim_index)
            return;
        if (anim_index >= 0)
            actor->timeline.then(anim_index, t - anim_start, true, 0.2f);
        anim_index = index;
        anim_start = t;
    }

    void paragraph(const uint8_t* d, uint32_t ptype, size_t data, uint32_t csize) {
        if (ptype <= 0xFF)
            return; // réservés : flags, data blobs… rien à exécuter ici

        uint32_t prop = ptype >> 5;
        uint32_t op   = ptype & 0x1F;

        if (actor && prop == PROP_ANIMATION) {
            if (op == OP_REF_INDEX && csize >= 4)
                set_animation((int)rd_u32(d + data));
            return;
        }

        ChannelSet ch = channels(prop);
        if (!ch.x)
            return;
        CutsceneChannel* comps[3] = {ch.x, ch.y, ch.z};
        int n_comps = ch.z ? 3 : (ch.y ? 2 : 1);

        switch (op) {
        case OP_SET: // une f32 par composante
            if (csize >= (uint32_t)n_comps * 4)
                for (int i = 0; i < n_comps; i++)
                    bind_const(comps[i], t, rd_f32(d + data + i * 4));
            break;

        case OP_FVR_INDEX: // un u32 d'index FVB par composante
            if (csize >= (uint32_t)n_comps * 4)
                for (int i = 0; i < n_comps; i++)
                    bind_track(comps[i], t, (int)rd_u32(d + data + i * 4));
            break;

        case OP_FVR_NAME: { // noms null-terminés, stride égal
            if (n_comps == 0 || csize == 0)
                break;
            uint32_t stride = csize / n_comps;
            for (int i = 0; i < n_comps; i++) {
                std::string name((const char*)d + data + i * stride,
                                 std::min<size_t>(stride, csize - i * stride));
                name.resize(std::strlen(name.c_str()));
                auto it = fvb_by_name.find(name);
                if (it != fvb_by_name.end())
                    bind_track(comps[i], t, it->second);
            }
            break;
        }

        case OP_IMMEDIATE: { // hermite inline : ajoutée comme piste à part
            if (csize < 8)
                break;
            uint32_t head   = rd_u32(d + data);
            int      stride = (int)(head >> 28);
            uint32_t count  = head & 0x0FFFFFFF;
            if ((stride != 3 && stride != 4) || !count ||
                4 + count * stride * 4 > csize)
                break;
            FvbTrack track;
            track.btype  = 6;
            track.stride = stride;
            for (uint32_t k = 0; k < count * stride; k++)
                track.keys.push_back(rd_f32(d + data + 4 + k * 4));
            int idx = (int)cs.tracks.size();
            cs.tracks.push_back(std::move(track));
            bind_track(comps[0], t, idx);
            break;
        }

        default:
            break;
        }
    }

    // Interprète la séquence entière (process_sequence_ de stb.cpp JSystem).
    void run(const uint8_t* d, size_t off, size_t end, size_t blk_base) {
        int guard = 100000; // garde-fou contre les jumps en boucle
        while (off + 4 <= end && guard-- > 0) {
            uint32_t head  = rd_u32(d + off);
            uint32_t stype = head >> 24;
            uint32_t param = head & 0xFFFFFF;

            if (stype == 0)
                break;

            if (stype == 1) { // set_flag
                off += 4;
            } else if (stype == 2 || stype == 4) { // wait / suspend
                t += (float)param / kFps;
                off += 4;
            } else if (stype == 3) { // jump (offset signé 24 bits depuis le bloc)
                int32_t rel = (param & 0x800000) ? (int32_t)param - 0x1000000
                                                 : (int32_t)param;
                size_t target = blk_base + rel;
                if (target <= off) // saut arrière = boucle : on arrête le bake
                    break;
                off = target;
            } else if (stype == 0x80) { // bloc de paragraphes
                size_t p    = off + 4;
                size_t pend = std::min(end, p + param);
                uint32_t csize, ptype;
                size_t   data;
                while (p < pend && varint(d, p, pend, csize, ptype, data)) {
                    paragraph(d, ptype, data, csize);
                    p = data + align4(csize);
                }
                off = off + 4 + param;
            } else {
                off += 4;
            }
        }
    }
};

} // namespace

// ── FvbTrack ─────────────────────────────────────────────────────────────────
//
// Port fidèle de JStudio::TFunctionValue_list_parameter / _hermite
// (functionvalue.cpp). Les courbes type 5 (list_parameter) supportent quatre
// modes d'interpolation portés par le paragraphe 0x16 ; sans ce port, tout
// passe en linéaire et les mouvements n'ont aucun easing (mode 3 = B-spline,
// utilisé par quasiment toutes les pistes des cutscenes).

namespace {

// B-spline cubique non uniforme : interpolateValue_BSpline_nonuniform.
double bspline_nonuniform(double t, const double cp[4], const double knot[6]) {
    double k0 = knot[0], k1 = knot[1], k2 = knot[2];
    double k3 = knot[3], k4 = knot[4], k5 = knot[5];
    double d0 = t - k0, d1 = t - k1, d2 = t - k2;
    double d3 = k3 - t, d4 = k4 - t, d5 = k5 - t;
    double inv32 = 1.0 / (k3 - k2);
    double b3 = (d3 * inv32) / (k3 - k1);
    double b2 = (d2 * inv32) / (k4 - k2);
    double b1 = (d3 * b3) / (k3 - k0);
    double b4 = ((d1 * b3) + (d4 * b2)) / (k4 - k1);
    double b5 = (d2 * b2) / (k5 - k2);
    double term1 = d3 * b1;
    double term2 = (d0 * b1) + (d4 * b4);
    double term3 = (d1 * b4) + (d5 * b5);
    double term4 = d2 * b5;
    return term1 * cp[0] + term2 * cp[1] + term3 * cp[2] + term4 * cp[3];
}

} // namespace

float FvbTrack::evaluate(float t) const {
    if (keys.empty() || stride < 2)
        return 0.0f;

    size_t count = keys.size() / stride;
    auto key_t = [&](size_t i) { return (double)keys[i * stride]; };
    auto key_v = [&](size_t i) { return (double)keys[i * stride + 1]; };

    if (count == 1 || t <= key_t(0))
        return (float)key_v(0);
    if (t >= key_t(count - 1))
        return (float)key_v(count - 1);

    // i = borne supérieure (findUpperBound) : premier indice tel que t < t_i.
    size_t i = 0;
    while (i < count && key_t(i) <= t)
        i++;
    if (i == 0)
        return (float)key_v(0);
    if (i >= count)
        return (float)key_v(count - 1);

    double t0 = key_t(i - 1), t1 = key_t(i);
    double v0 = key_v(i - 1), v1 = key_v(i);

    // ── Hermite (btype 6) ────────────────────────────────────────────────────
    if (btype == 6) {
        double dt = t1 - t0;
        if (dt <= 0.0)
            return (float)v1;
        double u = (t - t0) / dt;
        double m0 = (stride == 4) ? keys[(i - 1) * stride + 3]
                                  : keys[(i - 1) * stride + 2]; // tangente sortante gauche
        double m1 = keys[i * stride + 2];                       // tangente entrante droite
        double u2 = u * u, u3 = u2 * u;
        return (float)((2*u3 - 3*u2 + 1) * v0 + (u3 - 2*u2 + u) * dt * m0 +
                       (-2*u3 + 3*u2)    * v1 + (u3 - u2)       * dt * m1);
    }

    // ── list_parameter (btype 5) ─────────────────────────────────────────────
    int mode = interp;
    if (mode == BSPLINE && count < 3) // prepare() retombe en linéaire à 2 points
        mode = LINEAR;

    switch (mode) {
    case STEP:
        return (float)v0;

    case PLATEAU: { // hermite à tangentes nulles = smoothstep
        double dt = t1 - t0;
        if (dt <= 0.0)
            return (float)v1;
        double u = (t - t0) / dt;
        double s = (3.0 - 2.0 * u) * u * u;
        return (float)(v0 + (v1 - v0) * s);
    }

    case BSPLINE: {
        // Reconstruction des points de contrôle / nœuds avec réflexion aux
        // bords, exactement comme update_INTERPOLATE_BSPLINE_dataMore3_.
        auto T = [&](long j) { return key_t((size_t)j); };
        auto V = [&](long j) { return key_v((size_t)j); };
        double cp[4], knot[6];
        cp[1] = V(i - 1); cp[2] = V(i);
        knot[2] = T(i - 1); knot[3] = T(i);

        long left  = 2 * (long)i;             // iVar5 : 2 → 1er segment, 4 → 2e
        long right = 2 * ((long)count - (long)i); // iVar3 : 2 → dernier, 4 → avant-dernier

        if (left == 2) {
            cp[0]   = 2 * cp[1] - cp[2];
            cp[3]   = V(i + 1);
            knot[4] = T(i + 1);
            knot[1] = 2 * knot[2] - knot[3];
            knot[0] = 2 * knot[2] - knot[4];
            knot[5] = (right == 4) ? 2 * knot[4] - knot[3] : T(i + 2);
        } else {
            cp[0]   = V(i - 2);
            knot[1] = T(i - 2);
            knot[0] = (left == 4) ? 2 * knot[1] - knot[2] : T(i - 3);
            if (right == 2) {
                cp[3]   = 2 * cp[2] - cp[1];
                knot[4] = 2 * knot[3] - knot[2];
                knot[5] = 2 * knot[3] - knot[1];
            } else if (right == 4) {
                cp[3]   = V(i + 1);
                knot[4] = T(i + 1);
                knot[5] = 2 * knot[4] - knot[3];
            } else {
                cp[3]   = V(i + 1);
                knot[4] = T(i + 1);
                knot[5] = T(i + 2);
            }
        }
        return (float)bspline_nonuniform((double)t, cp, knot);
    }

    case LINEAR:
    default: {
        double dt = t1 - t0;
        if (dt <= 0.0)
            return (float)v1;
        return (float)(v0 + (v1 - v0) * (t - t0) / dt);
    }
    }
}

// ── CutsceneChannel ──────────────────────────────────────────────────────────

float CutsceneChannel::sample(const std::vector<FvbTrack>& tracks,
                              float t, float fallback) const {
    const Seg* seg = nullptr;
    for (auto& s : segs) {
        if (s.t0 > t)
            break;
        seg = &s;
    }
    if (!seg)
        return fallback;
    if (!seg->is_track)
        return seg->constant;
    if (seg->track < 0 || seg->track >= (int)tracks.size())
        return fallback;
    return tracks[seg->track].evaluate(t - seg->t0);
}

// ── Cutscene ─────────────────────────────────────────────────────────────────

bool Cutscene::load(const char* path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Cutscene: cannot open " << path << std::endl;
        return false;
    }
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    const uint8_t* d = raw.data();

    if (raw.size() < 0x20 || std::memcmp(d, "STB", 3) != 0 ||
        rd_u16(d + 4) != 0xFEFF) {
        std::cerr << "Cutscene: " << path << " is not an STB file" << std::endl;
        return false;
    }

    uint32_t n_blocks = rd_u32(d + 12);
    std::map<std::string, int> fvb_by_name;

    // Première passe : les pistes FVB, référencées par index dans la suite.
    size_t off = 0x20;
    for (uint32_t i = 0; i < n_blocks && off + 8 <= raw.size(); i++) {
        uint32_t blk_size = rd_u32(d + off);
        if (rd_u32(d + off + 4) == BLK_JFVB)
            parse_fvb(d, off + 8, std::min(raw.size(), off + blk_size),
                      tracks, fvb_by_name);
        if (blk_size < 8)
            break;
        off += blk_size;
    }

    // Deuxième passe : bake des séquences acteurs / caméra.
    off = 0x20;
    for (uint32_t i = 0; i < n_blocks && off + 8 <= raw.size(); i++) {
        size_t   blk_base = off;
        uint32_t blk_size = rd_u32(d + off);
        uint32_t blk_type = rd_u32(d + off + 4);
        if (blk_size < 8)
            break;
        off += blk_size;

        if (blk_type != BLK_JACT && blk_type != BLK_JCMR)
            continue;

        uint16_t id_size = rd_u16(d + blk_base + 10);
        std::string id((const char*)d + blk_base + 12, id_size);
        id.resize(std::strlen(id.c_str()));

        size_t content = blk_base + 12 + align4(id_size);
        size_t content_end = std::min(raw.size(), blk_base + blk_size);

        Baker baker{*this, fvb_by_name};
        if (blk_type == BLK_JCMR) {
            camera.present = true;
            baker.camera = &camera;
        } else {
            actors.push_back({});
            actors.back().id = id;
            baker.actor = &actors.back();
        }

        baker.run(d, content, content_end, blk_base);

        // Le dernier clip d'animation court jusqu'à la fin de la séquence.
        if (baker.actor && baker.anim_index >= 0)
            baker.actor->timeline.then(baker.anim_index,
                                       std::max(baker.t - baker.anim_start, 1e-3f),
                                       true, 0.2f);

        duration = std::max(duration, baker.t);
    }

    // Les timelines bakées se figent sur le dernier clip une fois finies,
    // comme les transforms (local_time clampe quand loop == false).
    for (auto& a : actors)
        a.timeline.loop_all = false;

    std::cout << "Cutscene: " << path << " — " << actors.size() << " actors, "
              << tracks.size() << " fvb tracks, "
              << (camera.present ? "camera, " : "no camera, ")
              << duration << "s" << std::endl;
    return true;
}

CutsceneActor* Cutscene::find(const std::string& id) {
    for (auto& a : actors)
        if (a.id == id)
            return &a;
    return nullptr;
}

float Cutscene::local_time(float time) const {
    if (duration <= 0.0f)
        return 0.0f;
    return loop ? fmodf(time, duration) : std::min(time, duration);
}

glm::mat4 Cutscene::actor_transform(const CutsceneActor& actor, float t,
                                    glm::vec3 fallback_translation) const {
    glm::vec3 tr(actor.tx.sample(tracks, t, fallback_translation.x),
                 actor.ty.sample(tracks, t, fallback_translation.y),
                 actor.tz.sample(tracks, t, fallback_translation.z));
    glm::vec3 rot(actor.rx.sample(tracks, t, 0.0f),
                  actor.ry.sample(tracks, t, 0.0f),
                  actor.rz.sample(tracks, t, 0.0f));
    glm::vec3 sc(actor.sx.sample(tracks, t, 1.0f),
                 actor.sy.sample(tracks, t, 1.0f),
                 actor.sz.sample(tracks, t, 1.0f));

    glm::mat4 m = glm::translate(world, tr);
    m = glm::rotate(m, glm::radians(rot.y), glm::vec3(0, 1, 0));
    m = glm::rotate(m, glm::radians(rot.x), glm::vec3(1, 0, 0));
    m = glm::rotate(m, glm::radians(rot.z), glm::vec3(0, 0, 1));
    return glm::scale(m, sc);
}

void Cutscene::remap_animations(const std::string& actor_id,
                                const std::map<int, int>& remap) {
    CutsceneActor* a = find(actor_id);
    if (!a)
        return;
    for (auto& clip : a->timeline.clips) {
        auto it = remap.find(clip.anim_index);
        if (it != remap.end())
            clip.anim_index = it->second;
    }
}

bool Cutscene::camera_view_projection(float time, float aspect,
                                      glm::mat4& out) const {
    if (!camera.present || duration <= 0.0f)
        return false;
    if (!loop && time >= duration)
        return false; // cutscene finie : rendre la main à la caméra libre

    float t = local_time(time);

    glm::vec3 eye(camera.eye_x.sample(tracks, t, 0.0f),
                  camera.eye_y.sample(tracks, t, 0.0f),
                  camera.eye_z.sample(tracks, t, 0.0f));
    glm::vec3 target(camera.tgt_x.sample(tracks, t, 0.0f),
                     camera.tgt_y.sample(tracks, t, 0.0f),
                     camera.tgt_z.sample(tracks, t, 1.0f));
    float fov  = camera.fovy.sample(tracks, t, 60.0f);
    float roll = camera.roll.sample(tracks, t, 0.0f);

    eye    = glm::vec3(world * glm::vec4(eye, 1.0f));
    target = glm::vec3(world * glm::vec4(target, 1.0f));

    glm::vec3 forward = glm::normalize(target - eye);
    glm::vec3 up(0, 1, 0);
    if (roll != 0.0f)
        up = glm::mat3(glm::rotate(glm::mat4(1.0f), glm::radians(roll), forward)) * up;

    glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, 1.0f, 100000.0f);
    out = proj * glm::lookAt(eye, target, up);
    return true;
}
