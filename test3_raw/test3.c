#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

/*セマフォID定義*/
#define SEM_BOARD 0  /* 盤面データの保護 */
#define SEM_UART  1  /* UART出力(my_write/gotoxy)の排他制御 */

/* ゲーム設定 */
#define BOARD_SIZE 3
char board[BOARD_SIZE][BOARD_SIZE];
int game_over=0;

/*システム基盤関数*/

/*割り込みレベル設定（タスク切り替え防止のため走行レベルを7に）*/



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
        if (counter>10) { /* 約10秒ごとにランダム爆発（簡易） */
            P(SEM_BOARD);
            board[1][1] = 'B'; /* 中央を爆破 */
            V(SEM_BOARD);
            counter=0;
        }
    }
}

int main(void) {
    init_kernel();
    init_uart2();
    init_app();

    set_task(player1_task);
    set_task(player2_task);
    set_task(supervisor_task);

    begin_sch();
    return 0;
}
