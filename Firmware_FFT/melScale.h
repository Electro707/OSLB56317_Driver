#ifndef MEL_SCALE_H
#define MEL_SCALE_H

#include "Adafruit_ZeroFFT.h"

void melScaleFftNormalize(q15_t *fftIn, uint16_t *segOut);

#endif