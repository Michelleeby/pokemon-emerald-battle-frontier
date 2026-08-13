    .syntax unified
    .thumb
    .thumb_func
    .global TestExit
TestExit:
    swi 0x0F
1:
    b 1b
