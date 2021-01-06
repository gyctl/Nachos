#include "syscall.h"

void func(){
    Create("testfork");
    // Exit(0);
}

int main(){
    
    Fork(func);
    // func();
    // Yield();
    Exit(0);
}

