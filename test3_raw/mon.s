*****************
** 各種レジスタ定義
*****************


** レジスタ群の先頭 ***************
	.equ REGBASE, 0xFFF000	/* DMAP を使用． */
	.equ IOBASE, 0x00d00000


** 割り込み関係のレジスタ***************
	.equ IVR, REGBASE+0x300	/* 割り込みベクタレジスタ */
	.equ IMR, REGBASE+0x304	/* 割り込みマスクレジスタ */
	.equ ISR, REGBASE+0x30c	/* 割り込みステータスレジスタ */
	.equ IPR, REGBASE+0x310	/* 割り込みペンディングレジスタ */

** タイマ関係のレジスタ ***************
	.equ TCTL1, REGBASE+0x600	/* タイマ１コントロールレジスタ */
	.equ TPRER1, REGBASE+0x602	/* タイマ１プリスケーラレジスタ */
	.equ TCMP1, REGBASE+0x604	/* タイマ１コンペアレジスタ */
	.equ TCN1, REGBASE+0x608	/* タイマ１カウンタレジスタ */
	.equ TSTAT1, REGBASE+0x60a	/* タイマ１ステータスレジスタ */


/* キューのオフセットと必要領域計算 */
.section .bss

	.equ	B_SIZE,	256	
	.equ	TOP,	0
	.equ	BOTTOM,	B_SIZE
	.equ	IN,	BOTTOM + 4
	.equ	OUT,	IN + 4
	.equ	S,	OUT + 4
	.equ	SIZE,	S + 2

/* キュー用のメモリ領域確保 */
QUEUES:		.ds.b	SIZE * 4


** UART1（送受信）関係のレジスタ 
	.equ USTCNT1, REGBASE+0x900	/* UART1 ステータス/コントロールレジスタ*/
	.equ UBAUD1, REGBASE+0x902	/* UART1 ボーコントロールレジスタ */
	.equ URX1, REGBASE+0x904	/* UART1 受信レジスタ */
	.equ UTX1, REGBASE+0x906	/* UART1 送信レジスタ */

***************
** LED
**ボード搭載の LED 用レジスタ,使用法については付録 A.4.3.1
***************
	.equ LED7, IOBASE+0x000002f
	.equ LED6, IOBASE+0x000002d 
	.equ LED5, IOBASE+0x000002b
	.equ LED4, IOBASE+0x0000029
	.equ LED3, IOBASE+0x000003f
	.equ LED2, IOBASE+0x000003d
	.equ LED1, IOBASE+0x000003b
	.equ LED0, IOBASE+0x0000039

********************
** スタック領域の確保 
********************
	.section .bss
	.even
SYS_STK:
	.ds.b 0x4000	/* システムスタック領域 */
	.even 
	SYS_STK_TOP: /*| システムスタック領域の最後尾 */
********************
** PUT/GETSTRING用変数の確保 
********************

sz:		.ds.l 1
i:		.ds.l 1

* SET_TIMER のコールバックルーチンポインタ
task_p:
    .ds.l   1       /* 4バイトの領域を確保 */



***************
** システムコール番号
***************
.equ SYSCALL_NUM_GETSTRING, 1
.equ SYSCALL_NUM_PUTSTRING, 2
.equ SYSCALL_NUM_RESET_TIMER, 3
.equ SYSCALL_NUM_SET_TIMER, 4




	.section .text
	.even
	
	.extern start
	.global monitor_begin
	.global BUF

monitor_begin:

***************************************************************
** 初期化 
** 内部デバイスレジスタには特定の値が設定されている． 
** その理由を知るには，付録 B にある各レジスタの仕様を参照すること． 
***************************************************************

	.section .text
	.even


boot: /* スーパーバイザ & 各種設定を行っている最中の割込禁止 */
	move.w #0x2700,%SR
	lea.l SYS_STK_TOP, %SP /* | Set SSP */
	
****************
** 割り込みコントローラの初期化
****************
	move.b #0x40, IVR	/* | ユーザ割り込みベクタ番号を| 0x40+level に設定． */
	move.l #0x00ffffff,IMR	/* | 全割り込みマスク */

