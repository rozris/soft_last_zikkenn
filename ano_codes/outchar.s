.global outbyte
.section .text
.even
outbyte:
movem.l%d0-%d3/%a0, -(%sp)
outbyte_ex:
move.l #2, %D0
move.l %sp, %a0
adda.l #28, %a0
move.l (%a0), %D1 | ch = fd
 move.l %sp, %d2
add.l #27, %d2
move.l #1, %D3 | size = 1
trap #0
cmp.l #1, %d0
bne outbyte_ex
movem.l (%sp)+, %d0-%d3/%a0
rts
