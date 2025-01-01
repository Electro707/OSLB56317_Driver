#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"

#include "bsp/board_api.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "Adafruit_ZeroFFT.h"
#include "melScale.h"

// SPI Defines
// We are going to use SPI 0, and allocate it to the following GPIO pins
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define SPI_PORT    spi0
#define PIN_SCK     21
#define PIN_DATA    20

#define DEBUG printf

#define PIN_OE      22
#define PIN_LATCH   27

enum
{
  VOLUME_CTRL_0_DB = 0,
  VOLUME_CTRL_10_DB = 2560,
  VOLUME_CTRL_20_DB = 5120,
  VOLUME_CTRL_30_DB = 7680,
  VOLUME_CTRL_40_DB = 10240,
  VOLUME_CTRL_50_DB = 12800,
  VOLUME_CTRL_60_DB = 15360,
  VOLUME_CTRL_70_DB = 17920,
  VOLUME_CTRL_80_DB = 20480,
  VOLUME_CTRL_90_DB = 23040,
  VOLUME_CTRL_100_DB = 25600,
  VOLUME_CTRL_SILENCE = 0x8000,
};

#define FFT_SIZE    512

#define PWM_LED_SLICE 0

bool mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1]; 				          // +1 for master channel 0
int16_t volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1]; 					// +1 for master channel 0
uint32_t sampFreq;
uint8_t clkValid;
audio_control_range_2_n_t(1) volumeRng[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX+1]; 			// Volume range state
audio_control_range_4_n_t(1) sampleFreqRng; 						// Sample frequency range state

void processLine(uint8_t *token);
void processAudioFFT(void);
void setColData(uint16_t toWrite);
void updateDisplay(void);
void pwmIrq(void);

uint16_t prevFilterData[16] = {0};          // buffer if previous filterbank result (range 0 to 16)
uint16_t dispFrameBuff[2][16] = {0};        // the display framebuffer as a swapping double buffer
uint8_t currDisp = 0;                       // index of the active display in the double buffer
uint8_t newDispRdy = 0;                     // flag to set whether the new framebuffer is ready to use

int main()
{
    board_init();
    tusb_init();

    // Init values
    sampFreq = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    clkValid = 1;
    sampleFreqRng.wNumSubRanges = 1;
    sampleFreqRng.subrange[0].bMin = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    sampleFreqRng.subrange[0].bMax = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    sampleFreqRng.subrange[0].bRes = 0;

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
    // sleep_us(2);
    gpio_put(PIN_LATCH, 1);
}

#if CFG_TUD_CDC
void tud_cdc_rx_cb(uint8_t itf)
{
  static uint8_t buf[128];      // todo: configurable size
  static uint32_t bufIdx = 0;
  uint32_t count;

  // connected() check for DTR bit
  // Most but not all terminal client set this when making connection
  if (tud_cdc_connected())
  {
    if (tud_cdc_available()) // data is available
    {
        // todo, if above buffer allocation, handle plz
        count = tud_cdc_n_read(itf, buf+bufIdx, 128-bufIdx);

        for(int i=bufIdx;i<bufIdx+count;i++){
            if(buf[i] == '\n'){
                processLine(buf);
                bufIdx = 0;
                count = 0;
                break;
            }
        }

        bufIdx += count;

    //   tud_cdc_n_write(itf, buf, count);
    //   tud_cdc_n_write_flush(itf);
      // dummy code to check that cdc serial is responding
    //   printf("Responding!\n");
    }
  }
}

void processLine(uint8_t *token){
    printf("Processing Line %s", token);
    if(strcmp(token, "ping")){
        tud_cdc_n_write(0, "pong!", 5);
    }
    tud_cdc_n_write_flush(0);
}