****************
** 送受信 (UART1) 関係の初期化 (割り込みレベルは 4 に固定されている) 
****************
	move.w #0x0000, USTCNT1	/* | リセット */
	move.w #0xe100, USTCNT1	/* | 送受信可能, パリティなし, 1 stop, 8 bit, */
				/* | 送受割り込み禁止 */
	move.w #0x0038, UBAUD1	/* | baud rate = 230400 bps */


/* キューの初期化 */
QueueInitialize:
	movem.l	%d0-%d1/%a0-%a2,-(%sp)
	lea.l	QUEUES,%a0
	moveq	#4,%d0
	
InitLoop:
	movea.l	%a0,%a1
	lea.l	TOP(%a1),%a2
	move.l	%a2,IN(%a1)
	move.l	%a2,OUT(%a1)
	move.w	#0,S(%a1)
	adda.l	#SIZE,%a0
	subq.l	#1,%d0
	bne	InitLoop
	movem.l	(%sp)+,%d0-%d1/%a0-%a2
	


****************
** タイマ関係の初期化 (割り込みレベルは 6 に固定されている) 
*****************
	move.w #0x0004, TCTL1	/* restart, 割り込み不可 */
                        **システムクロックの 1/16 を単位として計時，
                        **タイマ使用停止

						
	* TRAP #0 ハンドラをベクタテーブルに登録
	move.l	#TRAP0_HANDLER, 0x0080
	* TRAP 0 (ベクタ番号 32) のアドレスにハンドラを設定 
	/* ★ 修正点 1: UART1 割り込み(レベル4, ベクタ68) ハンドラを設定 */
    /* ( IVR 0x40 + Level 4 = 0x44 (68), Address 68 * 4 = 0x110 ) */
    move.l  #send_or_receive, 0x110
    
    /* ★ 修正点 1: タイマ1 割り込み(レベル6, ベクタ70) ハンドラを設定 */
    /* ( IVR 0x40 + Level 6 = 0x46 (70), Address 70 * 4 = 0x118 ) */
    move.l  #timer1_interrupt, 0x118
    
    /* ★ 修正点 2: UART(レベル4)の割り込みマスクをIMRで解除 */
    /* (IMRのビット4を0にする)<-ちがう！！！ */
    andi.l  #0xfffffff9, IMR
	
	bra MAIN_INIT

** メイン (PUTSTRING / INTERPUT 動作テスト)
***************************************************************


.section .bss
.even
WORK: .ds.b 256        /* 受信データ格納用バッファ */
BUF:  .ds.b 256 /* BUF[256] */

.section .text
.even
MAIN_INIT:
	* 走行レベル0で開始（スーパバイザモード）
	move.w	#0x2000, %SR

    * UART 割り込み許可（送受信割り込みON）
	move.w  #0xE10C, USTCNT1

	jmp start



****************************************************************
*** プログラム領域
****************************************************************
.section .text
.even
MAIN:






.section .text
send_or_receive:
    movem.l	%d0-%d7/%a0-%a7,-(%sp) /* 安全のため全レジスタを退避 */

* --- 1. 受信処理を優先的にチェック ---
check_receive:
    move.w	URX1, %d3       /* URX1レジスタ(受信)を読み込み */
    move.b	%d3, %d2        /* %d2 にデータ部(bit 7-0)を先に保存 */
    andi.w	#0x2000, %d3    /* DATA READYフラグ(bit 13)をチェック */
    beq		check_send      /* フラグが0 (受信データなし) なら、送信チェックへ進む */

* --- 受信データがあった場合の処理 ---
receive_init:
    move.l	#0, %d1         /* ch=0 (UART1) */
    jsr		INTERGET        /* %d2のデータを使ってINTERGETを実行し、キューに入れる */
                            /* INTERGETは %d2 のデータを使用します */
    
* --- 2. 送信処理をチェック ---
check_send:
    move.w	UTX1, %d3       /* UTX1レジスタ(送信)を読み込み */
    andi.w 	#0x8000, %d3    /* FIFO EMPTYフラグ(bit 15)をチェック */ 
    beq		SoR_end         /* フラグが0 (送信不可) なら、ハンドラを終了 */

