#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

/* --- 外部関数の宣言 --- */
extern int inkey(int fd);

#define BOARD_SIZE 5
#define SEM_BOARD 0
#define SEM_UART  1

typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE];
} BOARD_DATA;

BOARD_DATA game_board;
int game_over = 0;

/* --- 勝敗判定 (5つ並んだかチェック) --- */
char check_winner() {
    int i, j;
    // 横の判定
    for (i = 0; i < BOARD_SIZE; i++) {
        if (game_board.cells[i][0] != '.' &&
            game_board.cells[i][0] == game_board.cells[i][1] &&
            game_board.cells[i][1] == game_board.cells[i][2] &&
            game_board.cells[i][2] == game_board.cells[i][3] &&
            game_board.cells[i][3] == game_board.cells[i][4]) return game_board.cells[i][0];
    }
    // 縦の判定
    for (j = 0; j < BOARD_SIZE; j++) {
        if (game_board.cells[0][j] != '.' &&
            game_board.cells[0][j] == game_board.cells[1][j] &&
            game_board.cells[1][j] == game_board.cells[2][j] &&
            game_board.cells[2][j] == game_board.cells[3][j] &&
            game_board.cells[3][j] == game_board.cells[4][j]) return game_board.cells[0][j];
    }
    // 斜めの判定 (主対角・副対角)
    if (game_board.cells[2][2] != '.') {
        if (game_board.cells[0][0] == game_board.cells[1][1] && game_board.cells[1][1] == game_board.cells[2][2] &&
            game_board.cells[2][2] == game_board.cells[3][3] && game_board.cells[3][3] == game_board.cells[4][4]) return game_board.cells[2][2];
        if (game_board.cells[0][4] == game_board.cells[1][3] && game_board.cells[1][3] == game_board.cells[2][2] &&
            game_board.cells[2][2] == game_board.cells[3][1] && game_board.cells[3][1] == game_board.cells[4][0]) return game_board.cells[2][2];
    }

    // 引き分けチェック (空きマスの有無)
    for (i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) 
        if (game_board.cells[i/BOARD_SIZE][i%BOARD_SIZE] == '.') return 0;
    
    return 'D';
}

/* --- 描画関数 --- */
void draw_board(int fd, char* msg) {
    int i;
    char row_buf[128];
    char winner;

    P(SEM_UART);
    my_write(fd, "\033[2J\033[H"); 
    my_write(fd, "=== Survival Tic-Tac-Toe (5x5) ===\r\n");
    my_write(fd, "YOU: O | CPU: X | BOMB: Clear\r\n\r\n");
    my_write(fd, "      1   2   3   4   5\r\n");
    my_write(fd, "    +---+---+---+---+---+\r\n");

    for (i = 0; i < BOARD_SIZE; i++) {
        sprintf(row_buf, "  %d | %c | %c | %c | %c | %c |\r\n", i + 1,
                game_board.cells[i][0], game_board.cells[i][1], game_board.cells[i][2], 
                game_board.cells[i][3], game_board.cells[i][4]);
        my_write(fd, row_buf);
        my_write(fd, "    +---+---+---+---+---+\r\n");
    }

    winner = check_winner();
    if (winner != 0) {
        game_over = 1;
        if (winner == 'O') my_write(fd, "\r\n [!] ***** YOU WIN! *****\r\n");
        else if (winner == 'X') my_write(fd, "\r\n [!] ***** CPU WIN! *****\r\n");
        else my_write(fd, "\r\n [!] ***** DRAW GAME *****\r\n");
    } else {
        my_write(fd, "\r\n ");
        my_write(fd, msg);
    }
    V(SEM_UART);
}

/* --- プレイヤー：座標入力タスク --- */
void player_task() {
    int y = -1, x = -1;
    char prompt[64] = "Input Row (1-5): ";
    
    while (!game_over) {
        int c = inkey(0);
        if (c <= 0 || c == 10 || c == 13) continue;

        if (c >= '1' && c <= '5') {
            if (y == -1) {
                y = c - '1';
                sprintf(prompt, "Row %d selected. Input Col (1-5): ", y + 1);
            } else {
                x = c - '1';
                P(SEM_BOARD);
                if (game_board.cells[y][x] == '.') {
                    game_board.cells[y][x] = 'O';
                    sprintf(prompt, "Placed at (%d, %d). Next Row (1-5): ", y+1, x+1);
                    y = -1; x = -1;
                } else {
                    sprintf(prompt, "Occupied! Select Row (1-5) again: ");
                    y = -1; x = -1;
                }
                V(SEM_BOARD);
            }
            draw_board(0, prompt);
        }
    }
}

/* --- CPU：着手タスク --- */
void cpu_task() {
    while (!game_over) {
        // ビジーループによる時間稼ぎ
        for (volatile int d = 0; d < 800000; d++); 

        P(SEM_BOARD);
        int placed = 0;
        for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
            if (game_board.cells[i/BOARD_SIZE][i%BOARD_SIZE] == '.') {
                game_board.cells[i/BOARD_SIZE][i%BOARD_SIZE] = 'X';
                placed = 1;
                break;
            }
        }
        V(SEM_BOARD);
        if (placed) draw_board(0, "CPU moved.");
    }
}

/* --- ボンバー：消去タスク --- */
void bomber_task() {
    int target = 0;
    while (!game_over) {
        for (volatile int d = 0; d < 1800000; d++); 
        
        P(SEM_BOARD);
        if (game_board.cells[target/BOARD_SIZE][target%BOARD_SIZE] != '.') {
            game_board.cells[target/BOARD_SIZE][target%BOARD_SIZE] = '.';
            V(SEM_BOARD);
            draw_board(0, "Bomb ticking...");
        } else {
            V(SEM_BOARD);
        }
        target = (target + 1) % (BOARD_SIZE * BOARD_SIZE);
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

    draw_board(0, "Ready? Input Row (1-5): ");
    begin_sch();
    return 0;
}
