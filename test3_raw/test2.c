#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "mtk_c.h"

// 外部関数（タスク管理システムコール）のプロトタイプ宣言
extern void set_task(void (*func)());
extern void begin_sch();


void task1() {
    P(1);
    while(true)
    {
    printf("fuck");
    }
    V(1);
}


void task2() {
    P(2);
    while(true){
    printf("you\n");
    }
    V(2);
}

void task3() {
    P(2);
    while(true){
    printf("world");
    }
    V(2);
}


int main() {
    *(char*)0x00d00039='a';
    init_kernel();
    set_task(task1);
    set_task(task2);
    set_task(task3);
    begin_sch();
    return 0;
}
