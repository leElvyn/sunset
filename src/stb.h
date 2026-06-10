#pragma once
// Lecteur de cutscenes JStudio .stb (Zelda: Twilight Princess).
//
// Le fichier contient :
//  - un bloc JFVBFVB : des courbes scalaires ("function values") indexées,
//    soit des paires (param, valeur) interpolées linéairement (type 5),
//    soit des clés hermite (type 6) ;
//  - des blocs JACT (acteurs) / JCMR (caméra) : un flux séquentiel
//    "paragraphes + wait N frames" (30 fps) qui lie les propriétés
//    (translation, rotation, animation, eye/target/fov/roll…) à des
//    constantes ou à des courbes FVB.
//
// Au chargement, la séquence est entièrement "bakée" :
//  - chaque propriété devient un canal par morceaux échantillonnable en temps ;
//  - les changements d'ANIMATION deviennent des clips de la Timeline existante,
//    ce qui réutilise le blending de clips déjà en place.

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "timeline.h"

// Courbe FVB : keys aplaties par stride.
// btype 5 (list_parameter) : stride 2, paires (param, valeur). Le mode
//   d'interpolation est porté par le paragraphe 0x16 :
//     0 = step, 1 = linéaire, 2 = plateau (smoothstep), 3 = B-spline cubique.
// btype 6 (hermite) : stride 3 (t, val, tan) ou 4 (t, val, tan_in, tan_out).
struct FvbTrack {
    enum Interp { STEP = 0, LINEAR = 1, PLATEAU = 2, BSPLINE = 3 };

    int btype  = 0;
    int stride = 0;
    int interp = LINEAR;
    float range_end = 0.0f;
    std::vector<float> keys;

    float evaluate(float t) const;
};

// Canal scalaire d'une propriété : suite de segments [t0, t0_suivant) dont la
// valeur est une constante ou une courbe FVB évaluée en (t - t0).
struct CutsceneChannel {
    struct Seg {
        float t0       = 0.0f;
        bool  is_track = false;
        float constant = 0.0f;
        int   track    = -1;
    };
    std::vector<Seg> segs; // triés par t0 (ordre d'émission de la séquence)

    bool empty() const { return segs.empty(); }
    float sample(const std::vector<FvbTrack>& tracks, float t, float fallback) const;
};

struct CutsceneActor {
    std::string id;
    CutsceneChannel tx, ty, tz; // translation (unités du jeu)
    CutsceneChannel rx, ry, rz; // rotation en degrés
    CutsceneChannel sx, sy, sz; // échelle
    Timeline timeline;          // clips bakés depuis les paragraphes ANIMATION

    // Pour le cheval, la propriété ANIMATION ne porte pas un index d'animation
    // mais un MODE dispatché par daHorse_c::setDemoData :
    //   5            = téléport vers la position STB (+ angle)
    //   2/3/4/10/11  = course/marche VERS la position STB (cible mobile)
    //   1            = arrêt
    // Les paragraphes TRANSLATION sont donc soit une position instantanée
    // (téléport), soit une *cible* que le cheval rejoint progressivement.
    std::vector<std::pair<float, int>> demo_modes; // (temps, mode)

    // Position / lacet pré-simulés en respectant les modes (cf.
    // Cutscene::simulate_horse_drive). has_sim=true => actor_transform lit ces
    // tableaux au lieu d'échantillonner directement les canaux.
    std::vector<glm::vec3> sim_pos;
    std::vector<float>     sim_yaw;     // radians (lacet monde)
    float sim_dt        = 1.0f / 30.0f;
    bool  has_sim       = false;
    float facing_offset = 0.0f;         // aligne l'avant du modèle (radians)

    // Paragraphes DATA (0x80) horodatés : (temps, u32 big-endian). Pour TLogo :
    // "31 vv 00 00", l'octet vv (bits 16..23) bascule l'affichage (0=off,1=on).
    std::vector<std::pair<float, uint32_t>> data_events;
};

struct CutsceneCamera {
    bool present = false;
    CutsceneChannel eye_x, eye_y, eye_z;
    CutsceneChannel tgt_x, tgt_y, tgt_z;
    CutsceneChannel fovy, roll; // degrés
};

struct Cutscene {
    std::vector<FvbTrack> tracks;
    std::vector<CutsceneActor> actors;
    CutsceneCamera camera;
    float duration = 0.0f;             // secondes (frames séquence / 30)
    bool  loop     = false;            // rejouer en boucle ou figer à la fin
    glm::mat4 world = glm::mat4(1.0f); // repère cutscene → repère scène

    bool load(const char* path);
    CutsceneActor* find(const std::string& id);

    // Temps cutscene local (boucle ou clamp selon `loop`).
    float local_time(float time) const;

    // Position (translation seule) d'un acteur à l'instant t.
    glm::vec3 actor_position(const CutsceneActor& actor, float t,
                             glm::vec3 fallback = {0, 0, 0}) const;

    // Matrice modèle d'un acteur à l'instant t (temps local).
    glm::mat4 actor_transform(const CutsceneActor& actor, float t,
                              glm::vec3 fallback_translation = {0, 0, 0}) const;

    // Simule la position/orientation d'un acteur "cheval" en respectant les
    // modes demo : téléport (5) instantané, course (2/3/4/10/11) qui rejoint la
    // cible à vitesse bornée et oriente le cheval vers elle, arrêt (1) figé.
    // max_speed en unités/s (≥ vitesse de galop pour suivre fidèlement le
    // tracé ; borne uniquement les sauts type SET). facing_offset aligne
    // l'avant du modèle. Remplit actor.sim_pos / sim_yaw.
    void simulate_horse_drive(const std::string& actor_id,
                              float max_speed = 15000.0f,
                              float facing_offset = 0.0f);

    // Opacité [0,1] d'un acteur "logo" piloté par ses paragraphes DATA (octet
    // d'état 0/1). 0 tant qu'il est éteint ; rampe sur fade_in secondes à
    // l'allumage, redescend sur fade_out à l'extinction. Pour TLogo : reste 0
    // jusqu'à ~60,3 s puis fond en entrée.
    float logo_alpha(const std::string& actor_id, float t,
                     float fade_in = 1.5f, float fade_out = 1.0f) const;

    // Remplace les index d'animation bakés (index BCK du jeu → index glTF).
    void remap_animations(const std::string& actor_id,
                          const std::map<int, int>& remap);

    // view-projection de la caméra cutscene ; false si pas de bloc caméra
    // ou si la cutscene est terminée (loop == false).
    bool camera_view_projection(float time, float aspect, glm::mat4& out) const;
};
