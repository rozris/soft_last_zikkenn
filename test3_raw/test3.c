#include <stdarg.h>

/* =========================================================
   1. レジスタ定義・定数定義
   ========================================================= */
/* 基本アドレス */
#define REGBASE 0xFFF000
#define IMR     (*(volatile unsigned long *)(REGBASE + 0x304))

/* UART1 (fd=0) */
#define USTCNT1 (*(volatile unsigned short *)(REGBASE + 0x900))
#define UBAUD1  (*(volatile unsigned short *)(REGBASE + 0x902))
#define URX1    (*(volatile unsigned short *)(REGBASE + 0x904))
#define UTX1    (*(volatile unsigned short *)(REGBASE + 0x906))

/* UART2 (fd=1) [テキストp.91-92参照] */
#define USTCNT2 (*(volatile unsigned short *)(REGBASE + 0x910))
#define UBAUD2  (*(volatile unsigned short *)(REGBASE + 0x912))
#define URX2    (*(volatile unsigned short *)(REGBASE + 0x914))
#define UTX2    (*(volatile unsigned short *)(REGBASE + 0x916))

/* IMRのUART2マスクビット (Bit 12: 0x1000) [テキストC.2.1参照] */
#define IMR_UART2_MASK 0x00001000

/* ゲーム定数 */
#define WIDTH  40
#define HEIGHT 20

/* =========================================================
   2. システム基盤関数 (割り込み制御・I/O)
   ========================================================= */

/* 割り込みレベル操作 (インラインアセンブラ)
   戻り値: 変更前のSRの値 */
int set_ipl(int level) {
    int old_sr;
    /* 現在のSRを取得 */
    __asm__ volatile ("move.w %%sr, %0" : "=d" (old_sr));
    
    /* 割り込みレベルを設定 (上位バイトにレベル<<8 | 0x2000[S-bit]) */
    /* レベル7(全禁止)の場合は 0x2700 */
    if (level == 7) {
        __asm__ volatile ("move.w #0x2700, %%sr" : :);
    } else {
        /* 任意のレベル設定が必要な場合はここで調整 */
        /* 今回は排他制御用なので0x2700固定でも可 */
    }
    return old_sr;
}

/* SRを復元する */
void restore_ipl(int old_sr) {
    __asm__ volatile ("move.w %0, %%sr" : : "d" (old_sr));
}

/* UART2の初期化 [テキストp.41, C.2.3参照] */
void init_uart2(void) {
    /* 割り込み禁止状態で設定推奨 */
    int lock = set_ipl(7);

    /* UART2のリセットと設定 */
    USTCNT2 = 0x0000;   /* Reset */
    USTCNT2 = 0xE100;   /* RX/TX Enable, 8bit, No Parity, 1 Stop */
    UBAUD2  = 0x0126;   /* 38400 bps (33MHz clock) */

    /* IMRのUART2割り込みマスクを解除 (0で許可) */
    /* Bit 12をクリアする */
    IMR &= ~IMR_UART2_MASK;

    restore_ipl(lock);
}

/* 非ブロッキング入力関数
   fd: 0(UART1), 1(UART2)
   戻り値: 文字コード / -1(入力なし) */
int inkey(int fd) {
    volatile unsigned short *urx_reg;

    /* ポート選択 */
    if (fd == 0) urx_reg = &URX1;
    else         urx_reg = &URX2;

    /* URXレジスタのBit 13 (DATA READY) を確認 [テキストp.86参照] */
    unsigned short val = *urx_reg;
    if (val & 0x2000) { /* 0x2000 = Bit 13 */
        return (val & 0x00FF); /* 下位8ビットがデータ */
    }
    return -1;
}

/* 文字列出力関数 (割り込み禁止で保護)
   fd: 0(UART1), 1(UART2) */
void my_write(int fd, char *s) {
    volatile unsigned short *utx_reg;
    /* volatile unsigned short *ustcnt_reg; */ /* 必要ならステータス確認に使用 */

    if (fd == 0) utx_reg = &UTX1;
    else         utx_reg = &UTX2;

    /* 送信中は割り込みを禁止して、他タスクのprintf等による破壊を防ぐ */
    int lock = set_ipl(7);

    while (*s) {
        /* 送信可能になるまで待つ (簡易実装: FIFOが空くのを待つなど) */
        /* UTXのBit 15 (FIFO EMPTY) 等を確認しても良いが、
           実機ではウェイトなしでもFIFOがある程度吸収する。
           安全のため簡易ウェイトを入れるか、FIFOステータスをチェック推奨。
           ここでは直接書き込みを行う (多くの実験コードの実装準拠) */
           
        /* Bit 15 (FIFO EMPTY) が1になるまで待つループを入れるとより安全 */
        /* while (!(*utx_reg & 0x8000)); */

        *utx_reg = (unsigned short)(*s);
        s++;
        
        /* 文字間ウェイトが必要な場合あり */
        /* for(volatile int i=0; i<100; i++); */ 
    }

    restore_ipl(lock);
}

