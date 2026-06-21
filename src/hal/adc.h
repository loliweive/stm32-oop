/** OOP ADC driver (12-bit, single conversion). */

#ifndef ADC_HAL_H
#define ADC_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void    *adc;
    uint8_t  _init;
} AdcPort;

void AdcPort_ctor(AdcPort *self, void *adc);
void adc_init(AdcPort *self);
uint16_t adc_read(AdcPort *self, uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* ADC_HAL_H */
