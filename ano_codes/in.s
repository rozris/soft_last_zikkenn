.global inbyte
.section .text
.even
inbyte:
movem.l%d1-%d3/%a0, -(%sp)
inbyte_ex:
move.l #1, %D0
move.l %sp, %a0
adda.l #20, %a0
move.l (%a0), %D1 | ch = fd
move.l #BUF_i, %D2 | p = BUF_i
move.l #1, %D3 | size = 1
trap #0
cmp.l #1, %d0
bne inbyte_ex
move.b BUF_i, %d0
movem.l(%sp)+, %d1-%d3/%a0
rts
.section .bss
.even
BUF_i: .ds.b 1
