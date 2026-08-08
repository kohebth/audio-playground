/* STM32F729 RAM Memory Map (Internal SRAM + External 8MB SDRAM/DRAM) */
MEMORY
{
  ITCM (xrw)   : ORIGIN = 0x00000000, LENGTH = 16K
  FLASH (rx)   : ORIGIN = 0x08000000, LENGTH = 1024K
  DTCM (rw)    : ORIGIN = 0x20000000, LENGTH = 64K
  SRAM1 (xrw)  : ORIGIN = 0x20020000, LENGTH = 368K
  SRAM2 (xrw)  : ORIGIN = 0x2007C000, LENGTH = 16K
  SDRAM (xrw)  : ORIGIN = 0xC0000000, LENGTH = 8M   /* 8MB External DRAM */
}

/* Highest address of the user stack (Top of 64KB DTCM RAM) */
_estack = ORIGIN(DTCM) + LENGTH(DTCM);

/* Minimum Stack and Heap Sizes */
_Min_Heap_Size = 0x2000;  /* 8KB Heap */
_Min_Stack_Size = 0x4000; /* 16KB Stack */

ENTRY(Reset_Handler)

SECTIONS
{
  .text :
  {
    . = ALIGN(512);
    KEEP(*(.isr_vector))
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
    _etext = .;
  } > FLASH

  .itcm : ALIGN(4)
  {
    _sitcm = .;
    *(.itcm*)
    *(.fast_code*)
    . = ALIGN(4);
    _eitcm = .;
  } > ITCM AT> FLASH
  _sitcm_init = LOADADDR(.itcm);

  .dtcm (NOLOAD) : ALIGN(4)
  {
    _sdtcm = .;
    *(.dtcm*)
    *(.fast_ram*)
    . = ALIGN(4);
    _edtcm = .;
  } > DTCM

  .data : ALIGN(4)
  {
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > DTCM AT> FLASH
  _sdata_init = LOADADDR(.data);

  .bss : ALIGN(4)
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > SRAM1

  .apg_m7_audio_buffers (NOLOAD) : ALIGN(32)
  {
    *(.apg_m7_signal_buffers)
    *(.apg_m7_state_buffers)
  } > SRAM2

  .sdram (NOLOAD) : ALIGN(32)
  {
    _ssdram = .;
    *(.sdram*)
    *(.ext_ram*)
    *(.apg_m7_dram*)
    . = ALIGN(32);
    _esdram = .;
  } > SDRAM

  /* User Heap and Stack Section */
  ._user_heap_stack : ALIGN(8)
  {
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > DTCM
}
