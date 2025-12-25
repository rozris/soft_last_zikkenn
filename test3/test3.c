#include <stdio.h>
#include <stdarg.h>
#include "mtk_c.h"

extern int inbyte(int fd);
extern int inkey(int fd);

#define BOARD_SIZE 5
#define SEM_BOARD 0
#define SEM_UART  1

typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE];#include <stdio.h>
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

/* --- 指定マスの更新 (絶対座標指定) --- */
void update_cell(int y, int x, char mark) {
    char buf[32];
    P(SEM_UART);
    
    /* * 座標計算の根拠:
     * 行: 7行目から1行おき (7, 9, 11, 13, 15) -> 7 + y * 2
     * 列: 1枚目の画像では '1' の真下が 9列目付近。
     * "  1 | . |" の構成なら、最初の '.' は 9列目。
     * その後 4文字おき (| . |) なので 9, 13, 17, 21, 25列目となる。
     */
    sprintf(buf, "\033[%d;%dH%c", 7 + y * 2, 7 + x * 4, mark);
    my_write(0, buf);
    
    // カーソルを入力待ちの邪魔にならない位置（画面最下部）へ移動
    my_write(0, "\033[22;1H"); 
    V(SEM_UART);
}

/* --- メッセージ行の更新 --- */
void update_msg(char* msg) {
    P(SEM_UART);
    // 19行目に移動し、行全体を消去してからメッセージを書く
    my_write(0, "\033[19;1H\033[2K "); 
    my_write(0, msg);
    V(SEM_UART);
}

/* --- 初期画面描画 (枠組みを固定) --- */
void draw_board(int fd, char* msg) {
    int i;
    P(SEM_UART);
    // 画面クリア
    my_write(fd, "\033[2J\033[H"); 
    my_write(fd, "五目並べの達人\r\n");
    my_write(fd, "プレイヤー: O | CPU: X | 爆弾: 定期的に盤面を破壊します\r\n");
    my_write(fd, "マスは早い者勝ちです。列・行の順に指定してください\r\n\r\n");
    my_write(fd, "       1   2   3   4   5\r\n");
    my_write(fd, "    +---+---+---+---+---+\r\n");
    
    for (i = 0; i < BOARD_SIZE; i++) {
        // 余計なスペースを入れず、固定幅で描画する (重要)
        char row[64];
        sprintf(row, "  %d | . | . | . | . | . |\r\n", i + 1);
        my_write(fd, row);
        my_write(fd, "    +---+---+---+---+---+\r\n");
    }
    
    my_write(fd, "\r\n ");
    V(SEM_UART);
    
    // 既に石がある場合は上書き（リトライ時用）
    for (int ty = 0; ty < BOARD_SIZE; ty++) {
        for (int tx = 0; tx < BOARD_SIZE; tx++) {
            if (game_board.cells[ty][tx] != '.') {
                update_cell(ty, tx, game_board.cells[ty][tx]);
            }
        }
    }
    update_msg(msg);
}


// 終了判定と通知 (AA表示時の残骸消去を追加)
void check_and_announce(int fd) {
    char winner = check_winner();
    if (winner != 0) {
        game_over = 1;
        P(SEM_UART);
        
        // 1. メッセージ行（18行目）に移動し、そこから画面末尾までを完全に消去する
        // \033[18;1H : 18行1列へ移動
        // \033[J     : カーソル位置から画面の一番下までを消去
        my_write(fd, "\033[18;1H\033[J"); 
        
        if (winner == 'O') {
            my_write(fd, " [!] YOU WIN!");
        }
        else if (winner == 'X') {
            // 2. 綺麗な状態になったエリアにAAを描画
            my_write(fd, " G A M E O V E R\r\n");
            my_write(fd, "....................../´¯/)\n");
            my_write(fd, "....................,/¯../\n");
            my_write(fd, ".................../..../\n");
            my_write(fd, "............./´¯'...'/´¯¯`·¸\n");
            my_write(fd, "........../'/.../..../......./¨¯\\\n");
            my_write(fd, "........('(...'...'.... ¯~/'...')\n");
            my_write(fd, ".........\\.................'...../\n");
            my_write(fd, "..........''...\\.......... _.·´\n");
            my_write(fd, "............\\..............(\n");
            my_write(fd, "..............\\.............\\...\n");
        } 
        else {
            my_write(fd, " [!] DRAW-GAME");
        }
        
        // 3. リトライの問いかけを表示
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
} BOARD_DATA;

BOARD_DATA game_board;
int game_over = 0;
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
        y += dy;
        x += dx;
    }
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
                    candidates_y[count] = y;
                    candidates_x[count] = x;
                    count++;
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