* --- 送信可能だった場合の処理 ---
send_init:
    move.l	#0, %d1         /* ch=0 (UART1) */
    jsr		INTERPUT        /* INTERPUTを実行し、キューからデータを取り出し送信 */

* --- 終了処理 ---
SoR_end:
    movem.l	(%sp)+, %d0-%d7/%a0-%a7 /* 退避したレジスタを復帰 */
    rte


***************************************************************
** TRAP #0 システムコールハンドラ 
** (Step 8: OSサービスの呼び出しに対応)
***************************************************************
TRAP0_HANDLER:
	
	* サービス番号（%D0）に応じて分岐する
	cmpi.l	#1, %d0					/* %D0 が 1 (GETSTRING) かを比較する。*/
	beq		SYSCALL_GETSTRING
	
	cmpi.l	#2, %d0					/* %D0 が 2 (PUTSTRING) かを比較する。*/
	beq		SYSCALL_PUTSTRING

	cmpi.l	#3, %d0					/* %D0 が 3 (RESET_TIMER) かを比較する。*/
	beq		SYSCALL_RESET_TIMER
	
	cmpi.l	#4, %d0					/* %D0 が 4 (SET_TIMER) かを比較する。*/
	beq		SYSCALL_SET_TIMER

	bra		TRAP0_EXIT				/* どのサービス番号にも一致しない場合、終了へ進む。*/

SYSCALL_GETSTRING:
	* GETSTRING(ch=0, p=%D2, size=%D3)
	move.l	#GETSTRING, %d0
	jsr		GETSTRING				/* GETSTRING を呼び出す。*/
	bra		TRAP0_EXIT
	
SYSCALL_PUTSTRING:
	* PUTSTRING(ch=0, p=%D2, size=%D3)
	move.l	#PUTSTRING, %d0
	jsr		PUTSTRING				/* PUTSTRING を呼び出す。*/
	bra		TRAP0_EXIT

SYSCALL_RESET_TIMER:
	* RESET_TIMER()
	move.l	#RESET_TIMER, %d0
	jsr		RESET_TIMER				/* RESET_TIMER を呼び出す。*/
	bra		TRAP0_EXIT
	
SYSCALL_SET_TIMER:
	* SET_TIMER(t=%D1, p=%D2)
	move.l	#SET_TIMER, %d0
	jsr		SET_TIMER				/* SET_TIMER を呼び出す。*/
	bra		TRAP0_EXIT

TRAP0_EXIT:
	rte								/* 例外から復帰する。*/


**-----------------------------------------------------------------------
** INQ：番号noのキューにデータを入れる
** 入力：	キュー番号 no -> %d0.l
**			書き込む8bitデータ data -> %d1.b108
** 戻り値：	失敗0/成功1 -> %d0.l
**----------------------------------------------------------------------
INQ:
	move.w	%SR,-(%sp)					/* (1) 現走行レベルの退避 */
	move.w	#0x2700,%SR					/* (2) 割り込み禁止(= 走行レベルを 7 に) */
	movem.l	%d2-%d3/%a1-%a3,-(%sp)		/* レジスタの退避 */
	lea.l	QUEUES,%a1					/* 指定された番号のキューのアドレスを計算 */
	mulu.w	#SIZE,%d0
	adda.l	%d0,%a1
	jsr	PUT_BUF							/* (3) ～ (6) */
	movem.l	(%sp)+,%d2-%d3/%a1-%a3		/* レジスタの回復 */
	move.w	(%sp)+,%SR					/* (7) 旧走行レベルの回復 */
	rts

PUT_BUF:
	move.l	#0,%d2
	move.w	S(%a1),%d3
	cmp.w	#B_SIZE,%d3					/* (3)  s == 256 ならば %d0 を 0(失敗)に設定し，(7) へ */
	beq	PUT_BUF_Finish
	movea.l	IN(%a1),%a2
	move.b	%d1,(%a2)					/* (4) m[in] = data */
	adda.l	#1,%a2						/* in++ ( (5) の else ) */
	lea.l	BOTTOM(%a1),%a3
	cmpa.l	%a3,%a2						/* (5) if (in == bottom) in=top */
	bcs	PUT_BUF_STEP1
	lea.l	TOP(%a1),%a2

