#include "cpu_metrics.h"

static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

void cpu_metrics_init(CpuMetrics* metrics, float alpha, float floor,
                      float ceiling) {
  metrics->alpha = alpha;
  metrics->floor = floor;
  metrics->ceiling = ceiling;
  metrics->initialized = 0;
  for (int i = 0; i < 64; ++i) {
    metrics->smoothed[i] = 0.0f;
  }
}

void cpu_metrics_process(CpuMetrics* metrics, const float* usage, size_t len,
                         float* out_processed) {
  if (len > 64) {
    len = 64;
  }
  for (size_t i = 0; i < len; ++i) {
    float in = clampf(usage[i], 0.0f, 1.0f);
    if (!metrics->initialized) {
      metrics->smoothed[i] = in;
    } else {
      metrics->smoothed[i] =
          metrics->alpha * in + (1.0f - metrics->alpha) * metrics->smoothed[i];
    }
    float v = metrics->smoothed[i];
    if (v < metrics->floor) {
      v = 0.0f;
    }
    out_processed[i] = clampf(v, 0.0f, metrics->ceiling);
    if (metrics->ceiling > 0.0f) {
      out_processed[i] /= metrics->ceiling;
    }
  }
  metrics->initialized = 1;
}
