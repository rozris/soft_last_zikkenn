#ifndef MTK_C_H
#define MTK_C_H

/* 定数定義 */
#define NULLTASKID  0       /* 無効なタスクID (キューの終端) */
#define NUMTASK     5       /* 最大タスク数 [cite: 766] */
#define STKSIZE     1024    /* スタックサイズ [cite: 784] */
#define NUMSEMAPHORE 5      /* セマフォの数 */

/* 型定義 */
typedef int TASK_ID_TYPE;

/* タスクコントロールブロック (TCB) [cite: 712] */
typedef struct {
    void (*task_addr)();    /* タスクの開始アドレス */
    void *stack_ptr;        /* 保存されたスタックポインタ(SSP) */
    int priority;           /* 優先度 (今回は未使用だが定義) */
    int status;             /* タスクの状態 */
    TASK_ID_TYPE next;      /* 次のタスクID (キュー用) */
} TCB_TYPE;

/* スタック構造体 [cite: 775] */
typedef struct {
    char ustack[STKSIZE];   /* ユーザスタック */
    char sstack[STKSIZE];   /* スーパーバイザスタック */
} STACK_TYPE;

/* セマフォ構造体 [cite: 707] */
typedef struct {
    int count;
    int nst;                /* reserved */
    TASK_ID_TYPE task_list; /* 待ち行列 */
} SEMAPHORE_TYPE;

/* グローバル変数の宣言 */
extern TCB_TYPE task_tab[NUMTASK + 1];
extern STACK_TYPE stacks[NUMTASK];
extern TASK_ID_TYPE ready;      /* 実行待ち行列 */
extern TASK_ID_TYPE curr_task;  /* 現在実行中のタスクID */
extern TASK_ID_TYPE new_task;   /* 新規作成タスクID */
extern TASK_ID_TYPE next_task;  /* 次に実行するタスクID */
extern SEMAPHORE_TYPE semaphore[NUMSEMAPHORE];

/* 関数のプロトタイプ宣言 */
void init_kernel(void);
void set_task(void (*func)());
void begin_sch(void);
void *init_stack(int id);

/* 外部関数（他担当またはアセンブリ） */
extern void P(int ch);
extern void V(int ch);
extern void addq(TASK_ID_TYPE *q, TASK_ID_TYPE id); /* キュー追加 */
extern TASK_ID_TYPE removeq(TASK_ID_TYPE *q);       /* キュー取り出し */
extern void pv_handler(void);   /* TRAP #1 ハンドラ (mtk_asm.s) */
extern void init_timer(void);   /* タイマ初期化 (mtk_asm.s) */
extern void first_task(void);   /* 最初のタスク起動 (mtk_asm.s) */

#endif
