#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

extern int inbyte(int fd);
extern int inkey(int fd);

#define BOARD_SIZE 5
#define SEM_BOARD 0  /* 盤面データ操作用のセマフォID */
#define SEM_UART  1  /* UART出力（画面描画）用のセマフォID */

typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE];
} BOARD_DATA;

/* 共有資源：ゲーム盤面データ */
BOARD_DATA game_board;
int game_over = 0;
int is_hard_mode = 0; 
static unsigned long seed = 12345;

/*線形合同法による擬似乱数*/
int my_rand() {
    seed = seed * 1103515245 + 12345;
    return (unsigned int)(seed / 65536) % 32768;
}

/*指定された座標が盤面内かチェック*/
int is_valid(int y, int x) {
    return (y >= 0 && y < BOARD_SIZE && x >= 0 && x < BOARD_SIZE);
}

/*特定方向の連続する駒の数をカウント*/
int count_continuous(int y, int x, int dy, int dx, char mark) {
    int count = 0;
    while (is_valid(y, x) && game_board.cells[y][x] == mark) {
        count++;
        y += dy; x += dx;
    }
    return count;
}

/*CPUの思考ルーチン、指定した長さの連なりを探索*/
int get_strategic_move(int length, char mark, int *ry, int *rx) {
    int y, x, i, dy[] = {0, 1, 1, 1, 0, -1, -1, -1}, dx[] = {1, 0, 1, -1, -1, 0, -1, 1};
    int candidates_y[64], candidates_x[64], count = 0;
    for (y = 0; y < BOARD_SIZE; y++) {
        for (x = 0; x < BOARD_SIZE; x++) {
            if (game_board.cells[y][x] != '.') continue;
            for (i = 0; i < 8; i++) {
                if (count_continuous(y + dy[i], x + dx[i], dy[i], dx[i], mark) == length) {
                    candidates_y[count] = y; candidates_x[count] = x;
                    count++;
                    if (count >= 64) break;
                }
            }
        }
    }
    if (count > 0) {
        int target = my_rand() % count;
        *ry = candidates_y[target]; *rx = candidates_x[target];
        return 1;
    }
    return 0;
}

/* 盤面全域を走査し、5つ並んだ駒があるか、または引き分けかを判定 */
char check_winner() {
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (game_board.cells[i][0] != '.' && game_board.cells[i][0] == game_board.cells[i][1] && game_board.cells[i][1] == game_board.cells[i][2] && game_board.cells[i][2] == game_board.cells[i][3] && game_board.cells[i][3] == game_board.cells[i][4]) return game_board.cells[i][0];
        if (game_board.cells[0][i] != '.' && game_board.cells[0][i] == game_board.cells[1][i] && game_board.cells[1][i] == game_board.cells[2][i] && game_board.cells[2][i] == game_board.cells[3][i] && game_board.cells[3][i] == game_board.cells[4][i]) return game_board.cells[0][i];
    }
    if (game_board.cells[2][2] != '.') {
        if (game_board.cells[0][0] == game_board.cells[1][1] && game_board.cells[1][1] == game_board.cells[2][2] && game_board.cells[2][2] == game_board.cells[3][3] && game_board.cells[3][3] == game_board.cells[4][4]) return game_board.cells[2][2];
        if (game_board.cells[0][4] == game_board.cells[1][3] && game_board.cells[1][3] == game_board.cells[2][2] && game_board.cells[2][2] == game_board.cells[3][1] && game_board.cells[3][1] == game_board.cells[4][0]) return game_board.cells[2][2];
    }
    for (int i = 0; i < 25; i++) if (game_board.cells[i / 5][i % 5] == '.') return 0;
    return 'D';
}

/*特定の座標の表示を更新*/
void update_cell(int y, int x, char mark) {
    char buf[16];
    sprintf(buf, "\033[%d;%dH%c", 7 + y * 2, 7 + x * 4, mark);
    P(SEM_UART); //UART描画権の取得
    my_write(0, buf);
    my_write(0, "\033[22;1H"); 
    V(SEM_UART); //UART描画権の解放
}

/*画面下部のメッセージ領域の更新*/
void update_msg(char* msg) {
    if (game_over) return; // ゲーム終了後は更新を停止
    P(SEM_UART);
    my_write(0, "\033[19;1H\033[K "); 
    my_write(0, msg);
    my_write(0, "\033[22;1H"); 
    V(SEM_UART);
}

