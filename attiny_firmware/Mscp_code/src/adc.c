#include "adc.h"
#include "usart_out.h"
#include <avr/io.h>
#include <stdint.h>
#include <stdbool.h>

/* Resources used:
 * ADC0, TCA0, SYNCCH0, ASYNCUSER1, Pins B2, B3, B4, B5
 */

// Number of cycles for ADC conversion
#define ADC_SAMPLE_PER_CLK_BASE_CYCLES 0x0038
#define ADC_SAMPLE_DELAY 0x0000
#define ADC_SAMPLE_PER_CLK_CYCLES (ADC_SAMPLE_PER_CLK_BASE_CYCLES + (4 * ADC_SAMPLE_DELAY))

// Max sample rates without interrupts
#define ADC_SAFE_SAMPLE_PER ADC_SAMPLE_PER_CLK_CYCLES
#define ADC_SAFE_SAMPLE_PER_MULT (ADC_SAFE_SAMPLE_PER + 40)

// Default sampling period
#define ADC_SAMPLE_CYCLES_DEF ADC_SAFE_SAMPLE_PER

#define ADC_FRAME_PREAMBLE_gc 0x40
#define ADC_FRAME_ALTERNATEPIN_bm (0x01 << 5)

volatile bool ADC_SAMPLE_REQ = false;
volatile bool ADC_UPDATE_REQ = false;
volatile bool ADC_MULTIPLEX_REQ = false;

#define ADC_SAMPLE_LOOP_WAIT_FOR_TIMER                \
while (!(TCA0.SINGLE.INTFLAGS & TCA_SINGLE_OVF_bm)) {}        \
TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm

#define ADC_SAMPLE_LOOP_PROCESS_RESULT                \
image_sync = VPORTB.IN & (PIN2_bm | PIN3_bm); \
usart_transmit(ADC0.RESL);                      \
usart_transmit(adc_read_h | ADC0.RESH)

static volatile uint16_t ADC_SAMPLE_CYCLES_LAST_PER  = ADC_SAMPLE_CYCLES_DEF;

// void adc_sample_loop(void)
//
// When called, this function will sample analog inputs and transmit them with USART
// while the global variable ADC_SAMPLE_LOOP_REQ is true. If ADC_MULTIPLEX_REQ is true
// when the function is first called, then AIN8 and AIN9 will be sampled in a round robin
// fashion. Otherwise, only AIN8 will be sampled.
//
// ADC reads are transmitted in two bytes. The LSB of the ADC read is transmitted in the first byte.
// The second byte is [ffprssaa], most significant bit on the left, where ff is the frame indicator (0x40),
// p is the adc input bit (0 for AIN8, 1, for AIN9), r is reserved (intended for overflow), ss are the sync variables (LV - msb, FV -lsb)
// from frame enable and line enable, and aa are the two most significant bits from the ADC read, least significant bit in
// the least significant position.
//
// The USART does not use inverted output (e.g. low is 0), and transmits the least significant bit first.
static void adc_sample_loop(void);

static void adc_sample_loop(void)
{
  bool second_pin = false; // tracks which ADC input to sample
  uint8_t image_sync = VPORTB.IN & (PIN2_bm | PIN3_bm); // Holds frame and line enable input from imaging sensor
  uint8_t adc_read_h = ADC_FRAME_PREAMBLE_gc | image_sync; // high byte of adc read, which will be combined with header data
  
  if (ADC_MULTIPLEX_REQ) { // Alternate between two inputs
    do {
      ADC_SAMPLE_LOOP_WAIT_FOR_TIMER; // See macro above
      ADC_SAMPLE_LOOP_PROCESS_RESULT; // See macro above
      // Alternate inputs
      if (second_pin) {
        ADC0.MUXPOS = ADC_MUXPOS_AIN8_gc;
        second_pin = false;
        adc_read_h = ADC_FRAME_PREAMBLE_gc | image_sync; // This frame is still the second pin
      }
      else {
        ADC0.MUXPOS = ADC_MUXPOS_AIN9_gc;
        second_pin = true;
        adc_read_h = ADC_FRAME_PREAMBLE_gc | ADC_FRAME_ALTERNATEPIN_bm | image_sync;
      }
    } while (ADC_SAMPLE_REQ);
  }
  else {
    do {
      ADC_SAMPLE_LOOP_WAIT_FOR_TIMER; // See macro above
      ADC_SAMPLE_LOOP_PROCESS_RESULT; // See macro above
      adc_read_h = ADC_FRAME_PREAMBLE_gc | image_sync;
    } while (ADC_SAMPLE_REQ);
  }
}

