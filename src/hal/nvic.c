#include "core_cm3.h"

void NVIC_EnableIRQ(uint8_t irq)
{
    NVIC->ISER[irq >> 5] = (1UL << (irq & 0x1F));
}

void NVIC_DisableIRQ(uint8_t irq)
{
    NVIC->ICER[irq >> 5] = (1UL << (irq & 0x1F));
}

void NVIC_SetPriority(uint8_t irq, uint8_t priority)
{
    NVIC->IP[irq] = (uint8_t)((priority << 4) & 0xF0);
}
