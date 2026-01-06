#include <stdarg.h>
#include <fcntl.h>

int fcntl(int fd, int cmd, ...) {
    /*ライブラリ内部でファイル記述子の状態（読み書き可能か等）を確認するために呼ばれる。*/
    if(cmd == F_GETFL) {
        return O_RDWR;
    } else {
        return 0;
    }
}
