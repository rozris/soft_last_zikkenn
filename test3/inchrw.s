.global inbyte
.global inkey

.equ SYSCALL_NUM_GETSTRING, 1
.section .text
.even

inbyte:
movem.l %d1-%d3, -(%sp)
i_loop:
move.l #SYSCALL_NUM_GETSTRING, %d0
move.l %sp, %a0
adda.l #20, %a0
move.l (%a0), %D1 | ch = fd 
move.l #BUF_in, %d2 /* p = #in */
move.l #1, %d3 /* size = 1 */
trap #0

 cmpi.l #1, %d0
 bne i_loop /* 文字が取り出されなかったらループ */

 move.b BUF_in, %d0 /* 文字を d0 レジスタに格納 */

 movem.l (%sp)+, %d1-%d3
 rts


.section .bss
.even
BUF_in:
.ds.b 1
.even


.section .text
.even

inkey:

    movem.l %d1-%d3, -(%sp)

	move.l #SYSCALL_NUM_GETSTRING, %d0
	move.l %sp, %a0
	adda.l #20, %a0
	move.l (%a0), %D1 | ch = fd 
	move.l #BUF_in, %d2 /* p = #in */
	move.l #1, %d3 /* size = 1 */
	trap #0
    	cmpi.l #1, %d0
    	beq .L_has_input 

	move.l #-1, %d0
	bra .L_inkey_exit

.L_has_input:
	move.b BUF_inkey, %d0 /* 文字を d0 レジスタに格納 */

.L_inkey_exit:
    movem.l (%sp)+, %d1-%d3
    rts

    .section .bss
    .even
BUF_inkey:
    .ds.b 1                 /*1文字分の作業領域  */
    .even
