#include "synth.h"

#include <math.h>

#include "mapper.h"

static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static float wrap_phase(float phase) {
  while (phase >= 1.0f) {
    phase -= 1.0f;
  }
  while (phase < 0.0f) {
    phase += 1.0f;
  }
  return phase;
}

static float one_pole_alpha(float cutoff_hz, float sample_rate) {
  cutoff_hz = clampf(cutoff_hz, 20.0f, sample_rate * 0.45f);
  return 1.0f - expf(-2.0f * (float)M_PI * cutoff_hz / sample_rate);
}

static float one_pole_highpass_coeff(float cutoff_hz, float sample_rate) {
  cutoff_hz = clampf(cutoff_hz, 20.0f, sample_rate * 0.45f);
  float dt = 1.0f / sample_rate;
  float rc = 1.0f / (2.0f * (float)M_PI * cutoff_hz);
  return rc / (rc + dt);
}

static float tri_from_phase(float phase) {
  float saw = 2.0f * phase - 1.0f;
  return 2.0f * fabsf(saw) - 1.0f;
}

void synth_init(SynthState* synth, float sample_rate) {
  synth->sample_rate = sample_rate;
  synth->master_gain = 1.0f;
  for (int i = 0; i < MAX_CORES; ++i) {
    synth->phase[i] = 0.0f;
    synth->sub_phase[i] = 0.0f;
    synth->current_freq[i] = 110.0f;
    synth->current_gain[i] = 0.0f;
    synth->lp_state[i] = 0.0f;
    synth->hp_state[i] = 0.0f;
    synth->hp_input[i] = 0.0f;
  }
}

void synth_set_master_gain(SynthState* synth, float master_gain) {
  if (!synth) {
    return;
  }
  synth->master_gain = clampf(master_gain, 0.0f, 2.0f);
}

void synth_render(SynthState* synth, const CoreUsageFrame* frame, float* out_l,
                  float* out_r, size_t frames) {
  VoiceParams params[MAX_CORES];
  int num_voices = frame->num_cores;
  if (num_voices > MAX_CORES) {
    num_voices = MAX_CORES;
  }

  for (int c = 0; c < num_voices; ++c) {
    mapper_usage_to_voice(frame->usage[c], c, &params[c]);
  }

  for (size_t i = 0; i < frames; ++i) {
    float mix_l = 0.0f;
    float mix_r = 0.0f;
    for (int c = 0; c < num_voices; ++c) {
      float target_freq = params[c].frequency;
      float target_gain = params[c].gain;

      float glide_coeff = 0.0025f;
      synth->current_freq[c] +=
          glide_coeff * (target_freq - synth->current_freq[c]);

      float gain_coeff =
          target_gain > synth->current_gain[c] ? 0.002f : 0.0009f;
      synth->current_gain[c] +=
          gain_coeff * (target_gain - synth->current_gain[c]);

      float phase = synth->phase[c];
      float sub_phase = synth->sub_phase[c];

      float s1 = sinf(phase * 2.0f * (float)M_PI);
      float s3 = sinf(phase * 6.0f * (float)M_PI);
      float tri = tri_from_phase(phase);
      float sub = sinf(sub_phase * 2.0f * (float)M_PI);

      float body = 0.68f * s1 + 0.20f * tri + 0.12f * s3;
      float raw = body * (1.0f - 0.45f * params[c].bass) +
                  sub * (0.45f * params[c].bass);

      float alpha_lp = one_pole_alpha(params[c].cutoff_hz, synth->sample_rate);
      synth->lp_state[c] += alpha_lp * (raw - synth->lp_state[c]);

      float hp_coeff = one_pole_highpass_coeff(22.0f, synth->sample_rate);
      float hp = hp_coeff *
                 (synth->hp_state[c] + synth->lp_state[c] - synth->hp_input[c]);
      synth->hp_input[c] = synth->lp_state[c];
      synth->hp_state[c] = hp;

      float tone = hp * (0.72f + 0.28f * params[c].warm);
      float colored = tanhf(tone * (1.08f + 0.35f * params[c].warm));
      float voice = colored * synth->current_gain[c];

      float pan01 = (params[c].pan + 1.0f) * 0.5f;
      pan01 = clampf(pan01, 0.0f, 1.0f);
      float gain_l = sqrtf(1.0f - pan01);
      float gain_r = sqrtf(pan01);

      mix_l += voice * gain_l;
      mix_r += voice * gain_r;

      phase = wrap_phase(phase + synth->current_freq[c] / synth->sample_rate);
      sub_phase = wrap_phase(sub_phase + (synth->current_freq[c] * 0.5f) /
                                             synth->sample_rate);
      synth->phase[c] = phase;
      synth->sub_phase[c] = sub_phase;
    }

    float out_gain = 0.92f * synth->master_gain;
    out_l[i] = tanhf(mix_l * 1.35f) * out_gain;
    out_r[i] = tanhf(mix_r * 1.35f) * out_gain;
  }
}