PUT_BUF_STEP1:
	move.l	%a2,IN(%a1)
	add.w	#1,%d3						/* (6) s++,                  */
	move.w	%d3,S(%a1)
	move.l	#1,%d2						/*     %D0 を 1（成功）に設定 */

PUT_BUF_Finish:
	move.l	%d2,%d0
	rts

**-----------------------------------------------------------------------
** OUTQ：番号noのキューからデータを一つ取り出す
** 入力：	キュー番号 no -> %d0.l
** 戻り値：	失敗0/成功1 -> %d0.l
** 			取り出した8bitデータ data -> %d1.b
**----------------------------------------------------------------------
OUTQ:
	move.w	%SR,-(%sp)					/* (1) 現走行レベルの退避 */
	move.w	#0x2700,%SR					/* (2) 割り込み禁止(= 走行レベルを 7 に) */
	movem.l	%d2-%d3/%a1-%a3,-(%sp)		/* レジスタの退避 */
	lea.l	QUEUES,%a1					/* 指定された番号のキューのアドレスを計算 */
	mulu.w	#SIZE,%d0
	adda.l	%d0,%a1
	jsr	GET_BUF							/* (3) ～ (6) */
	movem.l	(%sp)+,%d2-%d3/%a1-%a3		/* レジスタの回復 */
	move.w	(%sp)+,%SR					/* (7) 旧走行レベルの回復 */
	rts

GET_BUF:
	move.l	#0,%d2
	move.w	S(%a1),%d3
	cmp.w	#0x00,%d3					/* (3)  s == 0 ならば %d0 を 0(失敗)に設定し，(7) へ */
	beq	GET_BUF_Finish
	movea.l	OUT(%a1),%a2
	move.b	(%a2),%d1					/* (4) data = m[out] */
	adda.l	#1,%a2						/* out++ ( (5) の else ) */
	lea.l	BOTTOM(%a1),%a3
	cmpa.l	%a3,%a2						/* (5) if (out == bottom) iout=top */
	bcs	GET_BUF_STEP1
	lea.l	TOP(%a1),%a2

GET_BUF_STEP1:
	move.l	%a2,OUT(%a1)
	sub.w	#1,%d3						/* (6) s--                   */
	move.w	%d3,S(%a1)
	move.l	#1,%d2						/*     %d0 を 1（成功）に設定 */

GET_BUF_Finish:	
	move.l	%d2,%d0
	rts


*****************************************
**入力
  **チャネル ch→%d1
**戻り値
  **なし 
*****************************************


INTERPUT:
	movem.l	 %a0-%a2/%d0,-(%sp)	/*レジスタ退避*/
	
	/*(1) 割り込み禁止（走行レベルを7に）*/
	ori.w  #0x0700,%SR
	
	/*(2) chが0でないならば，何もせずに復帰*/
	cmp.l #0,%d1
	bne  END_of_INTERPUT
	
	/*(3) OUTQ(1,data) を実行する*/
	move.l  #1,%d0
	jsr  OUTQ
	
	/*(4) OUTQの戻り値が0(失敗)ならば，送信割り込みをマスク(USTCNT1 を操作)して復帰*/
	cmp.l #0,%d0			
	beq MASK
	
	/*(5) dataを送信レジスタUTX1に代入して送信*/
	move.l  %d1,%d0		
	andi.l  #0x000000ff, %d0	
	add.l   #0x0800,%d0		/*ヘッダの付与*/
	move.w  %d0,UTX1	
	bra    END_of_INTERPUT
	

MASK:
	move.w #0xe108,USTCNT1
	/*move.l #0x00fffffb, IMR
	move.w #0x2000, %SR*/
	
	

END_of_INTERPUT:	
	movem.l	(%sp)+, %a0-%a2/%d0 /*レジスタ退復帰*/
	rts

*****************************************
**入力
  **チャネル ch→%d1
  **データ読み込み先の先頭アドレス p→%d2
  **送信するデータ数 size→%d3
**戻り値
  **実際に送信したデータ数 sz→ %d0 
