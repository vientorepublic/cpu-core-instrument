#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cpu_sampler.h"

#define MAX_CORES 64

struct CpuSampler {
  int num_cores;
  unsigned long long last_total[MAX_CORES];
  unsigned long long last_idle[MAX_CORES];
};

CpuSampler* cpu_sampler_create(void) {
  CpuSampler* sampler = (CpuSampler*)calloc(1, sizeof(CpuSampler));
  if (!sampler) return NULL;

  FILE* fp = fopen("/proc/stat", "r");
  if (!fp) {
    free(sampler);
    return NULL;
  }

  char line[256];
  int core_count = 0;
  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
      core_count++;
    }
  }
  fclose(fp);

  sampler->num_cores = core_count > MAX_CORES ? MAX_CORES : core_count;
  return sampler;
}

void cpu_sampler_destroy(CpuSampler* sampler) {
  if (sampler) {
    free(sampler);
  }
}

int cpu_sampler_get_core_count(const CpuSampler* sampler) {
  if (!sampler) return 0;
  return sampler->num_cores;
}

int cpu_sampler_poll(CpuSampler* sampler, float* out_usage, size_t out_len) {
  if (!sampler || !out_usage) return -1;

  FILE* fp = fopen("/proc/stat", "r");
  if (!fp) return -1;

  char line[256];
  int core_idx = 0;
  while (fgets(line, sizeof(line), fp) && core_idx < sampler->num_cores &&
         core_idx < out_len) {
    if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
      unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0,
                         irq = 0, softirq = 0, steal = 0, guest = 0,
                         guest_nice = 0;
      if (sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                 &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal,
                 &guest, &guest_nice) >= 4) {
        unsigned long long idle_time = idle + iowait;
        unsigned long long non_idle_time =
            user + nice + system + irq + softirq + steal;
        unsigned long long total_time = idle_time + non_idle_time;

        unsigned long long total_diff =
            total_time - sampler->last_total[core_idx];
        unsigned long long idle_diff = idle_time - sampler->last_idle[core_idx];

        if (total_diff > 0) {
          out_usage[core_idx] =
              (float)(total_diff - idle_diff) / (float)total_diff;
        } else {
          out_usage[core_idx] = 0.0f;
        }

        sampler->last_total[core_idx] = total_time;
        sampler->last_idle[core_idx] = idle_time;
        core_idx++;
      }
    }
  }
  fclose(fp);

  return 0;
}