void adc_set_sample_per(uint16_t req_period)
{
  uint16_t safe_val = ADC_MULTIPLEX_REQ ? ADC_SAFE_SAMPLE_PER_MULT : ADC_SAFE_SAMPLE_PER;
  uint16_t this_val = (req_period < safe_val) ? safe_val : req_period;
  ADC_SAMPLE_CYCLES_LAST_PER = this_val;
  TCA0.SINGLE.PERBUF = this_val;
}

uint16_t adc_get_sample_per(void)
{
  return TCA0.SINGLE.PER;
}

static void ref_init(void)
{
  VREF.CTRLA = VREF_ADC0REFSEL_1V1_gc;
}

static void sample_timer_init(void)
{
  /* Default registers:
   * CTRLA
   * CTRLB
   * CTRLC
   * CTRLD
   * CTRLE
   * CTRLF
   * EVCTRL
   */
  adc_set_sample_per(ADC_SAMPLE_CYCLES_DEF);
}

static void sample_timer_enable(void)
{
  TCA0.SINGLE.CNT = 0x0000;
  TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
  TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
  TCA0.SINGLE.CTRLESET = TCA_SINGLE_CMD_UPDATE_gc;
}

static void sample_timer_disable(void)
{
  TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm;
}

static void adc_event_init(void)
{
  // Connect TCA0 overflow to ADC on sync channel 0
  EVSYS.SYNCCH0 = EVSYS_SYNCCH0_TCA0_OVF_LUNF_gc; // connect TCA0 overflow output to sync channel 0
  EVSYS.ASYNCUSER1 = EVSYS_ASYNCUSER1_SYNCCH0_gc; // connect ADC event input to synch channel 0
}

void adc_init(void)
{
  /* ADC configuration
   * 10 bit reads, no accumulation
   * ADC clock 1.25 MHz (1.5 MHz max at 10 bit resolution)
   */
  PORTB.PIN4CTRL = PORT_ISC_INPUT_DISABLE_gc; //ain9
  PORTB.PIN5CTRL = PORT_ISC_INPUT_DISABLE_gc; //ain8

  ref_init();

  /* Default registers:
   * CTRLA
   * CTRLB
   * CTRLD
   * CTRLE
   * SAMPLCTRL
   */

  ADC0.CTRLC = ADC_SAMPCAP_bm | ADC_PRESC_DIV4_gc; // implicitly use internal ref,
  ADC0.EVCTRL = ADC_STARTEI_bm;
  ADC0.SAMPCTRL = ADC_SAMPLE_DELAY;
  sample_timer_init();
  adc_event_init();
  usart_init();
}

void adc_enable(void)
{
  usart_enable();
  ADC0.MUXPOS = ADC_MUXPOS_AIN8_gc;
  ADC0.CTRLA |= ADC_ENABLE_bm;
  sample_timer_enable();
}

void adc_disable(void)
{
  sample_timer_disable();
  ADC0.CTRLA &= ~ADC_ENABLE_bm;
  usart_disable();
}

void adc_process_reqs(void) {
    if (ADC_SAMPLE_REQ) {
        adc_set_sample_per(ADC_SAMPLE_CYCLES_LAST_PER); // make sure safe value is used
        adc_enable();
        adc_sample_loop(); // will not return until ADC_SAMPLE_REQ is false
        adc_disable();
    } else if (ADC_UPDATE_REQ) {
        ADC_UPDATE_REQ = false;
        ADC_SAMPLE_REQ = true;
    }
}
