#include <stdarg.h>
#include <fcntl.h>
int fcntl(int fd, int cmd, ...);
 if(cmd == F_GETFL)
 {
 return O_RDWR;
 }
 else
 {
 return 0;
 }
}
