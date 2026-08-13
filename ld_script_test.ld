ENTRY(Start)

MEMORY
{
    EWRAM (rwx) : ORIGIN = 0x02000000, LENGTH = 256K
    IWRAM (rwx) : ORIGIN = 0x03000000, LENGTH = 32K
    ROM    (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
}

SECTIONS
{
    .text :
    {
        build/test/rom_header.o(.text*)
        build/test/runner/start.o(.text*)
        build/test/fixtures/*.o(.text*)
        build/test/runner/*.o(.text*)
        *(.rodata*)
    } > ROM = 0

    .data : { *(.data*) } > EWRAM AT > ROM
    .bss (NOLOAD) : { *(.bss*) *(COMMON) } > EWRAM

    /DISCARD/ : { *(.comment) *(.ARM.attributes) }
}
