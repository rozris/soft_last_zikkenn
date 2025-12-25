#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

/* --- 外部関数の宣言 --- */
extern int inbyte(int fd);
extern int inkey(int fd);

#define BOARD_SIZE 5
#define SEM_BOARD 0
#define SEM_UART  1

typedef struct {
    //構造体で確保
    char cells[BOARD_SIZE][BOARD_SIZE];
} BOARD_DATA;

BOARD_DATA game_board;
int game_over = 0;
static unsigned long seed = 12345;

//疑似乱数生成
int my_rand() {
    seed=seed*1103515245+12345;
    return (unsigned int)(seed/65536)%32768;
}

//補助関数
int is_valid(int y,int x){
    return (y>=0 && y<BOARD_SIZE && x>=0 && x<BOARD_SIZE);
}

int count_continuous(int y,int x,int dy,int dx,char mark) {
    int count=0;
    while (is_valid(y, x) && game_board.cells[y][x]==mark){ 
        count++;
        y+=dy; 
        x+=dx; 
    }
    return count;
}

int get_strategic_move(int length,char mark,int *ry,int *rx) {
    int y,x,i,dy[]={0,1,1,1,0,-1,-1,-1},dx[]={1, 0, 1, -1, -1, 0, -1, 1};
    int candidates_y[64], candidates_x[64], count = 0;
    for (y = 0; y < BOARD_SIZE; y++) {
        for (x = 0; x < BOARD_SIZE; x++){
            if (game_board.cells[y][x] != '.') continue;
            for (i = 0; i < 8; i++) {
                if (count_continuous(y + dy[i], x + dx[i], dy[i], dx[i], mark) == length) {
                    candidates_y[count]=y; 
                    candidates_x[count]=x; 
                    count++;
                    if(count>=64)break;
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

//勝敗判定
char check_winner() {
    for (int i = 0; i < BOARD_SIZE; i++) {
        if(game_board.cells[i][0] != '.' && game_board.cells[i][0] == game_board.cells[i][1] && game_board.cells[i][1] == game_board.cells[i][2] && game_board.cells[i][2] == game_board.cells[i][3] && game_board.cells[i][3] == game_board.cells[i][4]) return game_board.cells[i][0];
    }
    for (int i = 0; i < BOARD_SIZE; i++) {
        if(game_board.cells[0][i] != '.' && game_board.cells[0][i] == game_board.cells[1][i] && game_board.cells[1][i] == game_board.cells[2][i] && game_board.cells[2][i] == game_board.cells[3][i] && game_board.cells[3][i] == game_board.cells[4][i]) return game_board.cells[0][i];
    }
    if (game_board.cells[2][2] != '.') {
        if (game_board.cells[0][0] == game_board.cells[1][1] && game_board.cells[1][1] == game_board.cells[2][2] && game_board.cells[2][2] == game_board.cells[3][3] && game_board.cells[3][3] == game_board.cells[4][4]) return game_board.cells[2][2];
        if (game_board.cells[0][4] == game_board.cells[1][3] && game_board.cells[1][3] == game_board.cells[2][2] && game_board.cells[2][2] == game_board.cells[3][1] && game_board.cells[3][1] == game_board.cells[4][0]) return game_board.cells[2][2];
    }
    for(int i = 0; i < 25; i++) {
        if(game_board.cells[i/5][i%5]=='.') return 0;
    }
    return 'D';
}

void init_game_state() {
    int i, j;
    game_over = 0;
    for (i = 0;i<BOARD_SIZE; i++)
        for (j=0;j<BOARD_SIZE;j++)game_board.cells[i][j]='.';
}

/* --- 高速描画用：指定マスの更新 --- */
void update_cell(int y, int x, char mark) {
    char buf[32];
    P(SEM_UART);
    // 枠組みに基づいた座標計算: 行 = 7 + y*2, 列 = 10 + x*4
    sprintf(buf, "\033[%d;%dH%c", 7 + y * 2, 10 + x * 4, mark);
    my_write(0, buf);
    // カーソルをメッセージ行の先頭へ戻しておく
    my_write(0, "\033[18;1H");
    V(SEM_UART);
}

/* --- 高速描画用：メッセージのみ更新 --- */
void update_msg(char* msg) {
    P(SEM_UART);
    my_write(0, "\033[18;1H\033[K "); // 18行目に移動し、行末まで消去
    my_write(0, msg);
    V(SEM_UART);
}

/* --- 初期画面描画（固定部分を一度に書く） --- */
void draw_board(int fd, char* msg) {
    int i;
    P(SEM_UART);
    my_write(fd, "\033[2J\033[H"); 
    my_write(fd, "五目並べの達人\r\nプレイヤー: O | CPU: X | 爆弾:定期的に盤面を破壊します \r\n");
    my_write(fd, "マスは早い者勝ちです、列・行の順に指定してください\r\n");
    my_write(fd, "\r\n       1   2   3   4   5\r\n    +---+---+---+---+---+\r\n");
    for (i = 0; i < BOARD_SIZE; i++) {
        char row[64];
        sprintf(row, "  %d | %c | %c | %c | %c | %c |\r\n", i + 1, game_board.cells[i][0], game_board.cells[i][1], game_board.cells[i][2], game_board.cells[i][3], game_board.cells[i][4]);
        my_write(fd, row);
        my_write(fd, "    +---+---+---+---+---+\r\n");
    }
    my_write(fd, "\r\n ");
    my_write(fd, msg);
    V(SEM_UART);
}

//終了判定と通知
void check_and_announce(int fd) {
    char winner = check_winner();
    if (winner != 0) {
        game_over = 1;
        P(SEM_UART);
        my_write(fd, "\033[18;1H\033[K ");
        if (winner == 'O') my_write(fd, "YOU WIN!");
        else if (winner == 'X') {
            my_write(fd, "G A M E O V E R\r\n....................../´¯/)\n....................,/¯../\n.................../..../\n............./´¯'...'/´¯¯`·¸\n........../'/.../..../......./¨¯\\\n........('(...'...'.... ¯~/'...')\n.........\\.................'...../\n..........''...\\.......... _.·´\n............\\..............(\n..............\\.............\\...\n");
        } 
        else my_write(fd, "DRAW-GAME");
        my_write(fd, "\r\n り、と、ら、い、する? (Y/N): ");
        V(SEM_UART);
    }
}

//タスク1: プレイヤータスク
void player_task() {
    int y = -1, x = -1; char prompt[64] = "列を選択してください(1-5): ";
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 100000; d++); continue; }
        int c = inkey(0);
        if (c <= 0 || c == 10 || c == 13) continue;
        seed += c;
        if (c >= '1' && c <= '5') {
            if (y == -1) { 
                y = c - '1'; 
                sprintf(prompt, " %d 列目選択済み. 行を選択してください: ", y+1);
                update_msg(prompt);
            }
            else {
                x = c - '1'; P(SEM_BOARD);
                if (!game_over) {
                    if (game_board.cells[y][x] == '.') { 
                        game_board.cells[y][x] = 'O'; 
                        update_cell(y, x, 'O');
                        sprintf(prompt, "(%d, %d)に設置、次の列を選択してください ", y+1, x+1);
                        check_and_announce(0);
                    } else { 
                        sprintf(prompt, "そのマスは埋まっています!: "); 
                    }
                }
                y=-1;x=-1; 
                V(SEM_BOARD);
                if(!game_over) update_msg(prompt);
            }
        }
    }
}

//CPUタスク
void cpu_task() {
    int ty, tx;
    while (1) {
        if (game_over){ for (volatile int d = 0; d < 100000; d++); continue; }
        for(volatile int d = 0; d < 200000; d++); 
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
        if(placed && !game_over) { 
            game_board.cells[ty][tx] = 'X'; 
            update_cell(ty, tx, 'X');
            check_and_announce(0);
            if(!game_over) update_msg("CPU moved.");
        }
        V(SEM_BOARD);
    }
}

//爆弾タスク
void bomber_task() {
    int target = 0;
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 100000; d++); continue; }
        for (volatile int d = 0; d < 400000; d++); 
        P(SEM_BOARD);
        if (game_over) { V(SEM_BOARD); continue; }
        int ty=target/BOARD_SIZE, tx=target % BOARD_SIZE;
        if (my_rand() % 100 < 20) {
            int dy[] = {0, 0, 1, -1, 0}, dx[] = {0, 1, 0, 0, -1};
            for (int i=0;i<5;i++) {
                int ny = ty + dy[i], nx = tx + dx[i];
                if (is_valid(ny, nx)) {
                    game_board.cells[ny][nx] = '.';
                    update_cell(ny, nx, '.');
                }
            }
            if(!game_over) update_msg("!!! CROSS BOMB !!!");
        } else {
            if (game_board.cells[ty][tx] != '.') { 
                game_board.cells[ty][tx] = '.'; 
                update_cell(ty, tx, '.');
                if(!game_over) update_msg("Bomb!!");
            }
        }
        target = (target + 1 + (my_rand() % 3)) % 25;
        V(SEM_BOARD);
    }
}

//リトライタスク
void retry_task() {
    while (1) {
        int c = inkey(0);
        if (game_over && c > 0) {
            if (c == 'y' || c == 'Y') {
                P(SEM_BOARD);
                init_game_state();
                V(SEM_BOARD);
                draw_board(0, "Reset Complete. Input Row (1-5): ");
            } 
            else if (c == 'n' || c == 'N') {
                P(SEM_UART);
                my_write(0, "\033[2J\033[H\r\n [!] SHUTTING DOWN...\r\n");
                V(SEM_UART);
                P(SEM_BOARD); 
                while(1); 
            }
        }
        if (game_over) { for (volatile int d = 0; d < 50000; d++); }
        else { for (volatile int d = 0; d < 200000; d++); }
    }
}

int main(void) {
    init_kernel();
    init_game_state();
    
    set_task(player_task);
    set_task(cpu_task);
    set_task(bomber_task);
    set_task(retry_task);

    draw_board(0, "列を選択してください(1-5): ");
    begin_sch();
    return 0;
}