*****************************************
	

PUTSTRING:
	movem.l	 %a0-%a3,-(%sp)	/*レジスタ退避*/
	
	/*(1) chが0でないならば，何もせずに復帰*/
	cmp.l    #0,%d1			
	bne      END_of_PUTSTRING	/*chが0でないならば何もせずに復帰*/
	
	/*(2) sz ← 0 , i ← p*/
	lea.l    sz,%a0
	lea.l    i,%a1
    move.l   #0,(%a0) /*szを初期化*/
	move.l   %d2,(%a1)		/*iに文字列ポインタを保存*/
	
	/*(3) size = 0 ならば(10)へ*/
	cmp.l    #0,%d3
 	beq      sz_SET			/*(10)へ*/
	
	/*(4) sz = size ならば (9) へ*/
loop_PUTSTRING:	
	cmp.l    (%a0),%d3
	beq      UNMASK			/*sizeが0ならばアンマスクへ*/
	
	/*(5) INQ(1,i)を実行し，送信キューへi番地のデータを書き込む*/
	move.l   #1,%d0		/*noにチャンネルをセット*/
	movea.l  (%a1),%a3
	move.b   (%a3),%d1		/*dataにp[i]をセット*/
	jsr 	 INQ

	/*(6) INQの復帰値が0(失敗/queue full)なら(9)へ*/
	cmp      #0,%d0
	beq      UNMASK

	/*(7) sz++, i++*/
	addq.l   #1,(%a0)			/*sz++*/
	addq.l   #1,(%a1)			/*i++*/

	/*(8) (4) へ*/
	bra      loop_PUTSTRING

/*(9) USTCNT1 を操作して送信割り込み許可 (アンマスク)*/
UNMASK:
	move.w #0xe10c,USTCNT1

/*(10) %D0 ←− sz*/
sz_SET:
	move.l   (%a0),%d0		/*szをセット*/

END_of_PUTSTRING:	
	movem.l	(%sp)+, %a0-%a3
	rts
	
*----------------------------------------------------------------------
* GETSTRING と INTERGET 
*----------------------------------------------------------------------
GETSTRING:
	movem.l	%d1-%d4/%a0,-(%sp)		/* レジスタの退避 */
	cmp.l	#0,%d1					/* (1) ch ≠ 0 なら何も実行せず復帰 */
	bne	GETSTRING_END

GETSTRING1:
	move.l	#0,%d4					/* (2) sz <- 0 */
	move.l	%d2,%a0					/*     i <- p */

GETSTRING2:
	cmp.l	%d3,%d4					/* (3) sz = size なら (9) へ */
	beq	GETSTRING3
	move.l	#0,%d0					/* (4) OUTQ(0,data)により受信キューから8bitデータ読み込み */
	jsr	OUTQ
	cmp.l	#0,%d0					/* (5) OUTQの復帰値(%d0の値)が 0(失敗) なら (9) へ */
	beq	GETSTRING3
	move.b	%d1,(%a0)				/* (6) i番地にdataをコピー */
	addq.l	#1,%d4					/* (7) sz++ */
	adda.l	#1,%a0					/*     i++ */
	bra	GETSTRING2					/* (8) (3) へ */

GETSTRING3:
	move.l	%d4,%d0					/* (9) %d0 <- sz */
	bra	GETSTRING_Finish
	
GETSTRING_END:
	move.l	#0,%d0

GETSTRING_Finish:
	movem.l	(%sp)+,%d1-%d4/%a0		/* レジスタの回復 */
	rts

INTERGET:
	movem.l	%d0-%d1,-(%sp)			/* レジスタの退避 */
	cmp.l	#0,%d1					/* (1) ch ≠ 0 ならば何も実行せず復帰 */
	bne	INTERGET_Finish
	move.l	#0,%d0					/* (2) INQ(0,data) */
	move.l	%d2,%d1
	jsr	INQ

INTERGET_Finish:
	movem.l	(%sp)+,%d0-%d1			/* レジスタの回復 */
	rts


