#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

/* --- 外部関数の宣言 --- */
extern int inbyte(int fd); // 1文字入るまで待機する関数
extern int inkey(int fd);

#define BOARD_SIZE 5
#define SEM_BOARD 0
#define SEM_UART  1

typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE];
} BOARD_DATA;

BOARD_DATA game_board;
int game_over = 0;
static unsigned long seed = 12345;

/* --- 補助関数・乱数・判定系 (変更なし) --- */
int my_rand() {
    seed = seed * 1103515245 + 12345;
    return (unsigned int)(seed / 65536) % 32768;
}

int is_valid(int y, int x) { return (y >= 0 && y < BOARD_SIZE && x >= 0 && x < BOARD_SIZE); }

int count_continuous(int y, int x, int dy, int dx, char mark) {
    int count = 0;
    while (is_valid(y, x) && game_board.cells[y][x] == mark) { count++; y += dy; x += dx; }
    return count;
}

int get_strategic_move(int length, char mark, int *ry, int *rx) {
    int y, x, i, dy[] = {0, 1, 1, 1, 0, -1, -1, -1}, dx[] = {1, 0, 1, -1, -1, 0, -1, 1};
    int candidates_y[64], candidates_x[64], count = 0;
    for (y = 0; y < BOARD_SIZE; y++) {
        for (x = 0; x < BOARD_SIZE; x++) {
            if (game_board.cells[y][x] != '.') continue;
            for (i = 0; i < 8; i++) {
                if (count_continuous(y + dy[i], x + dx[i], dy[i], dx[i], mark) == length) {
                    candidates_y[count] = y; candidates_x[count] = x; count++;
                    if (count >= 64) break;
                }
            }
        }
    }
    if (count > 0) {
        int target = my_rand() % count; *ry = candidates_y[target]; *rx = candidates_x[target];
        return 1;
    }
    return 0;
}

char check_winner() {
    int i, j;
    for (i = 0; i < BOARD_SIZE; i++) {
        if (game_board.cells[i][0] != '.' && game_board.cells[i][0] == game_board.cells[i][1] && game_board.cells[i][1] == game_board.cells[i][2] && game_board.cells[i][2] == game_board.cells[i][3] && game_board.cells[i][3] == game_board.cells[i][4]) return game_board.cells[i][0];
    }
    for (j = 0; j < BOARD_SIZE; j++) {
        if (game_board.cells[0][j] != '.' && game_board.cells[0][j] == game_board.cells[1][j] && game_board.cells[1][j] == game_board.cells[2][j] && game_board.cells[2][j] == game_board.cells[3][j] && game_board.cells[3][j] == game_board.cells[4][j]) return game_board.cells[0][j];
    }
    if (game_board.cells[2][2] != '.') {
        if (game_board.cells[0][0] == game_board.cells[1][1] && game_board.cells[1][1] == game_board.cells[2][2] && game_board.cells[2][2] == game_board.cells[3][3] && game_board.cells[3][3] == game_board.cells[4][4]) return game_board.cells[2][2];
        if (game_board.cells[0][4] == game_board.cells[1][3] && game_board.cells[1][3] == game_board.cells[2][2] && game_board.cells[2][2] == game_board.cells[3][1] && game_board.cells[3][1] == game_board.cells[4][0]) return game_board.cells[2][2];
    }
    for (i = 0; i < 25; i++) if (game_board.cells[i/5][i%5] == '.') return 0;
    return 'D';
}

/* --- ゲーム初期化用関数 --- */
void init_game_state() {
    int i, j;
    game_over = 0;
    for (i = 0; i < BOARD_SIZE; i++)
        for (j = 0; j < BOARD_SIZE; j++) game_board.cells[i][j] = '.';
}

void draw_board(int fd, char* msg) {
    int i;
    char row_buf[128], winner;
    P(SEM_UART);
    my_write(fd, "\033[2J\033[H"); 
    my_write(fd, "=== Survival Tic-Tac-Toe (5x5) ===\r\nYOU: O | CPU: X | BOMB: Clear\r\n\r\n      1   2   3   4   5\r\n    +---+---+---+---+---+\r\n");
    for (i = 0; i < BOARD_SIZE; i++) {
        sprintf(row_buf, "  %d | %c | %c | %c | %c | %c |\r\n", i + 1, game_board.cells[i][0], game_board.cells[i][1], game_board.cells[i][2], game_board.cells[i][3], game_board.cells[i][4]);
        my_write(fd, row_buf); my_write(fd, "    +---+---+---+---+---+\r\n");
    }
    winner = check_winner();
    if (winner != 0) {
        game_over = 1;
        if (winner == 'O') my_write(fd, "\r\n [!] YOU WIN!\r\n");
    else if (winner == 'X'){
        my_write(fd, "\r\n G A M E O V E R\r\n");
        my_write(fd,"....................../´¯/)\n");
        my_write(fd,"....................,/¯../\n");
        my_write(fd,".................../..../\n");
        my_write(fd,"............./´¯'...'/´¯¯`·¸\n");
        my_write(fd,"........../'/.../..../......./¨¯\\\n");
        my_write(fd,"........('(...'...'.... ¯~/'...')\n");
        my_write(fd,".........\\.................'...../\n");
        my_write(fd,"..........''...\\.......... _.·´\n");
        my_write(fd,"............\\..............(\n");
        my_write(fd,"..............\\.............\\...\n");
    }        else my_write(fd, "\r\n [!] ***** DRAW GAME *****\r\n");
        my_write(fd, "\r\n Retry? (Y/N): ");
    } else { my_write(fd, "\r\n "); my_write(fd, msg); }
    V(SEM_UART);
}

