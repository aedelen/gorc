#pragma once

#include "content/flags/sound_flag.h"
#include "base/content/assets/sound.h"
#include "base/utility/flag_set.h"
#include "base/math/vector.h"
#include "base/utility/easing.h"
#include <SFML/Audio.hpp>

namespace gorc {
namespace game {
namespace world {

class level_model;

namespace components {
class thing;
}

namespace sounds {

class sound {
private:
    sf::Sound internal_sound;
    bool expired = true;

    int thing;
    bool update_position;
    bool do_distance_attenuation;
    float actual_min_rad;
    float actual_max_rad;
    vector<3> position;

    linear_eased<float> pitch, vol;
    float stop_delay = 0.0f;

    void play_ambient(const content::assets::sound& sound, float volume, float panning, flag_set<flags::sound_flag> flags);
    void play_voice(const content::assets::sound& sound, float volume, flag_set<flags::sound_flag> flags);
    void play_positional(const content::assets::sound& sound, const vector<3>& position, float volume, float minrad, float maxrad,
            flag_set<flags::sound_flag> flags);

public:
    sound() = default;
    sound(sound&&) = default;
    sound(const sound&) = delete;

    const sound& operator=(const sound&) = delete;

    void play_sound_local(level_model& model, const content::assets::sound& sound, float volume, float panning, flag_set<flags::sound_flag> flags);
    void play_sound_pos(level_model& model, const content::assets::sound& sound, const vector<3>& pos, float volume,
            float minrad, float maxrad, flag_set<flags::sound_flag> flags);
    void play_sound_thing(level_model& model, const content::assets::sound& sound, int thing, float volume,
            float minrad, float maxrad, flag_set<flags::sound_flag> flags);

    void set_pitch(float pitch, float delay);
    void set_volume(float volume, float delay);
    void stop();
    void stop(float delay);

    void update(double dt, level_model& model);

    inline bool get_expired() {
        return expired;
    }

    inline bool is_attached_to_thing(int thing_id) {
        return update_position && thing == thing_id;
    }

    ~sound();
};

}
}
}
}