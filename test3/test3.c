#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

/* --- 外部関数・変数の宣言 --- */
/* OS側(mtk_asm.o / mtk_c.o)で定義されている関数とセマフォ */
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
    /* 画面をクリアしてホームポジションへ移動 */
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

/* --- タスク1: プレイヤー (UART 0) --- */
void player_task() {
    while (!game_over) {
        int c = inkey(0);
        
        /* 入力がない、または不整合(0)の時は sw_task で他タスクにCPUを譲る */
        if (c <= 0) {
            //sw_task(); 
            continue;
        }

        /* 1-9 の入力に対応するマスを 'O' にする */
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

/* --- タスク2: CPU (自動着手) --- */
void cpu_task() {
    while (!game_over) {
        /* CPUの待機（ビジーループ中に sw_task を挟んで他タスクを動かす） */
        for (volatile int d = 0; d < 1000000; d++) {
            if (d % 1000 == 0) //sw_task(); 
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

/* --- タスク3: ボンバー (消去) --- */
void bomber_task() {
    int target = 0;
    while (!game_over) {
        /* CPUより少し遅い周期で巡回 */
        for (volatile int d = 0; d < 1500000; d++) {
            if (d % 1000 == 0) //sw_task();
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

    /* カーネルとハードウェアの初期化 */
    init_kernel();

    /* 盤面の初期状態を設定 */
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            game_board.cells[i][j] = '.';
        }
    }

    /* 各タスクをスケジューラに登録 */
    set_task(player_task);
    set_task(cpu_task);
    set_task(bomber_task);

    /* 初期状態を描画してからマルチタスク開始 */
    draw_board(0);
    begin_sch();

    return 0;
}
