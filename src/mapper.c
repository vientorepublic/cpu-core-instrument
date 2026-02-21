#include "mapper.h"

#include <math.h>

static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static const int pentatonic_minor_semitones[] = {0,  3,  5,  7,  10,
                                                 12, 15, 17, 19, 22};

static float midi_to_hz(float midi_note) {
  return 440.0f * powf(2.0f, (midi_note - 69.0f) / 12.0f);
}

void mapper_usage_to_voice(float usage, int core_index, VoiceParams* out) {
  usage = clampf(usage, 0.0f, 1.0f);

  int scale_size = (int)(sizeof(pentatonic_minor_semitones) /
                         sizeof(pentatonic_minor_semitones[0]));
  int degree = (int)(usage * (float)(scale_size - 1) + 0.5f);
  degree = (int)clampf((float)degree, 0.0f, (float)(scale_size - 1));

  int group = core_index % 8;
  int is_bass_voice = (group == 0 || group == 1) ? 1 : 0;

  int octave = is_bass_voice ? 1 + (core_index % 2) : 3 + (core_index % 2);
  int note = 12 * octave + 33 + pentatonic_minor_semitones[degree];

  float spread_cents = ((float)(core_index % 5) - 2.0f) * 2.5f;
  float spread_ratio = powf(2.0f, spread_cents / 1200.0f);

  out->frequency = midi_to_hz((float)note) * spread_ratio;

  float base_gain = is_bass_voice ? 0.040f : 0.026f;
  float dyn_gain = is_bass_voice ? 0.080f : 0.050f;
  out->gain = base_gain + usage * dyn_gain;

  float min_cutoff = is_bass_voice ? 120.0f : 380.0f;
  float max_cutoff = is_bass_voice ? 1600.0f : 5200.0f;
  out->cutoff_hz =
      min_cutoff + (max_cutoff - min_cutoff) * (0.15f + usage * 0.85f);

  float pan_shape = ((float)group - 3.5f) / 3.5f;
  out->pan = clampf(pan_shape * 0.85f, -0.95f, 0.95f);

  out->warm =
      clampf(0.45f + usage * 0.5f + (is_bass_voice ? 0.15f : 0.0f), 0.0f, 1.0f);
  out->bass = is_bass_voice ? 1.0f : 0.0f;
}
