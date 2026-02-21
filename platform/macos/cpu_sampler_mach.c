#include <mach/mach.h>
#include <stdlib.h>
#include <string.h>

#include "cpu_sampler.h"

struct CpuSampler {
  int core_count;
  processor_cpu_load_info_data_t* prev_ticks;
  int has_prev;
};

static int sample_ticks(processor_cpu_load_info_data_t** out_info,
                        natural_t* out_count) {
  processor_info_array_t info_array = NULL;
  mach_msg_type_number_t info_count = 0;
  natural_t cpu_count = 0;

  kern_return_t kr =
      host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &cpu_count,
                          &info_array, &info_count);

  if (kr != KERN_SUCCESS) {
    return -1;
  }

  *out_info = (processor_cpu_load_info_data_t*)info_array;
  *out_count = cpu_count;
  return 0;
}

CpuSampler* cpu_sampler_create(void) {
  CpuSampler* sampler = (CpuSampler*)calloc(1, sizeof(CpuSampler));
  if (!sampler) {
    return NULL;
  }

  processor_cpu_load_info_data_t* current = NULL;
  natural_t count = 0;
  if (sample_ticks(&current, &count) != 0 || count == 0) {
    free(sampler);
    return NULL;
  }

  sampler->core_count = (int)count;
  if (sampler->core_count > 64) {
    sampler->core_count = 64;
  }

  sampler->prev_ticks = (processor_cpu_load_info_data_t*)calloc(
      (size_t)sampler->core_count, sizeof(processor_cpu_load_info_data_t));
  if (!sampler->prev_ticks) {
    vm_deallocate(mach_task_self(), (vm_address_t)current,
                  count * sizeof(processor_cpu_load_info_data_t));
    free(sampler);
    return NULL;
  }

  memcpy(sampler->prev_ticks, current,
         (size_t)sampler->core_count * sizeof(processor_cpu_load_info_data_t));
  sampler->has_prev = 1;

  vm_deallocate(mach_task_self(), (vm_address_t)current,
                count * sizeof(processor_cpu_load_info_data_t));
  return sampler;
}

void cpu_sampler_destroy(CpuSampler* sampler) {
  if (!sampler) {
    return;
  }
  free(sampler->prev_ticks);
  free(sampler);
}

int cpu_sampler_get_core_count(const CpuSampler* sampler) {
  return sampler ? sampler->core_count : 0;
}

int cpu_sampler_poll(CpuSampler* sampler, float* out_usage, size_t out_len) {
  if (!sampler || !out_usage || out_len == 0) {
    return -1;
  }

  processor_cpu_load_info_data_t* current = NULL;
  natural_t count = 0;
  if (sample_ticks(&current, &count) != 0) {
    return -1;
  }

  int n = sampler->core_count;
  if (n > (int)out_len) {
    n = (int)out_len;
  }
  if (n > (int)count) {
    n = (int)count;
  }

  for (int i = 0; i < n; ++i) {
    uint32_t user = current[i].cpu_ticks[CPU_STATE_USER];
    uint32_t sys = current[i].cpu_ticks[CPU_STATE_SYSTEM];
    uint32_t idle = current[i].cpu_ticks[CPU_STATE_IDLE];
    uint32_t nice = current[i].cpu_ticks[CPU_STATE_NICE];

    uint32_t p_user = sampler->prev_ticks[i].cpu_ticks[CPU_STATE_USER];
    uint32_t p_sys = sampler->prev_ticks[i].cpu_ticks[CPU_STATE_SYSTEM];
    uint32_t p_idle = sampler->prev_ticks[i].cpu_ticks[CPU_STATE_IDLE];
    uint32_t p_nice = sampler->prev_ticks[i].cpu_ticks[CPU_STATE_NICE];

    uint32_t d_user = user - p_user;
    uint32_t d_sys = sys - p_sys;
    uint32_t d_idle = idle - p_idle;
    uint32_t d_nice = nice - p_nice;

    uint32_t d_total = d_user + d_sys + d_idle + d_nice;
    uint32_t d_active = d_user + d_sys + d_nice;

    if (d_total == 0) {
      out_usage[i] = 0.0f;
    } else {
      out_usage[i] = (float)d_active / (float)d_total;
    }
  }

  memcpy(sampler->prev_ticks, current,
         (size_t)n * sizeof(processor_cpu_load_info_data_t));
  vm_deallocate(mach_task_self(), (vm_address_t)current,
                count * sizeof(processor_cpu_load_info_data_t));

  return n;
}
