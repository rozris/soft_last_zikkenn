#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

#define BOARD_SIZE 3
#define SEM_BOARD 0
#define SEM_UART  1

typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE];
} BOARD_DATA;

/* --- 静的グローバル変数 --- */
BOARD_DATA game_board;
int game_over = 0;

/* --- 描画関数 (画面崩れ対策: 行バッファ送信) --- */
void draw_board(int fd) {
    int i;
    char row_buf[32];

    P(SEM_UART);
    // 画面消去とカーソルホーム (ANSIエスケープ)
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

/* --- タスク1: プレイヤー (UART 0からの入力) --- */
void player_task() {
    while (!game_over) {
        int c = inkey(0);
        if (c >= '1' && c <= '9') {
            int pos = c - '1';
            P(SEM_BOARD);
            if (game_board.cells[pos / 3][pos % 3] == '.') {
                game_board.cells[pos / 3][pos % 3] = 'O';
            }
            V(SEM_BOARD);
            draw_board(0);
        }
    }
}

/* --- タスク2: CPU (敵の自動着手) --- */
void cpu_task() {
    int pos = 0;
    while (!game_over) {
        // 簡易的な時間待ち (環境に合わせて調整)
        for (volatile int d = 0; d < 1000000; d++); 

        P(SEM_BOARD);
        // 空いている場所を探して 'X' を置く
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

/* --- タスク3: ボンバー (ランダム消去タスク) --- */
void bomber_task() {
    int target = 0;
    while (!game_over) {
        // CPUより少し遅い周期で巡回
        for (volatile int d = 0; d < 1500000; d++); 

        P(SEM_BOARD);
        // 埋まっているマスを強制的にクリアする
        if (game_board.cells[target / 3][target % 3] != '.') {
            game_board.cells[target / 3][target % 3] = '.';
        }
        V(SEM_BOARD);
        
        target = (target + 1) % 9;
        draw_board(0);
    }
}

/* --- メイン関数 --- */
int main(void) {
    int i, j;

    // カーネル初期化 (ここにUART初期化も含まれる)
    init_kernel();

    // セマフォの手動初期化
    semaphore[SEM_BOARD].count = 1;
    semaphore[SEM_UART].count = 1;

    // 盤面の初期化
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            game_board.cells[i][j] = '.';
        }
    }

    // 3並列タスクの設定
    set_task(player_task);
    set_task(cpu_task);
    set_task(bomber_task);

    // マルチタスク開始
    begin_sch();

    return 0;
}
