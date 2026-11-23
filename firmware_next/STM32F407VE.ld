ENTRY(Reset_Handler)
MEMORY {
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 256K
    RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
    CCM (xrw) : ORIGIN = 0x10000000, LENGTH = 64K
}
/* Flash sectors 6/7 (0x08040000-0x0807ffff) are reserved for calibration. */
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
    .isr_vector : { . = ALIGN(512); KEEP(*(.isr_vector)) } > FLASH
    .text : { *(.text*) *(.rodata*) KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); } > FLASH
    .ARM.extab : { *(.ARM.extab*) } > FLASH
    .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
    .data : {
        . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .;
    } > RAM AT > FLASH
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) : {
        . = ALIGN(8); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(8); _ebss = .;
    } > RAM
    .dma_buffer (NOLOAD) : {
        . = ALIGN(4); _sdma = .; *(.dma_buffer*) . = ALIGN(4); _edma = .;
    } > RAM
    _end = .; end = .;
    ASSERT(_end <= _estack - 16384, "SRAM overflow: reserve 16KB stack")
    ASSERT(_sdma >= 0x20000000 && _edma <= 0x20020000, "DMA buffer must be SRAM")
    /DISCARD/ : { *(.note.GNU-stack) *(.comment) }
}
