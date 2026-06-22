/* Bootloader linker: 8KB at Flash start (0x08000000 - 0x08002000) */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 8K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}

_estack = ORIGIN(SRAM) + LENGTH(SRAM);

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) . = ALIGN(4); } > FLASH
    .text : {
        . = ALIGN(4);
        *(.text) *(.text*) *(.rodata) *(.rodata*)
        KEEP(*(.init)) KEEP(*(.fini))
        . = ALIGN(4); _etext = .;
    } > FLASH
    .ARM.extab : { *(.ARM.extab*) } > FLASH
    .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
    _sidata = LOADADDR(.data);
    .data : { . = ALIGN(4); _sdata = .; *(.data) *(.data*) . = ALIGN(4); _edata = .; } > SRAM AT> FLASH
    .bss (NOLOAD) : { . = ALIGN(4); _sbss = .; *(.bss) *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; } > SRAM
    end = .;
}
