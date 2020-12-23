#include "syscall.h"

int 
main()
{
    
    int a, b, i ;
    char buffer[20];         //变量的定义必须在放在最前面
    for (i =0;i<20;i++){
        buffer[i] = '\0';
    }
    // Create("aa");
    // Create("ab");
    a = Open("aa");
    b = Open("ab");
    Read(buffer,8,a);
    Write(buffer,8,b);
    Close(a);
    Close(b);
    Exit(0);
}