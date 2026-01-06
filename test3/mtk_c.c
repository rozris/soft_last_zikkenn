#include "mtk_c.h"
#include <stdio.h>
#include <stdlib.h>


void sleep(int ch);
void wakeup(int ch);
void sched();
void swtch();

/* グローバル変数の定義 */
TCB_TYPE task_tab[NUMTASK + 1];
STACK_TYPE stacks[NUMTASK];
TASK_ID_TYPE ready;
TASK_ID_TYPE curr_task;
TASK_ID_TYPE new_task;
TASK_ID_TYPE next_task;
SEMAPHORE_TYPE semaphore[NUMSEMAPHORE];

/* p_body(ch): Pシステムコールの処理本体 */
void p_body(int ch)
{
	*(char*)0x00d00039='A';
    /* セマフォカウンタのデクリメント */
    semaphore[ch].count--;

    /* セマフォが獲得できない場合(count < 0)、タスクを休眠状態へ移行 */
    if (semaphore[ch].count < 0){
        sleep(ch);
    }
	*(char*)0x00d00039='a';
	return;
}

/* v_body(ch): Vシステムコールの処理本体 */
void v_body(int ch)
{
	*(char*)0x00d00039='B';
    /* セマフォカウンタのインクリメント */
    semaphore[ch].count++;

    /* 待機中のタスクが存在する場合(count <= 0)、タスクを起床させる */
    if (semaphore[ch].count <= 0) {
        wakeup(ch);
    }
	*(char*)0x00d00039='b';
	return;
}


/* sleep(ch): 自タスクを待機状態に遷移させタスクスイッチを実行 */
void sleep(int ch){
	*(char*)0x00d00039='C';
    /* 対象セマフォの待ち行列に現在のタスクを追加 */
    addq(&semaphore[ch].task_list, curr_task);
    
    /* 次に実行すべきタスクの選定 */
    sched();
    
    /* コンテキストスイッチの実行 */
    swtch();
	*(char*)0x00d00039='c';
	return;
}

/* wakeup(ch): 待機状態のタスクを実行可能状態へ戻す */
void wakeup(int ch){
	*(char*)0x00d00039='D';
    TASK_ID_TYPE waiting_task;
    
    /* 待ち行列が空でないことを確認 */
    if(semaphore[ch].task_list != NULLTASKID){ // NULLTASKID = 0
        /* 待ち行列の先頭タスクを取り出す */
        waiting_task = removeq(&semaphore[ch].task_list);
        
        /* 取り出したタスクを実行可能キューに追加 */
        addq(&ready, waiting_task);
    }
	*(char*)0x00d00039='d';
	return;
}


void init_kernel(){
	*(char*)0x00d00039='E';
    int i;
    
    /* TCBテーブルの初期化 */
    for(i = 0; i <= NUMTASK; i++){
        task_tab[i].next = NULLTASKID;
        task_tab[i].status = 0; // 0: FREE
        task_tab[i].priority = 0;
        task_tab[i].stack_ptr = NULL;
    }

    /* 実行可能キューの初期化 */
    ready = NULLTASKID;
    
    /* トラップベクタの設定 */
    *(void **)0x00000084 = pv_handler;

    /* セマフォの初期化 */
    for(i = 0; i < NUMSEMAPHORE; i++) {
        semaphore[i].count = 1;      /* バイナリセマフォとして初期化 */
        semaphore[i].task_list = NULLTASKID;
        semaphore[i].nst = 0;
    }
	*(char*)0x00d00039='e';
	return;
}


void set_task(void (*func)()) {
	*(char*)0x00d00039='F';
    TASK_ID_TYPE new_task = NULLTASKID;
    int i;

    /* 空きTCBスロットの検索 */
    for(i = 1; i <= NUMTASK; i++) {
        if (task_tab[i].status == 0) {
            new_task = i;
            break;
        }
    }
    
    /* 空きスロットがない場合は終了 */
    if (new_task == NULLTASKID) {
        return;
    }

    /* TCBの設定 */
    task_tab[new_task].task_addr = func;
    task_tab[new_task].status = 1; /* 1: READY/RUNNING */
    task_tab[new_task].priority = 0; 
    
    /* スタック領域の初期化 */
    task_tab[new_task].stack_ptr = init_stack(new_task);

    /* 実行可能キューへ登録 */
    addq(&ready, new_task);
	*(char*)0x00d00039='f';
	return;
}

void *init_stack(int id) {
    *(char*)0x00d00039='G';
    
    /* スタックポインタの初期値を設定 (スタック底のアドレス) */
    char *sp = (char *)&stacks[id - 1].sstack[STKSIZE]; 

    /* 初期PCの格納 */
    sp -= 4;
    *(int *)sp = (int)task_tab[id].task_addr;

    /* 初期SRの格納 (ユーザモード、全割り込み許可) */
    sp -= 2;
    *(short *)sp = 0x2000;
    
    /* 汎用レジスタ保存領域の確保 (D0-D7, A0-A6) */
    sp -= 60;
    
    /* 初期USPの格納 */
    sp -= 4;
    *(int *)sp = (int)&stacks[id - 1].ustack[STKSIZE];

    /* 設定後のスタックポインタを返す */
    *(char*)0x00d00039='g'; 
    return (void *)sp;
}


void begin_sch() {
	*(char*)0x00d00039='H';
    /* 実行開始タスクの取り出し */
    curr_task = removeq(&ready);

    /* ハードウェアタイマの起動 */
    init_timer();

    /* 最初のタスクの実行開始 */
    first_task();
	*(char*)0x00d00039='h';
	return;
}




/* sched(): 次に実行するタスクを選択する */
void sched() {
	*(char*)0x00d00039='I';
	TASK_ID_TYPE tid;
	
	tid = removeq(&ready);
	
	/* 実行可能タスクが存在しない場合は待機 */
	if (tid == NULLTASKID) {
	while (1);
	}
	
	next_task = tid;
	*(char*)0x00d00039='i';
	return;
}

/* addq(): 指定されたキューの末尾にタスクを追加する */
void addq(TASK_ID_TYPE *head, TASK_ID_TYPE tid) {
	*(char*)0x00d00039='J';
	TASK_ID_TYPE p = *head;
	
	if (p == NULLTASKID) {
		*head = tid;
		task_tab[tid].next = NULLTASKID;
		return;
	}
	
	while (task_tab[p].next != NULLTASKID) {
		p = task_tab[p].next;
	}
	
	task_tab[p].next = tid;
	task_tab[tid].next = NULLTASKID;
	*(char*)0x00d00039='j';
	return;
}

/* removeq(): 指定されたキューの先頭からタスクを取り出す */
TASK_ID_TYPE removeq(TASK_ID_TYPE *head) {
	*(char*)0x00d00039='K';

	TASK_ID_TYPE t = *head;
	
	if (t != NULLTASKID)*head = task_tab[t].next;
	*(char*)0x00d00039='k';	
	return t;
}

/* UART2初期化処理 */
void init_uart2(void) {
    int lock = set_ipl(7);
    USTCNT2 = 0x0000;   /* 通信制御レジスタリセット */
    USTCNT2 = 0xE100;   /* 送受信有効化 */
    UBAUD2  = 0x0126;   /* ボーレート設定: 38400 bps */
    IMR &= ~0x00001000; /* UART2割り込みマスク解除 */
    restore_ipl(lock);
}