void init_game_state() {
    game_over = 0;
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++) game_board.cells[i][j] = '.';
}

/* --- 指定マスの更新 (P/V操作を my_write 直前に限定) --- */
void update_cell(int y, int x, char mark) {
    char buf[16];
    sprintf(buf, "\033[%d;%dH%c", 7 + y * 2, 9 + x * 4, mark);
    P(SEM_UART);
    my_write(0, buf);
    my_write(0, "\033[22;1H"); 
    V(SEM_UART);
}

/* --- メッセージ行の更新 (\r とスペースによる上書きで通信量削減) --- */
void update_msg(char* msg) {
    P(SEM_UART);
    my_write(0, "\033[19;1H\r ");
    my_write(0, msg);
    my_write(0, "                    "); // 残り文字消去用スペース
    V(SEM_UART);
}

void draw_board(int fd, char* msg) {
    int i;
    P(SEM_UART);
    my_write(fd, "\033[2J\033[H"); 
    my_write(fd, "五目並べの達人\r\n");
    my_write(fd, "プレイヤー: O | CPU: X | 爆弾: 定期的に盤面を破壊します\r\n");
    my_write(fd, "マスは早い者勝ちです。列・行の順に指定してください\r\n\r\n");
    my_write(fd, "       1   2   3   4   5\r\n");
    my_write(fd, "    +---+---+---+---+---+\r\n");
    for (i = 0; i < BOARD_SIZE; i++) {
        char row[64];
        sprintf(row, "  %d | . | . | . | . | . |\r\n", i + 1);
        my_write(fd, row);
        my_write(fd, "    +---+---+---+---+---+\r\n");
    }
    my_write(fd, "\r\n ");
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
            my_write(fd, " G A M E O V E R\r\n");
            my_write(fd, "....................../´¯/)\n....................,/¯../\n.................../..../\n............./´¯'...'/´¯¯`·¸\n........../'/.../..../......./¨¯\\\n........('(...'...'.... ¯~/'...')\n.........\\.................'...../\n..........''...\\.......... _.·´\n............\\..............(\n..............\\.............\\...\n");
        } 
        else my_write(fd, " [!] DRAW-GAME");
        my_write(fd, "\r\n り、と、ら、い、する? (Y/N): ");
        V(SEM_UART);
    }
}

void player_task() {
    int y = -1, x = -1; char prompt[64] = "列を選択してください(1-5): ";
    while (1) {
        if (game_over) { for (volatile int d = 0; d < 150000; d++); continue; }
        int c = inkey(0);
        if (c <= 0 || c == 10 || c == 13) {
            for (volatile int d = 0; d < 50000; d++); // 入力待ち中の負荷軽減
            continue;
        }
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
                y=-1;x=-1; V(SEM_BOARD);
                if(!game_over) update_msg(prompt);
            }
        }
    }
}

// CPUインテリジェンス・タスク
void cpu_task() {
    int ty, tx;
    while (!game_over) {
        // CPUの思考時間（難易度調整用）
        for (volatile int d = 0; d < 800000; d++); 

        int placed = 0;
        P(SEM_BOARD);

        /* 優先度ロジックの実装 */
        
        // 1. CPUが4つ並んでいる -> 5つ目をおいて勝利確定
        if (get_end_position(4, 'X', &ty, &tx)) placed = 1;
        
        // 2. プレイヤーが4つ並んでいる -> 阻止する
        else if (get_end_position(4, 'O', &ty, &tx)) placed = 1;
        
        // 3. CPUが3つ並んでいる -> 両端を伸ばす
        else if (get_end_position(3, 'X', &ty, &tx)) placed = 1;
        
        // 4. プレイヤーが3つ並んでいる -> 阻止する
        else if (get_end_position(3, 'O', &ty, &tx)) placed = 1;
        
        // 5. CPUが2つ並んでいる -> 伸ばす
        else if (get_end_position(2, 'X', &ty, &tx)) placed = 1;
        
        // 6. CPUのマークの隣に置く（1つ並んでいる場合）
        else if (get_end_position(1, 'X', &ty, &tx)) placed = 1;
        
        // 7. 条件に合う場所がなければ空いている場所を探す
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
