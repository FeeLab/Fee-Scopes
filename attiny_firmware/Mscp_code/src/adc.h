#ifndef ADC_H
#define ADC_H

#include <stdint.h>
#include <stdbool.h>

extern volatile bool ADC_SAMPLE_REQ;
extern volatile bool ADC_UPDATE_REQ;
extern volatile bool ADC_MULTIPLEX_REQ;

void adc_init(void);
void adc_enable(void);
void adc_disable(void);

void adc_process_reqs(void);

void adc_set_sample_per(uint16_t req_period);
uint16_t adc_get_sample_per(void);

#endif
