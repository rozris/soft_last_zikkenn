#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

#define BOARD_SIZE 3

/* ベクトル（動的配列）の代わりに、固定長の構造体で管理する */
typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE];
    int width;
    int height;
} BOARD_DATA;

/* グローバル変数として静的に実体を確保（ヒープを使わない） */
BOARD_DATA game_board;
int game_over = 0;

/* セマフォなどは前述の通り数値で指定 */
#define SEM_BOARD 0
#define SEM_UART  1

void draw_board(int fd) {
    int i, j;
    char cell_buf[2]; // 1文字表示用の固定バッファ
    cell_buf[1] = '\0';

    P(SEM_UART);
    gotoxy(fd, 1, 1);
    my_write(fd, "--- Board (1-9 to move) ---\r\n");

    for (i = 0; i < game_board.height; i++) {
        for (j = 0; j < game_board.width; j++) {
            // 直接配列の要素を参照して、1文字ずつ書き出す
            cell_buf[0] = game_board.cells[i][j];
            my_write(fd, cell_buf);
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
            // 構造体の中の静的配列を書き換え
            if (game_board.cells[y][x] == '.') {
                game_board.cells[y][x] = mark;
            }
            V(SEM_BOARD);
        }
        draw_board(fd);
    }
}

int main(void) {
    int i, j;
    init_kernel();
    init_uart2();

    /* 構造体のメンバを初期化 */
    game_board.width = BOARD_SIZE;
    game_board.height = BOARD_SIZE;

    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            game_board.cells[i][j] = '.';
        }
    }

    // タスク登録・開始
    set_task(player1_task);
    set_task(player2_task);
    set_task(supervisor_task);

    begin_sch();
    return 0;
}
