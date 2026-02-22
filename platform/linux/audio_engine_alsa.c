#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "audio_engine.h"
#include "synth.h"

#define SAMPLE_RATE 48000
#define FRAMES_PER_BUFFER 512

struct AudioEngine {
  snd_pcm_t* pcm_handle;
  SharedCoreState* shared_state;
  SynthState synth;
  CoreUsageFrame local_frame;

  pthread_t thread;
  bool running;
  float* buffer;
};

static void* audio_thread_func(void* arg) {
  AudioEngine* engine = (AudioEngine*)arg;

  float* left = malloc(FRAMES_PER_BUFFER * sizeof(float));
  float* right = malloc(FRAMES_PER_BUFFER * sizeof(float));

  while (engine->running) {
    uint64_t seq =
        atomic_load_explicit(&engine->shared_state->seq, memory_order_acquire);
    int idx = (int)(seq & 1ULL);
    engine->local_frame = engine->shared_state->frames[idx];

    synth_render(&engine->synth, &engine->local_frame, left, right,
                 FRAMES_PER_BUFFER);

    // Interleave the audio for ALSA
    for (int i = 0; i < FRAMES_PER_BUFFER; i++) {
      engine->buffer[i * 2] = left[i];
      engine->buffer[i * 2 + 1] = right[i];
    }

    snd_pcm_sframes_t frames =
        snd_pcm_writei(engine->pcm_handle, engine->buffer, FRAMES_PER_BUFFER);
    if (frames < 0) {
      frames = snd_pcm_recover(engine->pcm_handle, frames, 0);
    }
  }

  free(left);
  free(right);
  return NULL;
}

AudioEngine* audio_engine_create(SharedCoreState* shared_state) {
  if (!shared_state) return NULL;

  AudioEngine* engine = (AudioEngine*)calloc(1, sizeof(AudioEngine));
  if (!engine) return NULL;

  engine->shared_state = shared_state;
  engine->buffer = malloc(FRAMES_PER_BUFFER * 2 * sizeof(float));

  synth_init(&engine->synth, SAMPLE_RATE);

  int err =
      snd_pcm_open(&engine->pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
  if (err < 0) {
    free(engine->buffer);
    free(engine);
    return NULL;
  }

  err = snd_pcm_set_params(engine->pcm_handle, SND_PCM_FORMAT_FLOAT_LE,
                           SND_PCM_ACCESS_RW_INTERLEAVED, 2, SAMPLE_RATE, 1,
                           50000);  // 50ms latency
  if (err < 0) {
    snd_pcm_close(engine->pcm_handle);
    free(engine->buffer);
    free(engine);
    return NULL;
  }

  return engine;
}

void audio_engine_destroy(AudioEngine* engine) {
  if (!engine) return;

  audio_engine_stop(engine);

  if (engine->pcm_handle) {
    snd_pcm_close(engine->pcm_handle);
  }

  if (engine->buffer) {
    free(engine->buffer);
  }

  free(engine);
}

int audio_engine_start(AudioEngine* engine) {
  if (!engine || engine->running) return -1;

  engine->running = true;
  if (pthread_create(&engine->thread, NULL, audio_thread_func, engine) != 0) {
    engine->running = false;
    return -1;
  }

  return 0;
}

void audio_engine_stop(AudioEngine* engine) {
  if (!engine || !engine->running) return;

  engine->running = false;
  if (engine->pcm_handle) {
    snd_pcm_drop(engine->pcm_handle);
  }
  pthread_join(engine->thread, NULL);
}

void audio_engine_set_volume(AudioEngine* engine, float volume) {
  if (!engine) return;
  synth_set_master_gain(&engine->synth, volume);
}