/* --- タスク1〜3 (ゲームオーバー判定後のガードを維持) --- */
void player_task() {
    int y = -1, x = -1; char prompt[64] = "Input Row (1-5): ";
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 100000; d++); continue; }
        int c = inkey(0);
        if (c <= 0 || c == 10 || c == 13) continue;
        seed += c;
        if (c >= '1' && c <= '5') {
            if (y == -1) { y = c - '1'; sprintf(prompt, "Row %d selected. Input Col: ", y+1); }
            else {
                x = c - '1'; P(SEM_BOARD);
                if (!game_over) {
                    if (game_board.cells[y][x] == '.') { game_board.cells[y][x] = 'O'; sprintf(prompt, "Placed at (%d, %d). Next Row: ", y+1, x+1); }
                    else { sprintf(prompt, "Occupied! Select Row: "); }
                }
                y = -1; x = -1; V(SEM_BOARD);
            }
            draw_board(0, prompt);
        }
    }
}

void cpu_task() {
    int ty, tx;
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 100000; d++); continue; }
        for (volatile int d = 0; d < 200000; d++); 
        P(SEM_BOARD);
        if (game_over) { V(SEM_BOARD); continue; }
        int placed = 0;
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
        if (placed && !game_over) { game_board.cells[ty][tx] = 'X'; V(SEM_BOARD); draw_board(0, "CPU moved."); }
        else { V(SEM_BOARD); }
    }
}

void bomber_task() {
    int target = 0;
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 100000; d++); continue; }
        for (volatile int d = 0; d < 400000; d++); 
        P(SEM_BOARD);
        if (game_over) { V(SEM_BOARD); continue; }
        int ty = target / BOARD_SIZE, tx = target % BOARD_SIZE;
        if (my_rand() % 100 < 20) {
            int dy[] = {0, 0, 1, -1, 0}, dx[] = {0, 1, 0, 0, -1};
            for (int i = 0; i < 5; i++) {
                int ny = ty + dy[i], nx = tx + dx[i];
                if (is_valid(ny, nx)) game_board.cells[ny][nx] = '.';
            }
            V(SEM_BOARD); draw_board(0, "!!! CROSS BOMB !!!");
        } else {
            if (game_board.cells[ty][tx] != '.') { game_board.cells[ty][tx] = '.'; V(SEM_BOARD); draw_board(0, "Bomb!!"); }
            else { V(SEM_BOARD); }
        }
        target = (target + 1 + (my_rand() % 3)) % 25;
    }
}

/* --- タスク4: リトライ監視タスク --- */
/* --- タスク4: リトライ監視タスク (inbyteを使用) --- */
void retry_task() {
    while (1) {
        // game_overになるまで待機
        while (!game_over) {
            for (volatile int d = 0; d < 200000; d++); 
        }

        // ゲームが決着したら、inbyteで入力を待つ
        // inbyteは入力があるまでこのタスクを停止させるため、他タスクを邪魔しません
        int c = inbyte(0);

        if (c == 'y' || c == 'Y') {
            // ロジックと描画を止めるためにセマフォを取得
            P(SEM_BOARD);
            P(SEM_UART);
            
            init_game_state();
            draw_board(0, "Game Reset! Row (1-5): ");
            
            V(SEM_UART);
            V(SEM_BOARD);
        } 
        else if (c == 'n' || c == 'N') {
            P(SEM_UART);
            my_write(0, "\033[2J\033[H"); 
            my_write(0, "\r\n [!] PRESSED N: SHUTTING DOWN...\r\n");
            V(SEM_UART);
            
            // 全てのセマフォを確保して他タスクを停止させ、自分も停止
            P(SEM_BOARD);
            while(1); 
        }
        // y/n 以外なら何もしない（次の inbyte を待つ）
    }
}

int main(void) {
    init_kernel();
    init_game_state();
    
    set_task(player_task);
    set_task(cpu_task);
    set_task(bomber_task);
    set_task(retry_task); // 4つ目のタスクを追加

    draw_board(0, "Ready? Input Row (1-5): ");
    begin_sch();
    return 0; // ここには到達しない
}
