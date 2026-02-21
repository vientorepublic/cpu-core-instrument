#ifndef CPU_METRICS_H
#define CPU_METRICS_H

#include <stddef.h>

typedef struct {
    float alpha;
    float floor;
    float ceiling;
    float smoothed[64];
    int initialized;
} CpuMetrics;

void cpu_metrics_init(CpuMetrics* metrics, float alpha, float floor, float ceiling);
void cpu_metrics_process(CpuMetrics* metrics, const float* usage, size_t len, float* out_processed);

#endif
