#ifndef MAPPER_H
#define MAPPER_H

typedef struct {
  float frequency;
  float gain;
  float cutoff_hz;
  float pan;
  float warm;
  float bass;
} VoiceParams;

void mapper_usage_to_voice(float usage, int core_index, VoiceParams* out);

#endif
