/**
 * @file    adc.c
 * @brief   ADC 驱动 — 使用 CMSIS 位定义
 */
#include "adc.h"
#include "stm32f1xx_hal.h"

void AdcPort_ctor(AdcPort *self, void *adc)
{
    self->adc  = adc;
    self->_init = 0;
}

void adc_init(AdcPort *self)
{
    ADC_TypeDef *a = (ADC_TypeDef *)self->adc;

    /* ADC 预分频: PCLK2/6 = 12MHz @72MHz (max 14MHz) */
    RCC->CFGR = (RCC->CFGR & ~(3<<14)) | (2<<14);  /* ADCPRE=10 → /6 */
    { uint32_t _to = 10000; while (--_to) {} }

    /* ADON: 上电 ADC */
    a->CR2 |= ADC_CR2_ADON;
    /* tSTAB: 上电稳定时间 (~10µs), 超时保护 */
    { uint32_t _to = 10000; while (--_to) {} }

    /* CAL: 校准 ADC (带超时保护 + 恢复) */
    a->CR2 |= ADC_CR2_CAL;
    { uint32_t _to = 100000;
      while ((a->CR2 & ADC_CR2_CAL) && --_to) {}
      if (!_to) {
          /* 校准超时 — 复位校准电路, 标记失败 */
          a->CR2 |= ADC_CR2_RSTCAL;
          self->_init = 0;
          return;
      }
    }
    self->_init = 1;
}

uint16_t adc_read(AdcPort *self, uint8_t channel)
{
    ADC_TypeDef *a = (ADC_TypeDef *)self->adc;

    /* 通道范围检查: STM32F103C8T6 有 10 个外部通道 + 内部通道 */
    if (channel > 17) return 0xFFFF;

    /* 设置规则序列通道 */
    a->SQR3 = channel & 0x1F;

    /* 配置采样时间: 55.5 cycles (默认, 适合大多数场景) */
    if (channel < 10) {
        uint32_t shift = channel * 3;
        a->SMPR2 = (a->SMPR2 & ~(0x7UL << shift))
                 | (ADC_SAMPLETIME_55CYCLES_5 << shift);
    } else {
        uint32_t shift = (channel - 10) * 3;
        a->SMPR1 = (a->SMPR1 & ~(0x7UL << shift))
                 | (ADC_SAMPLETIME_55CYCLES_5 << shift);
    }

    /* ADON: 启动转换 (第二次写 ADON 在已上电时启动转换) */
    a->CR2 |= ADC_CR2_ADON;
    { uint32_t _to = 100000;
      while (!(a->SR & ADC_SR_EOC) && --_to) {}
      if (!_to) {
          /* 转换超时 — 停止转换, 返回错误标记 */
          a->CR2 &= ~ADC_CR2_ADON;  /* 停止 ADC (ADON=0) */
          return 0xFFFF;
      }
    }
    return (uint16_t)(a->DR & 0xFFFF);
}
