#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include "shared_state.h"

typedef struct AudioEngine AudioEngine;

AudioEngine* audio_engine_create(SharedCoreState* shared_state);
void audio_engine_destroy(AudioEngine* engine);
int audio_engine_start(AudioEngine* engine);
void audio_engine_stop(AudioEngine* engine);
void audio_engine_set_volume(AudioEngine* engine, float volume);

#endif
