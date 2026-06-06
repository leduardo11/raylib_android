#pragma once

#include "raylib.h"

namespace Systems {

struct Audio {
    Audio();
    ~Audio();

    Sound loadSound(const char* path);
    Music loadMusic(const char* path);
    void  play(Sound s);
    void  play(Music m);
    void  stop(Music m);
    void  updateMusicStream(Music m);

    void setMasterVolume(float vol);

private:
    float m_masterVolume = 1.0f;
};

} // namespace Systems
