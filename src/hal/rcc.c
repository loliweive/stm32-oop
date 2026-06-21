#include "rcc.h"
#include "stm32f103xb.h"

void rcc_set_sysclk(RccClockSource src, uint8_t pll_mul)
{
    /* 1. Enable HSE (if using) */
    if (src == RCC_HSE || src == RCC_PLL) {
        RCC->CR |= RCC_CR_HSEON;
        while (!(RCC->CR & RCC_CR_HSERDY)) {}
    }

    /* 2. Configure PLL */
    if (pll_mul >= 2 && pll_mul <= 16) {
        uint32_t cfgr = RCC->CFGR;
        cfgr &= ~RCC_CFGR_PLLMUL_Msk;
        cfgr |= RCC_CFGR_PLLMUL(pll_mul);
        if (src == RCC_HSE || src == RCC_PLL) {
            cfgr |= RCC_CFGR_PLLSRC; /* HSE as PLL source */
        }
        RCC->CFGR = cfgr;

        /* Enable PLL */
        RCC->CR |= RCC_CR_PLLON;
        while (!(RCC->CR & RCC_CR_PLLRDY)) {}
    }

    /* 3. Switch SYSCLK to desired source */
    uint32_t cfgr = RCC->CFGR;
    cfgr &= ~0x3; /* Clear SW */
    cfgr |= (src == RCC_PLL) ? RCC_CFGR_SW_PLL :
            (src == RCC_HSE) ? RCC_CFGR_SW_HSE : RCC_CFGR_SW_HSI;
    RCC->CFGR = cfgr;
    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) !=
           ((src == RCC_PLL) ? (RCC_CFGR_SW_PLL << 2) :
            (src == RCC_HSE) ? (RCC_CFGR_SW_HSE << 2) : 0)) {}
}

void rcc_get_config(RccConfig *cfg)
{
    uint32_t cfgr = RCC->CFGR;
    uint32_t sw   = (cfgr & RCC_CFGR_SWS_Msk) >> 2;

    switch (sw) {
        case RCC_CFGR_SW_PLL: {
            uint32_t pll_src = (cfgr & RCC_CFGR_PLLSRC) ? 8000000 : 4000000;
            uint32_t mul     = ((cfgr & RCC_CFGR_PLLMUL_Msk) >> 18) + 2;
            cfg->sysclk_hz   = pll_src * mul;
            break;
        }
        case RCC_CFGR_SW_HSE:
            cfg->sysclk_hz = 8000000;
            break;
        default:
            cfg->sysclk_hz = 8000000; /* HSI */
            break;
    }

    cfg->hclk_hz  = cfg->sysclk_hz;                            /* AHB = SYSCLK */
    cfg->pclk2_hz = cfg->hclk_hz;                              /* APB2 = AHB */
    cfg->pclk1_hz = cfg->hclk_hz > 36000000 ? cfg->hclk_hz / 2 : cfg->hclk_hz; /* APB1 ≤ 36MHz */
}

void rcc_enable_gpio(char port)
{
    switch (port) {
        case 'A': RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; break;
        case 'B': RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; break;
        case 'C': RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; break;
    }
}

void rcc_enable_usart(uint8_t n)
{
    switch (n) {
        case 1: RCC->APB2ENR |= RCC_APB2ENR_USART1EN; break;
        case 2: RCC->APB1ENR |= RCC_APB1ENR_USART2EN; break;
        case 3: RCC->APB1ENR |= RCC_APB1ENR_USART3EN; break;
    }
}

void rcc_enable_timer(uint8_t n)
{
    switch (n) {
        case 1: RCC->APB2ENR |= RCC_APB2ENR_TIM1EN; break;
        case 2: RCC->APB1ENR |= RCC_APB1ENR_TIM2EN; break;
        case 3: RCC->APB1ENR |= RCC_APB1ENR_TIM3EN; break;
        case 4: RCC->APB1ENR |= RCC_APB1ENR_TIM4EN; break;
    }
}

void rcc_enable_i2c(uint8_t n)
{
    if (n == 1) RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    if (n == 2) RCC->APB1ENR |= RCC_APB1ENR_I2C2EN;
}

void rcc_enable_spi(uint8_t n)
{
    if (n == 1) RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    if (n == 2) RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
}

void rcc_enable_adc(uint8_t n)
{
    if (n == 1) RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
}
