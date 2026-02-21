#ifndef SYNTH_H
#define SYNTH_H

#include <stddef.h>

#include "shared_state.h"

typedef struct {
  float sample_rate;
  float master_gain;
  float phase[MAX_CORES];
  float sub_phase[MAX_CORES];
  float current_freq[MAX_CORES];
  float current_gain[MAX_CORES];
  float lp_state[MAX_CORES];
  float hp_state[MAX_CORES];
  float hp_input[MAX_CORES];
} SynthState;

void synth_init(SynthState* synth, float sample_rate);
void synth_set_master_gain(SynthState* synth, float master_gain);
void synth_render(SynthState* synth, const CoreUsageFrame* frame, float* out_l,
                  float* out_r, size_t frames);

#endif
