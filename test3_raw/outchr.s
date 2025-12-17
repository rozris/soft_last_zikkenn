.global outbyte
.equ SYSCALL_NUM_PUTSTRING, 2
.section .text
.even

outbyte:
	movem.l %d0-%d3/%a0, -(%sp)
o_loop:
	move.l #SYSCALL_NUM_PUTSTRING, %d0
	move.l %sp,%a0
 	add.l #27, %a0 /*ch=%sp + 27 */
	move.l (%a0), %d1 /* ch = a0 */
 	move.l %sp, %d2
 	addi.l #28, %d2 /*p=%sp + 19 */
 	move.l #1, %d3 /* size = 1 */
 	trap #0

 	cmpi.l #1, %d0
 	bne o_loop /* 文字が送信されなかったらループ */

 	movem.l (%sp)+, %d0-%d3/%a0
 rts
