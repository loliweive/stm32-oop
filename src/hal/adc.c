/**
 * @file    adc.c
 * @brief   ADC 驱动 — 使用 CMSIS 位定义
 */
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

    /* ADC 预分频: PCLK2/6 = 12MHz @72MHz (max 14MHz) */
    RCC->CFGR = (RCC->CFGR & ~(3<<14)) | (2<<14);  /* ADCPRE=10 → /6 */
    for (volatile int i = 0; i < 100; i++) __asm__("nop");

    /* ADON: 上电 ADC */
    a->CR2 |= ADC_CR2_ADON;
    /* tSTAB: 上电稳定时间 (~10µs) */
    for (volatile int i = 0; i < 1000; i++) {}

    /* CAL: 校准 ADC */
    a->CR2 |= ADC_CR2_CAL;
    while (a->CR2 & ADC_CR2_CAL) {}  /* 等待校准完成 */
    self->_init = 1;
}

uint16_t adc_read(AdcPort *self, uint8_t channel)
{
    ADC_Type *a = (ADC_Type *)self->adc;

    /* 通道范围检查: STM32F103C8T6 有 10 个外部通道 + 内部通道 */
    if (channel > 17) return 0xFFFF;

    /* 设置规则序列通道 */
    a->SQR3 = channel & 0x1F;

    /* 配置采样时间: 55.5 cycles (默认, 适合大多数场景) */
    if (channel < 10) {
        uint32_t shift = channel * 3;
        a->SMPR2 = (a->SMPR2 & ~(0x7UL << shift))
                 | (ADC_SMPR_SMP_55_5 << shift);
    } else {
        uint32_t shift = (channel - 10) * 3;
        a->SMPR1 = (a->SMPR1 & ~(0x7UL << shift))
                 | (ADC_SMPR_SMP_55_5 << shift);
    }

    /* ADON: 启动转换 (第二次写 ADON 在已上电时启动转换) */
    a->CR2 |= ADC_CR2_ADON;
    while (!(a->SR & ADC_SR_EOC)) {}  /* 等待转换完成 */
    return (uint16_t)(a->DR & 0xFFFF);
}
