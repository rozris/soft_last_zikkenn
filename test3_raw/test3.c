#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

/* --- 定数・レジスタ定義 --- */
#define REGBASE 0xFFF000
#define IMR     (*(volatile unsigned long *)(REGBASE + 0x304))
#define USTCNT2 (*(volatile unsigned short *)(REGBASE + 0x910))
#define UBAUD2  (*(volatile unsigned short *)(REGBASE + 0x912))
#define URX1    (*(volatile unsigned short *)(REGBASE + 0x904))
#define UTX1    (*(volatile unsigned short *)(REGBASE + 0x906))
#define URX2    (*(volatile unsigned short *)(REGBASE + 0x914))
#define UTX2    (*(volatile unsigned short *)(REGBASE + 0x916))

/* セマフォID定義 */
#define SEM_BOARD 0  /* 盤面データの保護 */
#define SEM_UART  1  /* UART出力(my_write/gotoxy)の排他制御 */

/* ゲーム設定 */
#define BOARD_SIZE 3
char board[BOARD_SIZE][BOARD_SIZE];
int game_over = 0;

/* --- システム基盤関数 --- */

/* 割り込みレベル設定（IPL7でタスク切り替えを一時禁止する） */
int set_ipl(int level) {
    int old_sr;
    __asm__ volatile ("move.w %%sr, %0" : "=d" (old_sr));
    if (level == 7) __asm__ volatile ("move.w #0x2700, %%sr" : :);
    return old_sr;
}

void restore_ipl(int old_sr) {
    __asm__ volatile ("move.w %0, %%sr" : : "d" (old_sr));
}

/* UART2初期化 */
void init_uart2(void) {
    int lock = set_ipl(7);
    USTCNT2 = 0x0000;   /* Reset */
    USTCNT2 = 0xE100;   /* RX/TX Enable */
    UBAUD2  = 0x0126;   /* 38400 bps */
    IMR &= ~0x00001000; /* UART2割り込み許可 */
    restore_ipl(lock);
}

/* 非ブロッキング入力 */
int inkey(int fd) {
    volatile unsigned short *urx = (fd == 0) ? &URX1 : &URX2;
    unsigned short val = *urx;
    if (val & 0x2000) return (val & 0x00FF); /* Data Ready確認 */
    return -1;
}

/* 割り込み安全な文字列出力 */
void my_write(int fd, char *s) {
    volatile unsigned short *utx = (fd == 0) ? &UTX1 : &UTX2;
    int lock = set_ipl(7); /* 再入不可対策 */
    while (*s) {
        *utx = (unsigned short)(*s++);
    }
    restore_ipl(lock);
}

void gotoxy(int fd, int x, int y) {
    char buf[20];
    sprintf(buf, "\033[%d;%dH", y, x);
    my_write(fd, buf);
}

/* --- ゲームロジック関数 --- */

void init_app(void) {
    int i, j;
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE; j++) board[i][j] = '.';
    
    /* セマフォ初期値の設定は init_kernel 内で行われている前提 */
}

/* 盤面描画（差分更新） */
void draw_board(int fd) {
    int i, j;
    P(SEM_UART);
    gotoxy(fd, 1, 1);
    my_write(fd, "--- Board ---\r\n");
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            char b[2] = {board[i][j], '\0'};
            my_write(fd, b);
            my_write(fd, " ");
        }
        my_write(fd, "\r\n");
    }
    V(SEM_UART);
}

/* --- タスク実装 --- */

/* 共通プレイヤー処理 */
void player_proc(int fd, char mark) {
    while (!game_over) {
        int c = inkey(fd);
        if (c >= '1' && c <= '9') {
            int pos = c - '1';
            int y = pos / 3;
            int x = pos % 3;

            P(SEM_BOARD);
            if (board[y][x] == '.') {
                board[y][x] = mark;
            }
            V(SEM_BOARD);
        }
        draw_board(fd);
        /* 1秒のタイムスライスを待たず、相手に権限を譲渡する場合は
           ここでOS固有の sleep や yield を呼ぶのが望ましい */
    }
}

void player1_task(void) {
    player_proc(0, 'O');
}

void player2_task(void) {
    player_proc(1, 'X');
}

/* 審判・爆弾タスク（第3のタスク） */
void supervisor_task(void) {
    int counter = 0;
    while (!game_over) {
        counter++;
        if (counter > 10) { /* 約10秒ごとにランダム爆発（簡易） */
            P(SEM_BOARD);
            board[1][1] = 'B'; /* 中央を爆破 */
            V(SEM_BOARD);
            counter = 0;
        }
        /* 勝利判定ロジック等をここに記述 */
    }
}

int main(void) {
    init_kernel(); /* */
    init_uart2();
    init_app();

    set_task(player1_task);
    set_task(player2_task);
    set_task(supervisor_task);

    begin_sch(); /* */
    return 0;
}
