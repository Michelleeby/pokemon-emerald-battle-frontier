    .syntax unified
    .arm
    .global Init
Init:
    mov r0, #0x1F
    msr cpsr_cf, r0
    ldr sp, =0x03007E00
    ldr r0, =AgbMain + 1
    bx r0

    .thumb
    .thumb_func
    .global TestExit
TestExit:
    swi 0x0F
1:
    b 1b
