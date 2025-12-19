#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

#define SEM_BOARD 0
#define SEM_UART  1
#define BOARD_SIZE 3

char board[BOARD_SIZE][BOARD_SIZE];
int game_over = 0;

void gotoxy(int fd, int x, int y) {
    char buf[20];
    sprintf(buf, "\033[%d;%dH", y, x);
    my_write(fd, buf);
}

void draw_board(int fd) {
    int i, j;
    P(SEM_UART);
    gotoxy(fd, 1, 1);
    my_write(fd, "--- Board (1-9 to move) ---\r\n");
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

void player_proc(int fd, char mark) {
    while (!game_over) {
        int c = inkey(fd);
        if (c >= '1' && c <= '9') {
            int pos = c - '1';
            int y = pos / 3;
            int x = pos % 3;
            P(SEM_BOARD);
            if (board[y][x] == '.') board[y][x] = mark;
            V(SEM_BOARD);
        }
        draw_board(fd);
    }
}

void player1_task() { player_proc(0, 'O'); }
void player2_task() { player_proc(1, 'X'); }

void supervisor_task() {
    int count = 0;
    while (!game_over) {
        count++;
        if (count > 20) { /* 簡易タイマー */
            P(SEM_BOARD);
            /* ここに爆弾ロジック（ランダム消去など）を記述 */
            V(SEM_BOARD);
            count = 0;
        }
    }
}

int main(void) {
    int i, j;
    init_kernel();
    init_uart2();
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE; j++) board[i][j] = '.';

    set_task(player1_task);
    set_task(player2_task);
    set_task(supervisor_task);

    begin_sch();
    return 0;
}
