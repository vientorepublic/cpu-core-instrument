#ifndef CPU_SAMPLER_H
#define CPU_SAMPLER_H

#include <stddef.h>

typedef struct CpuSampler CpuSampler;

CpuSampler* cpu_sampler_create(void);
void cpu_sampler_destroy(CpuSampler* sampler);
int cpu_sampler_get_core_count(const CpuSampler* sampler);
int cpu_sampler_poll(CpuSampler* sampler, float* out_usage, size_t out_len);

#endif
