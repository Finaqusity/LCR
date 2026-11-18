/*
 * STM32F407VET6 链接脚本（终版，§8.1/§8.2）。
 *   FLASH 256 KB 代码区（0x08000000–0x0803FFFF）；sector6/7（0x08040000+）
 *   留给校准 A/B 日志，由 cal.c 直接按地址访问，不在本脚本建段。
 *   RAM 128 KB（SRAM1+SRAM2）；CCM 64 KB 仅放相位表/热循环（NOLOAD）。
 *   栈 4 KB，__StackLimit = _estack − 0x1000，heap = 0（禁 malloc）。
 *   .dma_buf 必须在 SRAM（DMA 不可访问 CCM），NOLOAD + KEEP。
 *   .ramfunc 运行于 SRAM、装载于 Flash，启动时由 startup 复制（cal_flash_op）。
 */
ENTRY(Reset_Handler)

_estack       = ORIGIN(RAM) + LENGTH(RAM);   /* 0x20020000 */
__StackLimit  = _estack - 0x1000;            /* 0x2001F000，栈保护区下界 */
_Min_Heap_Size  = 0x0;                       /* heap=0 */
_Min_Stack_Size = 0x1000;                    /* 4 KB 主栈 */

MEMORY
{
  RAM    (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  CCMRAM (rw)  : ORIGIN = 0x10000000, LENGTH = 64K
  FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 256K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP (*(.init))
    KEEP (*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab :
  {
    . = ALIGN(4);
    *(.ARM.extab* .gnu.linkonce.armextab.*)
    . = ALIGN(4);
  } >FLASH

  .ARM :
  {
    . = ALIGN(4);
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
    . = ALIGN(4);
  } >FLASH

  .preinit_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .init_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
    . = ALIGN(4);
  } >FLASH

  .fini_array :
  {
    . = ALIGN(4);
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
    . = ALIGN(4);
  } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT> FLASH

  /* Flash→SRAM 复制的常驻代码（cal_flash_op：擦除时不能取指 Flash）。 */
  _siramfunc = LOADADDR(.ramfunc);
  .ramfunc :
  {
    . = ALIGN(4);
    _sramfunc = .;
    KEEP(*(.ramfunc))
    KEEP(*(.ramfunc*))
    . = ALIGN(4);
    _eramfunc = .;
  } >RAM AT> FLASH

  /* ADC DMA 双缓冲：2×1024×u32 = 8 KB，NOLOAD，DMA 不可访问 CCM。 */
  .dma_buf (NOLOAD) :
  {
    . = ALIGN(4);
    KEEP(*(.dma_buf))
    KEEP(*(.dma_buf*))
    . = ALIGN(4);
  } >RAM

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /* CCM：相位表/热循环（NOLOAD，运行期算）。 */
  .ccmram (NOLOAD) :
  {
    . = ALIGN(4);
    KEEP(*(.ccmram))
    KEEP(*(.ccmram*))
    . = ALIGN(4);
  } >CCMRAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  .ARM.attributes 0 : { *(.ARM.attributes) }

  /* 链接期断言（比 POST_BUILD 更早失败，§8.2）。 */
  ASSERT(_ebss <= __StackLimit,
         "RAM overflow: .bss/.dma_buf/.ramfunc intrude into 4K stack guard")
  ASSERT(_sidata + (_edata - _sdata) <= 0x08040000,
         "code+data exceeds 256K code region (cal sectors start 0x08040000)")
  ASSERT(_siramfunc + (_eramfunc - _sramfunc) <= 0x08040000,
         "code+data+ramfunc exceeds 256K code region")
  ASSERT(__StackLimit == _estack - 0x1000,
         "stack guard must be exactly 4K below _estack")
}