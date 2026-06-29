/**
 * Startup for STM32F103C8T6 (Cortex-M3).
 * Full vector table — STM32 HAL expects all IRQ handler names.
 */
.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector, "a"
.global _estack
.global Reset_Handler

g_pfnVectors:
    .word _estack               /*  0: Initial SP */
    .word Reset_Handler          /*  1: Reset */
    .word NMI_Handler            /*  2: NMI */
    .word HardFault_Handler      /*  3: HardFault */
    .word MemManage_Handler      /*  4: MemManage */
    .word BusFault_Handler       /*  5: BusFault */
    .word UsageFault_Handler     /*  6: UsageFault */
    .word 0                      /*  7: Reserved */
    .word 0                      /*  8: Reserved */
    .word 0                      /*  9: Reserved */
    .word 0                      /* 10: Reserved */
    .word SVC_Handler            /* 11: SVCall */
    .word DebugMon_Handler       /* 12: Debug Monitor */
    .word 0                      /* 13: Reserved */
    .word PendSV_Handler         /* 14: PendSV */
    .word SysTick_Handler        /* 15: SysTick */

    /* Peripheral interrupts (STM32F103) */
    .word WWDG_IRQHandler        /* 16: Window Watchdog */
    .word PVD_IRQHandler         /* 17: PVD */
    .word TAMPER_IRQHandler      /* 18: Tamper */
    .word RTC_IRQHandler         /* 19: RTC */
    .word FLASH_IRQHandler       /* 20: Flash */
    .word RCC_IRQHandler         /* 21: RCC */
    .word EXTI0_IRQHandler       /* 22: EXTI Line 0 */
    .word EXTI1_IRQHandler       /* 23: EXTI Line 1 */
    .word EXTI2_IRQHandler       /* 24: EXTI Line 2 */
    .word EXTI3_IRQHandler       /* 25: EXTI Line 3 */
    .word EXTI4_IRQHandler       /* 26: EXTI Line 4 */
    .word DMA1_Channel1_IRQHandler /* 27: DMA1 Ch1 */
    .word DMA1_Channel2_IRQHandler /* 28: DMA1 Ch2 */
    .word DMA1_Channel3_IRQHandler /* 29: DMA1 Ch3 */
    .word DMA1_Channel4_IRQHandler /* 30: DMA1 Ch4 */
    .word DMA1_Channel5_IRQHandler /* 31: DMA1 Ch5 */
    .word DMA1_Channel6_IRQHandler /* 32: DMA1 Ch6 */
    .word DMA1_Channel7_IRQHandler /* 33: DMA1 Ch7 */
    .word ADC1_2_IRQHandler      /* 34: ADC1/2 */
    .word USB_HP_CAN1_TX_IRQHandler /* 35: USB HP / CAN TX */
    .word USB_LP_CAN1_RX0_IRQHandler /* 36: USB LP / CAN RX0 */
    .word CAN1_RX1_IRQHandler    /* 37: CAN1 RX1 */
    .word CAN1_SCE_IRQHandler    /* 38: CAN1 SCE */
    .word EXTI9_5_IRQHandler     /* 39: EXTI Line 9-5 */
    .word TIM1_BRK_IRQHandler    /* 40: TIM1 Break */
    .word TIM1_UP_IRQHandler     /* 41: TIM1 Update */
    .word TIM1_TRG_COM_IRQHandler /* 42: TIM1 Trigger/Commutation */
    .word TIM1_CC_IRQHandler     /* 43: TIM1 Capture Compare */
    .word TIM2_IRQHandler        /* 44: TIM2 */
    .word TIM3_IRQHandler        /* 45: TIM3 */
    .word TIM4_IRQHandler        /* 46: TIM4 */
    .word I2C1_EV_IRQHandler     /* 47: I2C1 Event */
    .word I2C1_ER_IRQHandler     /* 48: I2C1 Error */
    .word I2C2_EV_IRQHandler     /* 49: I2C2 Event */
    .word I2C2_ER_IRQHandler     /* 50: I2C2 Error */
    .word SPI1_IRQHandler        /* 51: SPI1 */
    .word SPI2_IRQHandler        /* 52: SPI2 */
    .word USART1_IRQHandler      /* 53: USART1 */
    .word USART2_IRQHandler      /* 54: USART2 */
    .word USART3_IRQHandler      /* 55: USART3 */
    .word EXTI15_10_IRQHandler   /* 56: EXTI Line 15-10 */
    .word RTCAlarm_IRQHandler    /* 57: RTC Alarm */
    .word USBWakeUp_IRQHandler   /* 58: USB Wakeup */

