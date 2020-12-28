#include "syscall.h"

int main(){
    int a;
    a = Exec("../test/testcode");
    Join(a);
    Exit(0);
}