void sentString(const char *str){
  tud_cdc_n_write(0, str, sizeof(str));
  tud_cdc_n_write_flush(0);
}
#endif

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request, uint8_t *pBuff)
{
  (void) rhport;

  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  uint8_t itf = TU_U16_LOW(p_request->wIndex);
  uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

  (void) itf;

  // We do not support any set range requests here, only current value requests
  TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

  // If request is for our feature unit
  if ( entityID == UAC2_ENTITY_FEATURE_UNIT )
  {
    switch ( ctrlSel )
    {
      case AUDIO_FU_CTRL_MUTE:
        // Request uses format layout 1
        TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_1_t));

        mute[channelNum] = ((audio_control_cur_1_t*) pBuff)->bCur;

        DEBUG("    Set Mute: %d of channel: %u\r\n", mute[channelNum], channelNum);
      return true;

      case AUDIO_FU_CTRL_VOLUME:
        // Request uses format layout 2
        TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_2_t));

        volume[channelNum] = (uint16_t) ((audio_control_cur_2_t*) pBuff)->bCur;

        DEBUG("    Set channel %d volume: %d dB\r\n", channelNum, volume[channelNum] / 256);
      return true;

        // Unknown/Unsupported control
      default:
        TU_BREAKPOINT();
      return false;
    }
  }
  return false;    // Yet not implemented
}

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
  (void) rhport;

  // Page 91 in UAC2 specification
  uint8_t channelNum = TU_U16_LOW(p_request->wValue);
  uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
  // uint8_t itf = TU_U16_LOW(p_request->wIndex); 			// Since we have only one audio function implemented, we do not need the itf value
  uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

  // Input terminal (Speaker input)
  if (entityID == UAC2_ENTITY_INPUT_TERMINAL)
  {
    switch ( ctrlSel )
    {
      case AUDIO_TE_CTRL_CONNECTOR:
      {
        // The terminal connector control only has a get request with only the CUR attribute.
        audio_desc_channel_cluster_t ret;

        // Those are dummy values for now
        ret.bNrChannels = 1;
        ret.bmChannelConfig = (audio_channel_config_t) 0;
        ret.iChannelNames = 0;

        DEBUG("    Get terminal connector\r\n");

        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void*) &ret, sizeof(ret));
      }
      break;

        // Unknown/Unsupported control selector
      default:
        TU_BREAKPOINT();
        return false;
    }
  }

  // Feature unit
  if (entityID == UAC2_ENTITY_FEATURE_UNIT)
  {
    switch ( ctrlSel )
    {
      case AUDIO_FU_CTRL_MUTE:
        // Audio control mute cur parameter block consists of only one byte - we thus can send it right away
        // There does not exist a range parameter block for mute
        DEBUG("    Get Mute of channel: %u\r\n", channelNum);
        return tud_control_xfer(rhport, p_request, &mute[channelNum], 1);

      case AUDIO_FU_CTRL_VOLUME:
        switch ( p_request->bRequest )
        {
          case AUDIO_CS_REQ_CUR:
            DEBUG("    Get Volume of channel: %u\r\n", channelNum);
            return tud_control_xfer(rhport, p_request, &volume[channelNum], sizeof(volume[channelNum]));

          case AUDIO_CS_REQ_RANGE:

            // Copy values - only for testing - better is version below
            audio_control_range_2_n_t(1) ret;

            ret.wNumSubRanges = tu_htole16(1),
            ret.subrange[0].bMin = tu_htole16(-VOLUME_CTRL_50_DB);
            ret.subrange[0].bMax = tu_htole16(VOLUME_CTRL_0_DB);
            ret.subrange[0].bRes = tu_htole16(256);

            DEBUG("    Get channel %u volume range (%d, %d, %u) dB\r\n", channelNum,
              ret.subrange[0].bMin / 256, ret.subrange[0].bMax / 256, ret.subrange[0].bRes / 256);

            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void*) &ret, sizeof(ret));

            // Unknown/Unsupported control
          default:
            TU_BREAKPOINT();
            return false;
        }
      break;

        // Unknown/Unsupported control
      default:
        TU_BREAKPOINT();
        return false;
    }
  }

  // Clock Source unit
  if ( entityID == UAC2_ENTITY_CLOCK )
  {
    switch ( ctrlSel )
    {
      case AUDIO_CS_CTRL_SAM_FREQ:
        // channelNum is always zero in this case
        switch ( p_request->bRequest )
        {
          case AUDIO_CS_REQ_CUR:
            DEBUG("    Get Sample Freq.\r\n");
            return tud_control_xfer(rhport, p_request, &sampFreq, sizeof(sampFreq));

          case AUDIO_CS_REQ_RANGE:
            DEBUG("    Get Sample Freq. range\r\n");
            return tud_control_xfer(rhport, p_request, &sampleFreqRng, sizeof(sampleFreqRng));

           // Unknown/Unsupported control
          default:
            TU_BREAKPOINT();
            return false;
        }
      break;

      case AUDIO_CS_CTRL_CLK_VALID:
        // Only cur attribute exists for this request
        DEBUG("    Get Sample Freq. valid\r\n");
        return tud_control_xfer(rhport, p_request, &clkValid, sizeof(clkValid));

      // Unknown/Unsupported control
      default:
        TU_BREAKPOINT();
        return false;
    }
  }

  DEBUG("  Unsupported entity: %d\r\n", entityID);
  return false; 	// Yet not implemented
}


bool tud_audio_rx_done_pre_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting)
{
    (void)rhport;
    (void)func_id;
    (void)ep_out;
    (void)cur_alt_setting;

    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const * p_request)
{
  (void) rhport;
  (void) p_request;

  return true;
}

void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t* feedback_param)
{
    (void)func_id;
    (void)alt_itf;
    // Set feedback method to fifo counting
    feedback_param->method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
    feedback_param->sample_freq = sampFreq;
}

// quick function to transpose a matrix, to turn from column to row index
void transpose(uint16_t *from, uint16_t *to) {
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
    // static uint8_t row = 0;
    // static uint16_t cnt = 0;

    // char tmp[32];
    // sprintf(tmp, "%d\n", tud_audio_available());
    // tud_cdc_n_write(0, tmp, strlen(tmp));

    
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

        melScaleFftNormalize(fftData, newFiltData);

        // software peak detector with a bleed of -2 per calculation, and to copy the
        // new filter bank to the global previous filter bank
        for(int i=0;i<16;i++){
            // only do something if the last filter value is not zero
            if(prevFilterData[i]){
                // bleed
                if(prevFilterData[i] <= 2){
                    prevFilterData[i] = 0;
                } else {
                    prevFilterData[i] -= 2;    
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
        newDispRdy = 0;
        currDisp++;
        currDisp &= 1;
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