#include <stdio.h>
#include "pico/stdlib.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT    spi0
#define PIN_SCK     21
#define PIN_DATA    20

#define PIN_OE      22
#define PIN_LATCH   27

void enableRow(uint8_t row);

const uint16_t displayData1[16] = {
    0x0001,
    0x0002,
    0x0004,
    0x0008,
    0x0010,
    0x0020,
    0x0040,
    0x0080,
    0x0100,
    0x0200,
    0x0400,
    0x0800,
    0x1000,
    0x2000,
    0x4000,
    0x8000,
};

int main()
{
    stdio_init_all();

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

    uint8_t flipMsk = 0;
    while(true) {
        for(int row=0;row<16;row++){
            gpio_put(PIN_OE, 1);
            gpio_put_masked(0xFFFF, displayData1[row]); 
            enableRow(row);
            gpio_put(PIN_OE, 0);

            sleep_ms(1);
        }

        for(int i=0;i<16;i++){

        }
    }
}


void enableRow(uint8_t row){
    uint16_t toWrite = (1 << row);
    // software SPI, because I screwed up on pin layout :P
    // probably can be moved to a PIO block, to investigate
    for(int i=0;i<16;i++){
        if(i == row){
            gpio_put(PIN_DATA, 1);
        } else {
            gpio_put(PIN_DATA, 0);
        }
        gpio_put(PIN_SCK, 1);
        sleep_us(1);
        gpio_put(PIN_SCK, 0);
        sleep_us(1);
    }

    gpio_put(PIN_LATCH, 1);
    sleep_us(10);
    gpio_put(PIN_LATCH, 0);
}