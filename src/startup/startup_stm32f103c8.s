/**
 * Minimal startup for STM32F103C8T6 (Cortex-M3).
 * Sets up .data and .bss, then calls main().
 */
.syntax unified
.cpu cortex-m3
.thumb

.section .isr_vector, "a"
.global _estack
.global Reset_Handler

g_pfnVectors:
    .word _estack               /* Initial stack pointer */
    .word Reset_Handler          /* Reset */
    .word NMI_Handler            /* NMI */
    .word HardFault_Handler      /* HardFault */
    .word MemManage_Handler      /* MemManage */
    .word BusFault_Handler       /* BusFault */
    .word UsageFault_Handler     /* UsageFault */
    .word 0                      /* Reserved */
    .word 0                      /* Reserved */
    .word 0                      /* Reserved */
    .word 0                      /* Reserved */
    .word SVC_Handler            /* SVCall */
    .word DebugMon_Handler       /* Debug Monitor */
    .word 0                      /* Reserved */
    .word PendSV_Handler         /* PendSV */
    .word SysTick_Handler        /* SysTick */

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

/* Default weak handlers */
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

Default_Handler:
    b .
