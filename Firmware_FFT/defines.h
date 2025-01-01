/**
 * A list with common defines
 */

#ifndef _COMMON_H
#define _COMMON_H

#define BLEED_VALUE 2       // how many segments to subtract per cycle, a cycle being when we get FFT_SIZE audio data (~6mS)

// pin definitions
#define PIN_SCK     21
#define PIN_DATA    20
#define PIN_OE      22
#define PIN_LATCH   27

// the FFT size to use
#define FFT_SIZE    512

// the PWM slice that is used for the real-time interrupt
#define PWM_LED_SLICE 0

#endif