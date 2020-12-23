#include "syscall.h"

int main(){
    Create("testcode");
    Yield();
    Exit(0);
}