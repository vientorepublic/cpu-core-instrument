#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "audio_engine.h"
#include "cli_view.h"
#include "cpu_metrics.h"
#include "cpu_sampler.h"
#include "shared_state.h"

static volatile sig_atomic_t g_running = 1;

static void on_sigint(int sig) {
  (void)sig;
  g_running = 0;
}

static void shared_state_write(SharedCoreState* state, const float* usage,
                               int cores) {
  uint64_t seq = atomic_load_explicit(&state->seq, memory_order_relaxed);
  int next = (int)((seq + 1ULL) & 1ULL);

  state->frames[next].num_cores = cores;
  for (int i = 0; i < cores && i < MAX_CORES; ++i) {
    state->frames[next].usage[i] = usage[i];
  }

  atomic_store_explicit(&state->seq, seq + 1ULL, memory_order_release);
}

int main(void) {
  signal(SIGINT, on_sigint);

  CpuSampler* sampler = cpu_sampler_create();
  if (!sampler) {
    fprintf(stderr, "Failed to initialize CPU sampler\n");
    return 1;
  }

  int cores = cpu_sampler_get_core_count(sampler);
  if (cores > MAX_CORES) {
    cores = MAX_CORES;
  }

  SharedCoreState shared;
  memset(&shared, 0, sizeof(shared));

  AudioEngine* audio = audio_engine_create(&shared);
  if (!audio) {
    fprintf(stderr, "Failed to initialize audio engine\n");
    cpu_sampler_destroy(sampler);
    return 1;
  }

  if (audio_engine_start(audio) != 0) {
    fprintf(stderr, "Failed to start audio engine\n");
    audio_engine_destroy(audio);
    cpu_sampler_destroy(sampler);
    return 1;
  }

  CpuMetrics metrics;
  cpu_metrics_init(&metrics, 0.35f, 0.02f, 1.0f);

  float raw[MAX_CORES];
  float processed[MAX_CORES];
  CoreUsageFrame frame;
  frame.num_cores = cores;

  while (g_running) {
    int read = cpu_sampler_poll(sampler, raw, MAX_CORES);
    if (read > 0) {
      cpu_metrics_process(&metrics, raw, (size_t)read, processed);
      frame.num_cores = read;
      for (int i = 0; i < read; ++i) {
        frame.usage[i] = processed[i];
      }
      shared_state_write(&shared, frame.usage, frame.num_cores);
      cli_view_render(&frame);
    }
    usleep(50000);
  }

  audio_engine_stop(audio);
  audio_engine_destroy(audio);
  cpu_sampler_destroy(sampler);

  printf("\nStopped.\n");
  return 0;
}