.section .text.Reset_Handler, "ax"
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
    ldr sp, =_estack

    /* Copy .data from flash to SRAM */
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata
    cmp r1, r2
    bcs fill_bss
copy_data:
    ldr r3, [r0], #4
    str r3, [r1], #4
    cmp r1, r2
    bcc copy_data

fill_bss:
    ldr r1, =_sbss
    ldr r2, =_ebss
    movs r3, #0
    cmp r1, r2
    bcs call_main
zero_bss:
    str r3, [r1], #4
    cmp r1, r2
    bcc zero_bss

call_main:
    bl SystemInit
    bl main
    b .

/* Default weak handlers — one macro to rule them all */
.macro weak_handler name
    .weak \name
    .thumb_set \name, Default_Handler
.endm

weak_handler NMI_Handler
weak_handler HardFault_Handler
weak_handler MemManage_Handler
weak_handler BusFault_Handler
weak_handler UsageFault_Handler
weak_handler SVC_Handler
weak_handler DebugMon_Handler
weak_handler PendSV_Handler
weak_handler SysTick_Handler
weak_handler WWDG_IRQHandler
weak_handler PVD_IRQHandler
weak_handler TAMPER_IRQHandler
weak_handler RTC_IRQHandler
weak_handler FLASH_IRQHandler
weak_handler RCC_IRQHandler
weak_handler EXTI0_IRQHandler
weak_handler EXTI1_IRQHandler
weak_handler EXTI2_IRQHandler
weak_handler EXTI3_IRQHandler
weak_handler EXTI4_IRQHandler
weak_handler DMA1_Channel1_IRQHandler
weak_handler DMA1_Channel2_IRQHandler
weak_handler DMA1_Channel3_IRQHandler
weak_handler DMA1_Channel4_IRQHandler
weak_handler DMA1_Channel5_IRQHandler
weak_handler DMA1_Channel6_IRQHandler
weak_handler DMA1_Channel7_IRQHandler
weak_handler ADC1_2_IRQHandler
weak_handler USB_HP_CAN1_TX_IRQHandler
weak_handler USB_LP_CAN1_RX0_IRQHandler
weak_handler CAN1_RX1_IRQHandler
weak_handler CAN1_SCE_IRQHandler
weak_handler EXTI9_5_IRQHandler
weak_handler TIM1_BRK_IRQHandler
weak_handler TIM1_UP_IRQHandler
weak_handler TIM1_TRG_COM_IRQHandler
weak_handler TIM1_CC_IRQHandler
weak_handler TIM2_IRQHandler
weak_handler TIM3_IRQHandler
weak_handler TIM4_IRQHandler
weak_handler I2C1_EV_IRQHandler
weak_handler I2C1_ER_IRQHandler
weak_handler I2C2_EV_IRQHandler
weak_handler I2C2_ER_IRQHandler
weak_handler SPI1_IRQHandler
weak_handler SPI2_IRQHandler
weak_handler USART1_IRQHandler
weak_handler USART2_IRQHandler
weak_handler USART3_IRQHandler
weak_handler EXTI15_10_IRQHandler
weak_handler RTCAlarm_IRQHandler
weak_handler USBWakeUp_IRQHandler

Default_Handler:
    b .
