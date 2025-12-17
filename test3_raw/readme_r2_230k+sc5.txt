mon_r2_230k+sc5.o
  compiled: 2023.12.13
    serial 2ポート対応
    USB-シリアル通信速度 230kbps
    SYSTEM_CALL No.5(SKIPMT) の機能追加, skipmt() 実装時動作確認済

mon_r2_230k+sc5v2.o
  compiled: 2024.12.14
    基本機能は mon_r2_230k+sc5.o と同じ
    SYSTEM_CALL No.5 (SKIPMT) 判定後の jmp先を調整
    環境変化に対する動作安定性向上

--

mon_r2_230k+sc5.o
  compiled: 2023.12.13
    Supported 2 serial ports.
    USB to serial signal speed: 230kbps
    Add the function of SYSTEM_CALL No.5 (SKIPMT), 
    and confirm to work the skipmt() function implementation.

mon_r2_230k+sc5v2.o
  compiled: 2024.12.14
    Basic functions are same with mon_r2_230k+sc5.o 
    Adjust jmp destination after detecting SYSTEM_CALL No.5 (SKIPMT)
    and improve the stability of work against different targets.

