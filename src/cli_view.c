#include "cli_view.h"

#include <stdio.h>

void cli_view_render(const CoreUsageFrame* frame) {
  printf("\x1b[H\x1b[J");
  printf("CPU Core Instrument (Ctrl+C to quit)\n\n");
  for (int i = 0; i < frame->num_cores; ++i) {
    float usage = frame->usage[i];
    int bars = (int)(usage * 40.0f);
    if (bars < 0) bars = 0;
    if (bars > 40) bars = 40;

    printf("Core %02d [", i);
    for (int b = 0; b < 40; ++b) {
      putchar(b < bars ? '#' : ' ');
    }
    printf("] %5.1f%%\n", usage * 100.0f);
  }
  fflush(stdout);
}
