#include "adc.h"
#include "stm32f103xb.h"

void AdcPort_ctor(AdcPort *self, void *adc)
{
    self->adc  = adc;
    self->_init = 0;
}

void adc_init(AdcPort *self)
{
    ADC_Type *a = (ADC_Type *)self->adc;
    /* Turn on ADC */
    a->CR2 |= 0x01;  /* ADON */
    /* Wait for stabilization (~10µs at startup) */
    for (volatile int i = 0; i < 1000; i++) {}
    /* Calibrate */
    a->CR2 |= 0x04;  /* CAL */
    while (a->CR2 & 0x04) {}
    self->_init = 1;
}

uint16_t adc_read(AdcPort *self, uint8_t channel)
{
    ADC_Type *a = (ADC_Type *)self->adc;

    /* Set channel in regular sequence */
    a->SQR3 = channel & 0x1F;
    /* Set sample time (55.5 cycles for channel) */
    if (channel < 10) {
        uint32_t shift = channel * 3;
        a->SMPR2 = (a->SMPR2 & ~(0x7UL << shift)) | (0x5UL << shift);
    }

    /* Start conversion */
    a->CR2 |= 0x01;  /* ADON */
    while (!(a->SR & 0x02)) {}  /* Wait EOC */
    return (uint16_t)(a->DR & 0xFFFF);
}