/*初期盤面および枠線の描画、全マスの現在の状態を出力*/
void draw_board(int fd, char* msg) {
    P(SEM_UART);
    my_write(fd, "\033[2J\033[H【 五目並べの達人 】\r\n");
    my_write(fd, "あなた: O  |  CPU: X  |  爆弾に注意! \r\n");
    my_write(fd, "1から5の数字キーで場所を指定してください（列、行の順）\r\n\r\n");
    my_write(fd, "      1   2   3   4   5\r\n    +---+---+---+---+---+\r\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        char row[64];
        sprintf(row, "  %d | . | . | . | . | . |\r\n", i + 1);
        my_write(fd, row);
        my_write(fd, "    +---+---+---+---+---+\r\n");
    }
    V(SEM_UART);
    for (int ty = 0; ty < BOARD_SIZE; ty++) {
        for (int tx = 0; tx < BOARD_SIZE; tx++) {
            if (game_board.cells[ty][tx] != '.') update_cell(ty, tx, game_board.cells[ty][tx]);
        }
    }
    update_msg(msg);
}

/*勝利判定の結果を確認*/
void check_and_announce(int fd) {
    char winner = check_winner();
    if (winner != 0) {
        game_over = 1;
        P(SEM_UART);
        my_write(fd, "\033[18;1H\033[J"); 
        if (winner == 'O') my_write(fd, "YOU WIN!!");
        else if (winner == 'X') my_write(fd, "GAME OVER");
        else my_write(fd, "DROW");
        my_write(fd, "\r\n もう一度遊びますか？ (y/n): ");
        V(SEM_UART);
    }
}

/*プレイヤー入力を処理するタスク*/
void player_task() {
    /*入力まち*/
    int y = -1, x = -1; char prompt[80] = "列を選択してください(1-5): ";
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 150000; d++); continue; }
        int c = inkey(0);
        if (c <= 0 || c == 10 || c == 13) { for (volatile int d = 0; d < 50000; d++); continue; }
        seed += c; //入力のタイミングを乱数シードに反映
        if (c >= '1' && c <= '5') {
            if (y == -1) { 
                y = c - '1'; 
                sprintf(prompt, "%d列目を選択中。行を選択してください(1-5): ", y + 1);
                update_msg(prompt);
            } else {
                x = c - '1'; 
                P(SEM_BOARD); //盤面データの操作権取得
                if (!game_over) {
                    if (game_board.cells[y][x] == '.') { 
                        game_board.cells[y][x] = 'O'; update_cell(y, x, 'O');
                        sprintf(prompt, "(%d, %d) に配置しました。次の列を選んでください。", y + 1, x + 1);
                        check_and_announce(0);
                    } else { 
                        sprintf(prompt, "そのマスには既に石が置かれています。"); 
                    }
                }
                y = -1; x = -1; 
                V(SEM_BOARD); //盤面データの操作権解放
                if (!game_over) update_msg(prompt);
            }
        }
    }
}

/* CPUタスク */
void cpu_task() {
    char log_buf[64];
    int ty, tx;
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 200000; d++); continue; }
        /*行動タイミング*/
        int base = is_hard_mode ? 50000  : 90000;
        int span = is_hard_mode ? 90000 : 200000;
        int random_wait = base + (my_rand() % span);
        for (volatile int d = 0; d < random_wait; d++); 

        int placed = 0;
        P(SEM_BOARD);
        if (game_over) { V(SEM_BOARD); continue; }

        /* 優先順位：CPU4 > プレイヤー4 > CPU3 > プレイヤー3 > 隣*/
        if (get_strategic_move(4, 'X', &ty, &tx)) placed = 1;
        else if (get_strategic_move(4, 'O', &ty, &tx)) placed = 1;
        else if (get_strategic_move(3, 'X', &ty, &tx)) placed = 1;
        else if (get_strategic_move(3, 'O', &ty, &tx)) placed = 1;
        else if (get_strategic_move(2, 'X', &ty, &tx)) placed = 1;
        else if (get_strategic_move(1, 'X', &ty, &tx)) placed = 1;
        else {
            int start = my_rand() % 25;
            for (int i = 0; i < 25; i++) {
                int idx = (start + i) % 25;
                if (game_board.cells[idx / 5][idx % 5] == '.') { ty = idx / 5; tx = idx % 5; placed = 1; break; }
            }
        }

        if (placed && !game_over) {
            game_board.cells[ty][tx] = 'X'; update_cell(ty, tx, 'X');
            /*UART2へのログ出力*/
            sprintf(log_buf, "CPU動作(%d, %d)\r\n", ty + 1, tx + 1);
            P(SEM_UART);
            my_write(4, log_buf); // fd=1(UART2)へ出力
            V(SEM_UART);
            check_and_announce(0);
        }
        V(SEM_BOARD);
    }
}

