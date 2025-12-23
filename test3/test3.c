#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

/* --- 外部関数宣言 --- */
extern void sw_task(void);
extern int inkey(int fd);

#define BOARD_SIZE 3
#define SEM_BOARD 0
#define SEM_UART  1

/* --- 盤面データ構造体 (静的確保) --- */
typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE];
} BOARD_DATA;

/* --- 静的グローバル変数 --- */
BOARD_DATA game_board;
int game_over = 0;

/* --- 描画関数 --- */
void draw_board(int fd) {
    int i;
    char row_buf[32];

    P(SEM_UART);
    my_write(fd, "\033[2J\033[H"); 
    my_write(fd, "=== Survival Tic-Tac-Toe ===\r\n");
    my_write(fd, "YOU: O | CPU: X | BOMB: Clear\r\n\r\n");

    for (i = 0; i < BOARD_SIZE; i++) {
        sprintf(row_buf, "  %c | %c | %c \r\n", 
                game_board.cells[i][0], 
                game_board.cells[i][1], 
                game_board.cells[i][2]);
        my_write(fd, row_buf);
        if (i < 2) my_write(fd, " ---+---+---\r\n");
    }
    my_write(fd, "\r\nInput (1-9): ");
    V(SEM_UART);
}

/* --- タスク1: プレイヤー --- */
void player_task() {
    while (!game_over) {
        int c = inkey(0);
        
        // 入力がない、あるいはアセンブラ側の不整合(0)の時はタスクを切り替える
        if (c <= 0) {
            sw_task(); 
            continue;
        }

        if (c >= '1' && c <= '9') {
            int pos = c - '1';
            int y = pos / 3;
            int x = pos % 3;

            P(SEM_BOARD);
            if (game_board.cells[y][x] == '.') {
                game_board.cells[y][x] = 'O';
                V(SEM_BOARD);
                draw_board(0);
            } else {
                V(SEM_BOARD);
            }
        }
    }
}

/* --- タスク2: CPU --- */
void cpu_task() {
    while (!game_over) {
        // CPUの待機時間（sw_taskを挟んで占有を回避）
        for (volatile int d = 0; d < 1000000; d++) {
            if (d % 1000 == 0) sw_task(); 
        }

        P(SEM_BOARD);
        int placed = 0;
        for (int i = 0; i < 9; i++) {
            if (game_board.cells[i / 3][i % 3] == '.') {
                game_board.cells[i / 3][i % 3] = 'X';
                placed = 1;
                break;
            }
        }
        V(SEM_BOARD);
        if (placed) draw_board(0);
    }
}

/* --- タスク3: ボンバー --- */
void bomber_task() {
    int target = 0;
    while (!game_over) {
        // ボンバーの待機時間
        for (volatile int d = 0; d < 1500000; d++) {
            if (d % 1000 == 0) sw_task();
        }

        P(SEM_BOARD);
        if (game_board.cells[target / 3][target % 3] != '.') {
            game_board.cells[target / 3][target % 3] = '.';
            V(SEM_BOARD);
            draw_board(0);
        } else {
            V(SEM_BOARD);
        }
        
        target = (target + 1) % 9;
    }
}

/* --- メイン関数 --- */
int main(void) {
    int i, j;
    init_kernel();
    /* セマフォの初期化は init_kernel() 等で実行済みと想定 */

    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            game_board.cells[i][j] = '.';
        }
    }

    set_task(player_task);
    set_task(cpu_task);
    set_task(bomber_task);

    begin_sch();
    return 0;
}
