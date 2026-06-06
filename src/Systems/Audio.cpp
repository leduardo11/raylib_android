#include "Audio.h"

namespace Systems {

Audio::Audio()
{
    InitAudioDevice();
}

Audio::~Audio()
{
    CloseAudioDevice();
}

Sound Audio::loadSound(const char* path)
{
    return LoadSound(path);
}

Music Audio::loadMusic(const char* path)
{
    return LoadMusicStream(path);
}

void Audio::play(Sound s)
{
    PlaySound(s);
}

void Audio::play(Music m)
{
    PlayMusicStream(m);
}

void Audio::stop(Music m)
{
    StopMusicStream(m);
}

void Audio::updateMusicStream(Music m)
{
    UpdateMusicStream(m);
}

void Audio::setMasterVolume(float vol)
{
    m_masterVolume = vol;
    SetMasterVolume(vol);
}

} // namespace Systems
