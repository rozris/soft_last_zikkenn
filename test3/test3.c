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

//勝敗判定部分
char check_winner() {
    int i, j;
    //横の判定
    for (i = 0; i < BOARD_SIZE; i++) {
        if (game_board.cells[i][0] != '.' &&
            game_board.cells[i][0] == game_board.cells[i][1] &&
            game_board.cells[i][1] == game_board.cells[i][2] &&
            game_board.cells[i][2] == game_board.cells[i][3] &&
            game_board.cells[i][3] == game_board.cells[i][4]) return game_board.cells[i][0];
    }
    //縦の判定
    for (j = 0; j < BOARD_SIZE; j++) {
        if (game_board.cells[0][j] != '.' &&
            game_board.cells[0][j] == game_board.cells[1][j] &&
            game_board.cells[1][j] == game_board.cells[2][j] &&
            game_board.cells[2][j] == game_board.cells[3][j] &&
            game_board.cells[3][j] == game_board.cells[4][j]) return game_board.cells[0][j];
    }
    //斜めの判定
    if (game_board.cells[2][2] != '.') {
        if (game_board.cells[0][0] == game_board.cells[1][1] && game_board.cells[1][1] == game_board.cells[2][2] &&
            game_board.cells[2][2] == game_board.cells[3][3] && game_board.cells[3][3] == game_board.cells[4][4]) return game_board.cells[2][2];
        if (game_board.cells[0][4] == game_board.cells[1][3] && game_board.cells[1][3] == game_board.cells[2][2] &&
            game_board.cells[2][2] == game_board.cells[3][1] && game_board.cells[3][1] == game_board.cells[4][0]) return game_board.cells[2][2];
    }

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
        if (winner == 'O') my_write(fd, "\r\n [!] YOU!!!!!!!!!!!!!!1 WIN!!!!!!!!!!!!!!\r\n");
        else if (winner == 'X'){
            my_write(fd, "\r\n [!] G☆A☆M☆E☆O☆V☆E☆R\r\n");
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
        }
        else my_write(fd, "\r\n [!] ***** DRAW GAME *****\r\n");
    } else {
        my_write(fd, "\r\n ");
        my_write(fd, msg);
    }
    V(SEM_UART);
}




//CPU補助関数群
int is_valid(int y, int x) {
    return (y >= 0 && y < BOARD_SIZE && x >= 0 && x < BOARD_SIZE);
}

/*count_continuous
連続数をカウントする関数
(y, x) から方向 (dy, dx) へ進みながら mark がいくつ並んでいるか返す
*/

int count_continuous(int y, int x, int dy, int dx, char mark) {
    int count = 0;
    while (is_valid(y, x) && game_board.cells[y][x] == mark) {
        count++;
        y += dy;
        x += dx;
    }
    return count;
}

/*get_end_position
  指定した長さの連なりを探し、その両端の空きマスを見つける関数
  返り値: 見つかれば1(座標を*ry, *rxに格納)、なければ0
 */
int get_end_position(int length, char mark, int *ry, int *rx) {
    int y, x, i;
    // 8方向のベクトル (右, 下, 右下, 左下, およびその逆方向)
    int dy[] = {0, 1, 1, 1, 0, -1, -1, -1};
    int dx[] = {1, 0, 1, -1, -1, 0, -1, 1};

    for (y = 0; y < BOARD_SIZE; y++) {
        for (x = 0; x < BOARD_SIZE; x++) {
            // 起点が調べたいマークである必要はない（空きマスから連なりをチェックするため）
            if (game_board.cells[y][x] != '.') continue;

            for (i = 0; i < 8; i++) {
                // 空きマス (y, x) の隣から、指定方向に mark が何個並んでいるか
                if (count_continuous(y + dy[i], x + dx[i], dy[i], dx[i], mark) == length) {
                    *ry = y;
                    *rx = x;
                    return 1; // ターゲット発見
                }
            }
        }
    }
    return 0;
}

//プレイヤータスク、座標入力
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
                } 
                else {
                    sprintf(prompt, "Occupied! Select Row (1-5) again: ");
                    y = -1; x = -1;
                }
                V(SEM_BOARD);
            }
            draw_board(0, prompt);
        }
    }
    while(game_over) {
        for (volatile int d = 0; d < 1800000; d++);
    }
}

//CPU基本タスク
void cpu_task() {
    int ty, tx;
    while (!game_over) {
        //CPUの思考時間（難易度調整用）
        for (volatile int d = 0; d < 800000; d++); 

        int placed = 0;
        P(SEM_BOARD);

        /*動作ロジック*/
        
        //CPUが4つ並んでいる->5つ目をおいて勝利確定
        if (get_end_position(4, 'X', &ty, &tx)) placed = 1;
        
        //プレイヤーが4つ並んでいる->阻止
        else if (get_end_position(4, 'O', &ty, &tx)) placed = 1;
        
        //CPUが3つ並んでいる->両端を伸ばす
        else if (get_end_position(3, 'X', &ty, &tx)) placed = 1;
        
        //プレイヤーが3つ並んでいる->阻止
        else if (get_end_position(3, 'O', &ty, &tx)) placed = 1;
        
        //CPUが2つ並んでいる->伸ばす
        else if (get_end_position(2, 'X', &ty, &tx)) placed = 1;
        
        //CPUのマークの隣に置く（1つ並んでいる場合）
        else if (get_end_position(1, 'X', &ty, &tx)) placed = 1;
        
        //条件に合う場所がなければ空いている場所を探す
        else {
            for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
                if (game_board.cells[i / BOARD_SIZE][i % BOARD_SIZE] == '.') {
                    ty = i / BOARD_SIZE;
                    tx = i % BOARD_SIZE;
                    placed = 1;
                    break;
                }
            }
        }

        if (placed && !game_over) {
            game_board.cells[ty][tx] = 'X';
            V(SEM_BOARD);
            draw_board(0, "CPU moved strategically!");
        } else {
            V(SEM_BOARD);
        }
    }

    // ゲーム終了後の無限ループ
    while (game_over) {
        for (volatile int d = 0; d < 1800000; d++);
    }
}

//爆弾タスク
void bomber_task() {
    int target = 0;
    while (!game_over) {
        for (volatile int d = 0; d < 1800000; d++); 
        
        P(SEM_BOARD);
        if (game_board.cells[target/BOARD_SIZE][target%BOARD_SIZE] != '.') {
            game_board.cells[target/BOARD_SIZE][target%BOARD_SIZE] = '.';
            V(SEM_BOARD);
            draw_board(0, "Bomb!! Cell cleared!!!!!!!!!!!");
        } else {
            V(SEM_BOARD);
        }
        target = (target + 1) % (BOARD_SIZE * BOARD_SIZE);
    }
    while(game_over) {
        for (volatile int d = 0; d < 1800000; d++);
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