/* =========================================================
   3. 描画・ユーティリティ関数
   ========================================================= */

void gotoxy(int fd, int x, int y) {
    char buf[30];
    /* sprintfが使える場合。使えない場合は自作の数値変換が必要 */
    sprintf(buf, "\033[%d;%dH", y, x);
    my_write(fd, buf);
}

void clrscr(int fd) {
    my_write(fd, "\033[2J"); /* 画面クリア */
    my_write(fd, "\033[H");  /* カーソルホーム */
}

/* =========================================================
   4. ゲームロジック関数 (ヒープ不使用)
   ========================================================= */

/* ゲームの状態を管理する構造体 */
typedef struct {
    int player_x;
    int player_y;
    int enemy_x;
    int enemy_y;
    int score;
    int active; /* 1: Active, 0: Waiting */
} GameState;

/* 静的領域に確保 (malloc禁止のため) */
GameState states[2]; /* 0: Task A用, 1: Task B用 */

void init_game(void) {
    int i;
    for (i = 0; i < 2; i++) {
        states[i].player_x = 10;
        states[i].player_y = 10;
        states[i].enemy_x  = 30;
        states[i].enemy_y  = 5;
        states[i].score    = 0;
        states[i].active   = 0;
    }
    /* 初期化時はTask A(UART1)をアクティブとする等の設定 */
    states[0].active = 1;
}

/* ゲームロジック更新
   id: タスクID (0 or 1) */
void update_logic(int id) {
    GameState *s = &states[id];
    
    /* 入力処理 (自機の移動) */
    int c = inkey(id); /* fd = id と仮定 */
    if (c != -1) {
        if (c == 'w') s->player_y--;
        if (c == 's') s->player_y++;
        if (c == 'a') s->player_x--;
        if (c == 'd') s->player_x++;
    }

    /* 境界チェック */
    if (s->player_x < 1) s->player_x = 1;
    if (s->player_x > WIDTH) s->player_x = WIDTH;
    if (s->player_y < 1) s->player_y = 1;
    if (s->player_y > HEIGHT) s->player_y = HEIGHT;

    /* 敵の簡易AI (自機に近づく) */
    /* 処理負荷軽減のため、毎回ではなく適当なタイミングで動かすなどの調整も可 */
    if (s->player_x > s->enemy_x) s->enemy_x++;
    else if (s->player_x < s->enemy_x) s->enemy_x--;
    
    if (s->player_y > s->enemy_y) s->enemy_y++;
    else if (s->player_y < s->enemy_y) s->enemy_y--;

    /* 当たり判定 */
    if (s->player_x == s->enemy_x && s->player_y == s->enemy_y) {
        s->score -= 10; /* 減点 */
        /* 敵をリセット */
        s->enemy_x = (s->player_x + 10) % WIDTH; 
        if(s->enemy_x == 0) s->enemy_x = 2;
    } else {
        s->score++; /* 生存スコア */
    }
}

/* 画面描画
   fd: 出力先UART, id: 参照するGameState ID */
void draw_screen(int fd, int id) {
    GameState *s = &states[id];
    
    /* 画面クリアは通信量が多いので、毎回は行わず、
       動きがあった部分だけ書き換えるか、周期的に行うのがベター。
       ここではシンプルに毎回クリアする（通信速度38400bpsなら多少ちらつく） */
    clrscr(fd);

    /* ステータス表示 */
    if (s->active) {
        my_write(fd, "[ACTIVE] Turn\r\n");
    } else {
        my_write(fd, "[WAITING] ...\r\n");
    }

    /* プレイヤー描画 */
    gotoxy(fd, s->player_x, s->player_y);
    my_write(fd, "P");

    /* 敵描画 */
    gotoxy(fd, s->enemy_x, s->enemy_y);
    my_write(fd, "E");

    /* スコア表示 */
    gotoxy(fd, 1, HEIGHT + 1);
    char buf[20];
    sprintf(buf, "Score: %d", s->score);
    my_write(fd, buf);
}

/* =========================================================
   5. タスク実装例 (Task A, Task B)
   ========================================================= */

/* Task A: UART1 (fd=0) 担当 */
void task_a_func(void) {
    /* ゲーム初期化はmain等で一度だけ呼ぶこと */
    
    while (1) {
        /* UART1に対するロジックと描画 */
        update_logic(0);
        draw_screen(0, 0); /* UART1にState0を表示 */
        
        /* ビジーループによる簡易ウェイト (実機速度に合わせて調整) */
        /* OSのsleep()システムコールがあればそれを使うべき */
        volatile long w;
        for (w = 0; w < 50000; w++); 
    }
}

/* Task B: UART2 (fd=1) 担当 */
void task_b_func(void) {
    while (1) {
        /* UART2に対するロジックと描画 */
        update_logic(1);
        draw_screen(1, 1); /* UART2にState1を表示 */
        
        volatile long w;
        for (w = 0; w < 50000; w++);
    }
}