/*爆弾タスク*/
void bomber_task() {
    char log_buf[64];
    int occupied_y[25], occupied_x[25]; //駒がある座標を保持する配列
    int count;
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 300000; d++); continue; }
        int wait_time = is_hard_mode ? 200000 : 450000;
        for (volatile int d = 0; d < wait_time; d++); 
        
        P(SEM_BOARD);
        if (game_over) { V(SEM_BOARD); continue; }

        /*駒が置かれている座標をリストアップ*/
        count = 0;
        for (int y = 0; y < BOARD_SIZE; y++) {
            for (int x = 0; x < BOARD_SIZE; x++) {
                if (game_board.cells[y][x] != '.') {
                    occupied_y[count] = y;
                    occupied_x[count] = x;
                    count++;
                }
            }
        }
        /*爆発処理*/
        if (count > 0) {
            //リストの中からランダムに1つ選択
            int idx = my_rand() % count;
            int ty = occupied_y[idx];
            int tx = occupied_x[idx];
            int changed = 0;

            /* 20%の確率で十字爆弾 */
            if (my_rand() % 100 < 20) {
                int dy[] = {0, 0, 1, -1, 0}, dx[] = {0, 1, 0, 0, -1};
                for (int i = 0; i < 5; i++) {
                    int ny = ty + dy[i], nx = tx + dx[i];
                    if (is_valid(ny, nx) && game_board.cells[ny][nx] != '.') {
                        game_board.cells[ny][nx] = '.'; 
                        update_cell(ny, nx, '.'); // 画面上の駒を消す
                        changed = 1;
                    }
                }
                if (changed && !game_over) {
                    update_msg("! ! ! どーん ! ! ! ");      
                    sprintf(log_buf, "十字爆弾爆発(%d, %d)\r\n", ty + 1, tx + 1);
                    P(SEM_UART);
                    my_write(4, log_buf); 
                    V(SEM_UART);
                }
            } 
            /*通常爆弾*/
            else {
                game_board.cells[ty][tx] = '.'; 
                update_cell(ty, tx, '.');
                if (!game_over) {
                    update_msg("どーん!");                   
                    sprintf(log_buf, "爆弾爆発(%d, %d)\r\n", ty + 1, tx + 1);
                    P(SEM_UART);
                    my_write(4, log_buf);
                    V(SEM_UART);
                }
            }
        }
        V(SEM_BOARD);
    }
}

/* ゲーム終了後のリトライ入力を監視するタスク */
void retry_task() {
    while (1) {
        int c = inkey(0);
        if (game_over && c > 0) {   
            if (c == 'y' || c == 'Y') {
                P(SEM_BOARD);
                is_hard_mode = (c == 'Y'); //'Y'ならハードモード
                game_over = 0;
                /* 盤面データの初期化 */
                for (int i = 0; i < BOARD_SIZE; i++) for (int j = 0; j < BOARD_SIZE; j++) game_board.cells[i][j] = '.';
                draw_board(0, is_hard_mode ? "-- hard mord --。列を選択してください: " : "リセット完了。列を選択してください: ");
                V(SEM_BOARD);
            } else if (c == 'n' || c == 'N') {
                P(SEM_UART); my_write(0, "\033[2J\033[H\r\n ゲーム終了・・・\r\n"); V(SEM_UART);
                P(SEM_BOARD); while(1); //0システム停止（無限ループ）
            }
        }
        for (volatile int d = 0; d < (game_over ? 50000 : 200000); d++);
    }
}

int main(void) {
    init_kernel();
    for (int i = 0; i < BOARD_SIZE; i++) for (int j = 0; j < BOARD_SIZE; j++) game_board.cells[i][j] = '.';
    P(SEM_UART);
    my_write(1, "\033[2J\033[H--- デバックログ  ---\r\n");
    V(SEM_UART);
    set_task(player_task); 
    set_task(cpu_task); 
    set_task(bomber_task); 
    set_task(retry_task);
    
    draw_board(0, "ゲームを開始します。列を選択してください(1-5): ");
    begin_sch(); 
    return 0;
}
