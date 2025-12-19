.global inbyte
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
    /* レジスタの退避 (d1-d3を使用するため) [cite: 247] /
    movem.l %d1-%d3, -(%sp)

    / GETSTRING システムコールの準備 [cite: 321] /
    move.l #SYSCALL_NUM_GETSTRING, %d0
    move.l #0, %d1          / ポート番号 0 (ch=0) [cite: 321] /
    move.l #BUF_inkey, %d2  / 格納先アドレス /
    move.l #1, %d3          / 読み込みサイズ 1バイト /
    trap #0                 / システムコール発行 [cite: 313, 314] /

    / 戻り値 %d0 の評価 (読み込めたバイト数が入っている) [cite: 329] /
    cmpi.l #1, %d0
    beq .L_has_input        / 1バイト読み込めたら処理へ /

    / 入力がない場合: -1 をセットして終了  /
    move.l #-1, %d0
    bra .L_inkey_exit

.L_has_input:
    / 入力がある場合: 0x000000ff でマスクして正の int 型として返す  /
    clr.l %d0               / d0をクリア /
    move.b BUF_inkey, %d0   / 読み込んだ文字を格納 /
    andi.l #0x000000FF, %d0 / 確実に下位8bitのみにする /

.L_inkey_exit:
    / 退避したレジスタの復元 [cite: 248] /
    movem.l (%sp)+, %d1-%d3
    rts

    .section .bss
    .even
BUF_inkey:
    .ds.b 1                 / 1文字分の作業領域  */
    .even
