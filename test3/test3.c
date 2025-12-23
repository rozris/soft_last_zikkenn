#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

/* --- 定数定義 --- */
#define BOARD_SIZE 3
#define NUMSEMAPHORE 2
#define SEM_BOARD 0
#define SEM_UART  1

/* --- 構造体定義 (ベクトル/ヒープを使わない固定長管理) --- */

/* セマフォ管理構造体 */
typedef struct {
    int count;
    int nst;                /* 予約領域 */
    TASK_ID_TYPE task_list; /* 待ちタスクのリスト */
} SEMAPHORE_TYPE;

/* 盤面データ構造体 */
typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE];
    int width;
    int height;
} BOARD_DATA;

/* --- グローバル変数 (静的確保) --- */
SEMAPHORE_TYPE semaphore[NUMSEMAPHORE];
BOARD_DATA game_board;
int game_over = 0;

/* --- 補助関数 (画面制御) --- */

void gotoxy(int fd, int x, int y) {
    char buf[20];
    /* sprintfは標準ライブラリのスタック利用なのでヒープは使いません */
    sprintf(buf, "\033[%d;%dH", y, x);
    my_write(fd, buf);
}

void draw_board(int fd) {
    int i, j;
    char cell_str[2] = {' ', '\0'};

    P(SEM_UART);
    gotoxy(fd, 1, 1);
    my_write(fd, "--- Board (1-9 to move) ---\r\n");

    for (i = 0; i < game_board.height; i++) {
        for (j = 0; j < game_board.width; j++) {
            cell_str[0] = game_board.cells[i][j];
            my_write(fd, cell_str);
            my_write(fd, " ");
        }
        my_write(fd, "\r\n");
    }
    V(SEM_UART);
}

/* --- 各タスクの実装 --- */

/* プレイヤー処理の共通ロジック */
void player_proc(int fd, char mark) {
    while (!game_over) {
        int c = inkey(fd); /* キー入力待ち */
        if (c >= '1' && c <= '9') {
            int pos = c - '1';
            int y = pos / 3;
            int x = pos % 3;

            P(SEM_BOARD);
            if (game_board.cells[y][x] == '.') {
                game_board.cells[y][x] = mark;
            }
            V(SEM_BOARD);
        }
        draw_board(fd);
    }
}

/* タスク1: プレイヤー1 (UART 0) */
void player1_task() {
    player_proc(0, 'O');
}

/* タスク2: プレイヤー2 (UART 1) */
void player2_task() {
    player_proc(1, 'X');
}

/* タスク3: 監視タスク (爆弾ロジック) */
void supervisor_task() {
    int count = 0;
    int bomb_pos = 0;

    while (!game_over) {
        /* 簡易タイマー: マルチタスクの切り替えを待つ */
        for(count = 0; count < 10000; count++); 

        P(SEM_BOARD);
        /* 1マスずつ順番に「消去」して回る簡易爆弾ロジック */
        int y = bomb_pos / 3;
        int x = bomb_pos % 3;
        if (game_board.cells[y][x] != '.') {
            game_board.cells[y][x] = '.'; 
        }
        V(SEM_BOARD);

        bomb_pos = (bomb_pos + 1) % 9;
        draw_board(0); /* UART 0側に状況を反映 */
    }
}

/* --- メイン関数 --- */

int main(void) {
    int i, j;

    /* カーネル初期化 */
    init_kernel();
    init_uart2();

    /* セマフォの手動初期化 (構造体配列) */
    semaphore[SEM_BOARD].count = 1;      /* 盤面アクセス権 */
    semaphore[SEM_BOARD].task_list = 0;
    
    semaphore[SEM_UART].count = 1;       /* 画面表示アクセス権 */
    semaphore[SEM_UART].task_list = 0;

    /* 盤面構造体の初期化 */
    game_board.width = BOARD_SIZE;
    game_board.height = BOARD_SIZE;
    for (i = 0; i < BOARD_SIZE; i++) {
        for (j = 0; j < BOARD_SIZE; j++) {
            game_board.cells[i][j] = '.';
        }
    }

    /* タスクの登録 */
    set_task(player1_task);
    set_task(player2_task);
    set_task(supervisor_task);

    /* スケジューリング開始 */
    begin_sch();

    return 0;
}