*----------------------------------------------------------------------
* RESET_TIMER ルーチン (タイマ停止 & 割り込み禁止)
*
* 役割: タイマーを安全に停止し、割り込みを禁止する。
*----------------------------------------------------------------------
RESET_TIMER:
    move.l  #TCTL1, %A0        /* A0 に TCTL1 のアドレスをロード */
    andi.w  #0xFFFE, (%A0)     /* タイマー停止 (TCTL1 Bit 0 'TEN' = 0) */
    
    move.l  #IMR, %A0          /* A0 に IMR のアドレスをロード */
    * IMR Bit 1 (Timer1) を 1 (マスク/禁止) に設定する
    ori.l   #0x00000002, (%A0)
    
    move.w  #0x0000, TSTAT1   /* 保留中の割り込みフラグをクリア */
    rts

*----------------------------------------------------------------------
* SET_TIMER ルーチン (タイマ設定 & 割り込み許可)
*
* 役割: 指定時間後に指定ルーチンを呼び出すよう設定する。
* 引数:
* %D1.W : タイムアウト時間 t (0.1msec単位)
* %D2.L : コールバックルーチン p のアドレス
*----------------------------------------------------------------------
SET_TIMER:
    move.l  #TCTL1, %A0        /* A0 に TCTL1 のアドレスをロード */
    andi.w  #0xFFFE, (%A0)     /* タイマーを一旦停止 (安全のため) */
    
    lea.l   task_p, %A0        /* A0 に task_p 変数のアドレスをロード */
    move.l  %D2, (%A0)        /* task_p にコールバックのアドレス (%D2) を保存 */
    
    move.w  #206, TPRER1      /* プリスケーラを 0.1ms 周期に設定 */
    move.w  %D1, TCMP1        /* 割り込み発生時間を設定 */
    move.w  #0x0000, TSTAT1   /* 古いフラグをクリア */
    
    move.l  #IMR, %A0          /* A0 に IMR のアドレスをロード */
    * IMR Bit 1 (Timer1) を 0 (許可/アンマスク) に設定する
    andi.l  #0xFFFFFFFD, (%A0)
    
    * タイマー起動
    * (TCTL1 = 0x0015 -> IRQEN=1, FRR=1, TEN=1)
    move.w  #0x0015, TCTL1
    rts

*----------------------------------------------------------------------
* CALL_RP ルーチン (コールバック呼び出し)
*
* 役割: 割り込みハンドラから呼び出され、task_p のルーチンを実行する。
*----------------------------------------------------------------------
CALL_RP:
    lea.l   task_p, %A0        /* A0 に task_p 変数のアドレスをロード */
    move.l  (%A0), %A0     /* A0 に task_p の中身 (コールバックアドレス) をロード */
    jsr     (%A0)          /* コールバックルーチンを実行 */
    rts

*----------------------------------------------------------------------
* タイマ1 割り込みハンドラ (ベクタ番号 70)
*
* 役割: タイマー割り込み発生時にCPUから直接呼び出される。
* (bootルーチンで 0x118 番地にこのアドレスを登録する必要あり)
*----------------------------------------------------------------------
timer1_interrupt:
    * 全てのレジスタをスタックに退避
    movem.l %D0-%D7/%A0-%A6, -(%SP)

    * 割り込み要因を確認 (mon.s 方式)
    move.w  TSTAT1, %D0       /* TSTAT1 (16bit) を %D0 に読み込む */
    andi.w  #0x0001, %D0      /* Bit 0 (COMPフラグ) だけを取り出す */
    beq     .L_TIMER_EXIT   /* Bit 0 が 0 なら（タイマー要因でないなら）終了 */

    * タイマー割り込みの処理
    move.w  #0x0000, TSTAT1   /* フラグをクリア */
    jsr     CALL_RP           /* コールバック呼び出し */

.L_TIMER_EXIT:
    * 全てのレジスタをスタックから復帰
    movem.l (%SP)+, %D0-%D7/%A0-%A6
    rte                     /* 割り込みから復帰 */





*----------------------------------------------------------------------
* キュー定義 
*----------------------------------------------------------------------
.section .data

TDATA1: .ascii "0123456789ABCDEF"
TDATA2: .ascii "klmnopqrstuvwxyz"
.end








