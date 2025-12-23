#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

extern void sw_task(void);
extern int inkey(int fd);

#define BOARD_SIZE 5
#define SEM_BOARD 0
#define SEM_UART  1

typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE];
} BOARD_DATA;

BOARD_DATA game_board;
int game_over = 0;

/* --- 勝敗判定 (5つ並んだか) --- */
char check_winner() {
    int i, j;
    // 横・縦の判定
    for (i = 0; i < BOARD_SIZE; i++) {
        int row_count = 0, col_count = 0;
        for (j = 0; j < BOARD_SIZE; j++) {
            if (game_board.cells[i][j] == 'O') row_count++; else if (game_board.cells[i][j] == 'X') row_count--;
            if (game_board.cells[j][i] == 'O') col_count++; else if (game_board.cells[j][i] == 'X') col_count--;
        }
        if (row_count == 5 || col_count == 5) return 'O';
        if (row_count == -5 || col_count == -5) return 'X';
    }
    // 斜めの判定 (簡易的に主対角のみ)
    int d1 = 0, d2 = 0;
    for (i = 0; i < BOARD_SIZE; i++) {
        if (game_board.cells[i][i] == 'O') d1++; else if (game_board.cells[i][i] == 'X') d1--;
        if (game_board.cells[i][BOARD_SIZE-1-i] == 'O') d2++; else if (game_board.cells[i][BOARD_SIZE-1-i] == 'X') d2--;
    }
    if (d1 == 5 || d2 == 5) return 'O';
    if (d1 == -5 || d2 == -5) return 'X';

    // 引き分けチェック
    for (i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) 
        if (game_board.cells[i/BOARD_SIZE][i%BOARD_SIZE] == '.') return 0;
    return 'D';
}

void draw_board(int fd, char* msg) {
    int i, j;
    char row_buf[64];
    P(SEM_UART);
    my_write(fd, "\033[2J\033[H"); 
    my_write(fd, "=== Survival Tic-Tac-Toe (5x5) ===\r\n");
    my_write(fd, "YOU: O | CPU: X | BOMB: Clear\r\n\r\n    1 2 3 4 5\r\n");
    for (i = 0; i < BOARD_SIZE; i++) {
        sprintf(row_buf, "%d  %c|%c|%c|%c|%c\r\n", i+1,
                game_board.cells[i][0], game_board.cells[i][1], game_board.cells[i][2], 
                game_board.cells[i][3], game_board.cells[i][4]);
        my_write(fd, row_buf);
        if (i < 4) my_write(fd, "   -+-+-+-+-\r\n");
    }
    my_write(fd, "\r\n");
    my_write(fd, msg);
    V(SEM_UART);
}

/* --- プレイヤー：2段階入力タスク --- */
void player_task() {
    int y = -1, x = -1;
    char msg[64] = "Input Row (1-5): ";
    
    while (!game_over) {
        int c = inkey(0);
        if (c <= 0 || c == 10 || c == 13) { ; continue; } // 入力なし or Enterは飛ばす

        if (c >= '1' && c <= '5') {
            if (y == -1) {
                y = c - '1';
                sprintf(msg, "Row %d selected. Input Col (1-5): ", y + 1);
            } else {
                x = c - '1';
                P(SEM_BOARD);
                if (game_board.cells[y][x] == '.') {
                    game_board.cells[y][x] = 'O';
                    sprintf(msg, "Set ( %d, %d ). Next Row: ", y+1, x+1);
                    y = -1; x = -1; // リセット
                } else {
                    sprintf(msg, "Occupied! Input Row (1-5): ");
                    y = -1; x = -1;
                }
                V(SEM_BOARD);
            }
            draw_board(0, msg);
        }
    }
}

/* --- CPU：高速化版 --- */
void cpu_task() {
    while (!game_over) {
        // 待機時間を短縮（プレイヤーが入力に苦戦する間に埋める）
        for (volatile int d = 0; d < 500000; d++) if (d % 1000 == 0); 

        P(SEM_BOARD);
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
            if (game_board.cells[i/BOARD_SIZE][i%BOARD_SIZE] == '.') {
                game_board.cells[i/BOARD_SIZE][i%BOARD_SIZE] = 'X';
                break;
            }
        }
        V(SEM_BOARD);
        draw_board(0, "CPU moved.");
    }
}

/* --- ボンバー --- */
void bomber_task() {
    int target = 0;
    while (!game_over) {
        for (volatile int d = 0; d < 1200000; d++) if (d % 1000 == 0); 
        P(SEM_BOARD);
        if (game_board.cells[target/BOARD_SIZE][target%BOARD_SIZE] != '.') {
            game_board.cells[target/BOARD_SIZE][target%BOARD_SIZE] = '.';
        }
        V(SEM_BOARD);
        target = (target + 1) % (BOARD_SIZE * BOARD_SIZE);
        draw_board(0, "Bomb ticking...");
    }
}

int main(void) {
    int i, j;
    init_kernel();
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE; j++) game_board.cells[i][j] = '.';

    set_task(player_task);
    set_task(cpu_task);
    set_task(bomber_task);

    draw_board(0, "Game Start! Input Row (1-5): ");
    begin_sch();
    return 0;
}
