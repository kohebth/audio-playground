ENTRY(main)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K
    DTCMRAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
    SRAM_D1 (xrw) : ORIGIN = 0x24000000, LENGTH = 512K
}

SECTIONS
{
    .text :
    {
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    .data :
    {
        . = ALIGN(4);
        *(.data*)
        . = ALIGN(4);
    } > SRAM_D1 AT > FLASH

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
    } > SRAM_D1

    .apg_m7_signal_buffers (NOLOAD) :
    {
        . = ALIGN(32);
        *(.apg_m7_signal_buffers)
        . = ALIGN(32);
    } > SRAM_D1

    .apg_m7_params (NOLOAD) :
    {
        . = ALIGN(32);
        *(.apg_m7_params)
        . = ALIGN(32);
    } > SRAM_D1

    .apg_m7_atom_storage (NOLOAD) :
    {
        . = ALIGN(32);
        *(.apg_m7_atom_storage)
        . = ALIGN(32);
    } > SRAM_D1

    .apg_m7_atom_calls (NOLOAD) :
    {
        . = ALIGN(32);
        *(.apg_m7_atom_calls)
        . = ALIGN(32);
    } > SRAM_D1

    .apg_m7_signal_arrays (NOLOAD) :
    {
        . = ALIGN(32);
        *(.apg_m7_signal_arrays)
        . = ALIGN(32);
    } > SRAM_D1

    .apg_m7_state_buffers (NOLOAD) :
    {
        . = ALIGN(32);
        *(.apg_m7_state_buffers)
        . = ALIGN(32);
    } > SRAM_D1
}
