#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

extern int inbyte(int fd);
extern int inkey(int fd);

#define BOARD_SIZE 5
#define SEM_BOARD 0
#define SEM_UART  1

typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE];
} BOARD_DATA;

BOARD_DATA game_board;
int game_over = 0;
int is_hard_mode = 0; //難易度フラグ
static unsigned long seed = 12345;


int my_rand() {
    seed = seed * 1103515245 + 12345;
    return (unsigned int)(seed / 65536) % 32768;
}

int is_valid(int y, int x) {
    return (y >= 0 && y < BOARD_SIZE && x >= 0 && x < BOARD_SIZE);
}

int count_continuous(int y, int x, int dy, int dx, char mark) {
    int count = 0;
    while (is_valid(y, x) && game_board.cells[y][x] == mark) {
        count++;
        y += dy; x += dx;
    }
    return count;
}

/* CPU戦略用：指定した長さの連なりを探し、その端の空きマスを返す */
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

/* --- 描画・判定 --- */
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

void update_cell(int y, int x, char mark) {
    char buf[16];
    sprintf(buf, "\033[%d;%dH%c", 7 + y * 2, 7 + x * 4, mark);
    P(SEM_UART);
    my_write(0, buf);
    my_write(0, "\033[22;1H"); 
    V(SEM_UART);
}

void update_msg(char* msg) {
    P(SEM_UART);
    my_write(0, "\033[19;1H\r ");
    my_write(0, msg);
    my_write(0, "                    "); 
    V(SEM_UART);
}

void draw_board(int fd, char* msg) {
    P(SEM_UART);
    my_write(fd, "\033[2J\033[H五目並べの達人\r\n");
    my_write(fd, "プレイヤー: O | CPU: X | 爆弾: 定期的に盤面を破壊します\r\n");
    my_write(fd, "マスは早い者勝ちです。列・行の順に指定してください\r\n\r\n");
    my_write(fd, "       1   2   3   4   5\r\n    +---+---+---+---+---+\r\n");
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

void check_and_announce(int fd) {
    char winner = check_winner();
    if (winner != 0) {
        game_over = 1;
        P(SEM_UART);
        my_write(fd, "\033[18;1H\033[J"); 
        if (winner == 'O') my_write(fd, " [!] YOU WIN!");
        else if (winner == 'X') {
            my_write(fd, " G A M E O V E R\r\n....................../´¯/)\n....................,/¯../\n.................../..../\n............./´¯'...'/´¯¯`·¸\n........../'/.../..../......./¨¯\\\n........('(...'...'.... ¯~/'...')\n.........\\.................'...../\n..........''...\\.......... _.·´\n............\\..............(\n..............\\.............\\...\n");
        } 
        else my_write(fd, "DRAW-GAME");
        my_write(fd, "\r\n リトライしますか? (Y/N): ");
        V(SEM_UART);
    }
}

/* --- タスク実装 --- */

void player_task() {
    int y = -1, x = -1; char prompt[64] = "列を選択してください(1-5): ";
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 150000; d++); continue; }
        int c = inkey(0);
        if (c <= 0 || c == 10 || c == 13) { for (volatile int d = 0; d < 50000; d++); continue; }
        seed += c;
        if (c >= '1' && c <= '5') {
            if (y == -1) { 
                y = c - '1'; sprintf(prompt, " %d 列目選択済み. 行を選択してください: ", y+1);
                update_msg(prompt);
            } else {
                x = c - '1'; P(SEM_BOARD);
                if (!game_over) {
                    if (game_board.cells[y][x] == '.') { 
                        game_board.cells[y][x] = 'O'; update_cell(y, x, 'O');
                        sprintf(prompt, "(%d, %d)に設置、次の列を選択してください ", y+1, x+1);
                        check_and_announce(0);
                    } else { sprintf(prompt, "そのマスは埋まっています!: "); }
                }
                y=-1; x=-1; V(SEM_BOARD);
                if(!game_over) update_msg(prompt);
            }
        }
    }
}

void cpu_task() {
    int ty, tx;
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 200000; d++); continue; }
        
        // 思考時間のランダム化（0.5〜1.5秒、Hard Mode時は高速）
        int base = is_hard_mode ? 50000 : 125000;
        int span = is_hard_mode ? 100000 : 250000;
        int random_wait = base + (my_rand() % span);
        for (volatile int d = 0; d < random_wait; d++); 

        int placed = 0;
        P(SEM_BOARD);
        if (game_over) { V(SEM_BOARD); continue; }

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
            check_and_announce(0);
            if(!game_over) update_msg("CPU moved strategically.");
        }
        V(SEM_BOARD);
    }
}

void bomber_task() {
    int target = 0;
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 300000; d++); continue; }
        int wait_time = is_hard_mode ? 200000 : 450000;
        for (volatile int d = 0; d < wait_time; d++); 
        
        P(SEM_BOARD);
        if (game_over) { V(SEM_BOARD); continue; }
        int ty = target / BOARD_SIZE, tx = target % BOARD_SIZE;
        int changed = 0;
        if (my_rand() % 100 < 20) {
            int dy[] = {0, 0, 1, -1, 0}, dx[] = {0, 1, 0, 0, -1};
            for (int i = 0; i < 5; i++) {
                int ny = ty + dy[i], nx = tx + dx[i];
                if (is_valid(ny, nx) && game_board.cells[ny][nx] != '.') {
                    game_board.cells[ny][nx] = '.'; update_cell(ny, nx, '.'); changed = 1;
                }
            }
            if(changed && !game_over) update_msg("!!! CROSS BOMB !!!");
        } else if (game_board.cells[ty][tx] != '.') { 
            game_board.cells[ty][tx] = '.'; update_cell(ty, tx, '.');
            if(!game_over) update_msg("Bomb!!");
        }
        target = (target + 1 + (my_rand() % 3)) % 25;
        V(SEM_BOARD);
    }
}

void retry_task() {
    while (1) {
        int c = inkey(0);
        if (game_over && c > 0) {   
            if (c == 'y' || c == 'Y') {
                P(SEM_BOARD);
                is_hard_mode = (c == 'Y');
                game_over = 0;
                for (int i = 0; i < BOARD_SIZE; i++) for (int j = 0; j < BOARD_SIZE; j++) game_board.cells[i][j] = '.';
                draw_board(0, is_hard_mode ? ">> HARD MODE ACTIVATED << Row (1-5):" : "Reset Complete. Row (1-5):");
                V(SEM_BOARD);
            } else if (c == 'n' || c == 'N') {
                P(SEM_UART); my_write(0, "\033[2J\033[H\r\n [!] SHUTTING DOWN...\r\n"); V(SEM_UART);
                P(SEM_BOARD); while(1); 
            }
        }
        for (volatile int d = 0; d < (game_over ? 50000 : 200000); d++);
    }
}

int main(void) {
    init_kernel();
    for (int i = 0; i < BOARD_SIZE; i++) for (int j = 0; j < BOARD_SIZE; j++) game_board.cells[i][j] = '.';
    set_task(player_task); set_task(cpu_task); set_task(bomber_task); set_task(retry_task);
    draw_board(0, "列を選択してください(1-5): ");
    begin_sch();
    return 0;
}
