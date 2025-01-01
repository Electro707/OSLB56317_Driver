#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"

#include "tusb.h"

#include "Adafruit_ZeroFFT.h"
#include "fftBin.h"
#include "usbHandler.h"

#include "defines.h"

void processAudioFFT(void);
void setColData(uint16_t toWrite);
void transpose(uint16_t *from, uint16_t *to);
void updateDisplay(void);
void pwmIrq(void);

uint16_t prevFilterData[16] = {0};          // buffer if previous filterbank result (range 0 to 16)
uint16_t dispFrameBuff[2][16] = {0};        // the display framebuffer as a swapping double buffer
uint8_t currDisp = 0;                       // index of the active display in the double buffer
uint8_t newDispRdy = 0;                     // flag to set whether the new framebuffer is ready to use

int main()
{
    usbInit();

    gpio_set_function(PIN_DATA,      GPIO_FUNC_SIO);
    gpio_set_function(PIN_SCK,       GPIO_FUNC_SIO);
    gpio_set_function(PIN_OE,        GPIO_FUNC_SIO);
    gpio_set_function(PIN_LATCH,     GPIO_FUNC_SIO);
    gpio_set_function_masked(0xFFFF, GPIO_FUNC_SIO);

    gpio_set_dir_out_masked(0xFFFF);

    gpio_put(PIN_LATCH, 0);
    gpio_put(PIN_OE, 1);

    gpio_set_dir(PIN_LATCH, GPIO_OUT);
    gpio_set_dir(PIN_OE, GPIO_OUT);
    gpio_set_dir(PIN_DATA, GPIO_OUT);
    gpio_set_dir(PIN_SCK, GPIO_OUT);

    pwm_clear_irq(PWM_LED_SLICE);
    pwm_set_clkdiv_int_frac4(PWM_LED_SLICE, 8, 0);
    pwm_set_wrap(PWM_LED_SLICE, 7875-1);
    pwm_set_irq_enabled(PWM_LED_SLICE, true);
    pwm_set_enabled(PWM_LED_SLICE, true);

    gpio_put(PIN_OE, 1);
    setColData(0);
    gpio_put_masked(0xFFFF, 0); 
    gpio_put(PIN_OE, 0);

    irq_set_exclusive_handler(PWM_IRQ_WRAP, pwmIrq);
    irq_set_priority(PWM_IRQ_WRAP, 0);
    irq_set_priority(USBCTRL_IRQ, 10);
    
    irq_set_enabled(PWM_DEFAULT_IRQ_NUM(), true);

    while(true) {
        tud_task();             // TinyUSB device task
        processAudioFFT();      // Process received audio data (if any)
    }
}

void setColData(uint16_t toWrite){
    // software SPI, because I screwed up on pin layout :P
    // probably can be moved to a PIO block, to investigate
    gpio_put(PIN_LATCH, 0);
    for(int i=0;i<16;i++){
        gpio_put(PIN_DATA, toWrite & 1);
        toWrite >>= 1;
        gpio_put(PIN_SCK, 1);
        sleep_us(1);
        gpio_put(PIN_SCK, 0);
        sleep_us(1);
    }
    gpio_put(PIN_LATCH, 1);
}

// quick function to transpose a matrix, to turn from column to row index
void transpose(uint16_t *from, uint16_t *to){
    for (int i = 0; i < 16; i++) {
      to[i] = 0;
      for (int j = 0; j < 16; j++) {
        to[i] <<= 1;
        to[i] |= (from[15-j] & (1 << i)) ? 1 : 0;
      }
    }
}

void processAudioFFT(void){
    uint16_t stat;
    static uint16_t fftData[FFT_SIZE];
    static uint16_t newFiltData[16];
    
    if(tud_audio_available() >= (FFT_SIZE*2)){
        stat = tud_audio_read(fftData, FFT_SIZE*2);
        if(stat != FFT_SIZE*2){
            sentString("ERROR: Read != FFT_SIZE\n");;
            return;
        }

        stat = ZeroFFTMagnitude(fftData, FFT_SIZE, false);
        if(stat != 0){
            sentString("ERROR: fft failed\n");
            return;
        }

        if(newDispRdy){
            sentString("ERR: overrun\n");
            return;
        }

        binFFT(fftData, newFiltData);

        // software peak detector with a bleed of -2 per calculation, and to copy the
        // new filter bank to the global previous filter bank
        for(int i=0;i<16;i++){
            // only do something if the last filter value is not zero
            if(prevFilterData[i]){
                // bleed
                if(prevFilterData[i] <= BLEED_VALUE){
                    prevFilterData[i] = 0;
                } else {
                    prevFilterData[i] -= BLEED_VALUE;    
                }

                // if the previous filter is greater than the current one, set current to old one (after -1)
                if(prevFilterData[i] > newFiltData[i]){
                    newFiltData[i] = prevFilterData[i];
                }
            }
            // update previous filter to the current one
            prevFilterData[i] = newFiltData[i];
            // now we convert the filter numerical value to number of 1's enabled (so 4 is actually 15)
            newFiltData[i] = (1 << newFiltData[i]) - 1;
        }
        
        // transpose the fiter data to the display buffer, as the rows and columns index are flipped
        uint16_t *nextDispBuff = dispFrameBuff[(currDisp + 1) & 0x1];
        transpose(newFiltData, nextDispBuff);
        // declare to the interrupt that the new display buffer is ready
        newDispRdy = 1;
    }
}

// function to update display, called by a pwm interrupt every 500uS
void updateDisplay(void){
    static uint8_t row = 0;
    uint16_t toDisp;

    // only update current display shown when we are ready, and we are starting a new row
    if(newDispRdy & (row == 0)){
        currDisp++;
        currDisp &= 1;
        newDispRdy = 0;
    }

    toDisp = dispFrameBuff[currDisp][row];

    // gpio_put(PIN_OE, 1);                    // turn off the display while updating (doesn't seem nessesary)
    setColData(toDisp);                        // update the column data
    gpio_put_masked(0xFFFF, 1 << row);         // set the appropriate row
    // gpio_put(PIN_OE, 0);

    // increment row from 0 to 15
    row++;
    row &= 0xF;
}

// interrupt entry point
void pwmIrq(void){
    if(pwm_get_irq_status_mask() & (1<<PWM_LED_SLICE)){
        updateDisplay();
        pwm_clear_irq(PWM_LED_SLICE);
    }
}