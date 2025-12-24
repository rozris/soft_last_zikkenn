void retry_task() {
    while (1) {
        if (game_over) {
            // まず盤面の操作(CPUや爆弾)を止めるために盤面セマフォをロック
            P(SEM_BOARD);
            
            // 画面表示は、入力待ちの間は他者に邪魔されないよう制御が必要だが、
            // inkey自体がUARTを触るため、ループ内で適切に制御する
            while (1) {
                // セマフォを取得して入力を確認
                P(SEM_UART);
                int c = inkey(0);
                V(SEM_UART); // inkeyの直後に解放（他のタスクが描画を終えられるようにするため）

                if (c == 'y' || c == 'Y') {
                    // UARTセマフォを確保してリセット描画
                    P(SEM_UART);
                    init_game_state();
                    draw_board(0, "Game Reset! Row (1-5): ");
                    V(SEM_UART);
                    
                    // 盤面セマフォを解放して全タスクを再開
                    V(SEM_BOARD);
                    break; 
                }
                
                if (c == 'n' || c == 'N') {
                    P(SEM_UART);
                    my_write(0, "\033[2J\033[H"); // 画面クリア
                    my_write(0, "\r\n [!] PRESSED N: SHUTTING DOWN...\r\n");
                    V(SEM_UART);
                    
                    // システムを完全に停止（OSを抜ける代わりに無限ループ）
                    while(1) {
                        for (volatile int d = 0; d < 1000000; d++);
                    }
                }
                
                // 入力がない時は少し待ってから再試行
                for (volatile int d = 0; d < 100000; d++); 
            }
        }
        // game_overになるまでは低負荷で監視
        for (volatile int d = 0; d < 200000; d++); 
    }
}
