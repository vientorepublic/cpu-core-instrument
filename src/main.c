#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "audio_engine.h"
#include "cli_view.h"
#include "cpu_metrics.h"
#include "cpu_sampler.h"
#include "shared_state.h"

static volatile sig_atomic_t g_running = 1;

static void print_usage(const char* program_name) {
  printf("Usage: %s [--volume <0.0-2.0>] [--refresh-hz <1-120>]\n",
         program_name);
  printf("       %s [-v <0.0-2.0>] [-f <1-120>]\n", program_name);
}

static int parse_args(int argc, char** argv, float* out_volume,
                      int* out_refresh_hz) {
  *out_volume = 0.5f;
  *out_refresh_hz = 3;

  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      print_usage(argv[0]);
      return 1;
    }

    if (strcmp(argv[i], "--volume") == 0 || strcmp(argv[i], "-v") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing value for %s\n", argv[i]);
        return -1;
      }

      char* end = NULL;
      float value = strtof(argv[i + 1], &end);
      if (end == argv[i + 1] || (end && *end != '\0')) {
        fprintf(stderr, "Invalid volume: %s\n", argv[i + 1]);
        return -1;
      }
      if (value < 0.0f || value > 2.0f) {
        fprintf(stderr, "Volume out of range (0.0 - 2.0): %s\n", argv[i + 1]);
        return -1;
      }

      *out_volume = value;
      ++i;
      continue;
    }

    if (strcmp(argv[i], "--refresh-hz") == 0 || strcmp(argv[i], "-f") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Missing value for %s\n", argv[i]);
        return -1;
      }

      char* end = NULL;
      long value = strtol(argv[i + 1], &end, 10);
      if (end == argv[i + 1] || (end && *end != '\0')) {
        fprintf(stderr, "Invalid refresh rate: %s\n", argv[i + 1]);
        return -1;
      }
      if (value < 1 || value > 120) {
        fprintf(stderr, "Refresh rate out of range (1 - 120): %s\n",
                argv[i + 1]);
        return -1;
      }

      *out_refresh_hz = (int)value;
      ++i;
      continue;
    }

    fprintf(stderr, "Unknown option: %s\n", argv[i]);
    return -1;
  }

  return 0;
}

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

int main(int argc, char** argv) {
  float volume = 0.5f;
  int refresh_hz = 3;
  int parse_result = parse_args(argc, argv, &volume, &refresh_hz);
  if (parse_result > 0) {
    return 0;
  }
  if (parse_result < 0) {
    print_usage(argv[0]);
    return 1;
  }

  unsigned int refresh_sleep_us = (unsigned int)(1000000 / refresh_hz);

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

  audio_engine_set_volume(audio, volume);

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
    usleep(refresh_sleep_us);
  }

  audio_engine_stop(audio);
  audio_engine_destroy(audio);
  cpu_sampler_destroy(sampler);

  printf("\nStopped.\n");
  return 0;
}
