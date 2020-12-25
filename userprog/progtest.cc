// progtest.cc 
//	Test routines for demonstrating that Nachos can load
//	a user program and execute it.  
//
//	Also, routines for testing the Console hardware device.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "console.h"
#include "addrspace.h"
#include "synch.h"

//----------------------------------------------------------------------
// StartProcess
// 	Run a user program.  Open the executable, load it into
//	memory, and jump to it.
//----------------------------------------------------------------------

void simpleThread(){
    printf("=======执行第二个程序======\n");
    machine->Run();
}

void
StartProcess(char *filename)
{
    OpenFile *executable = fileSystem->Open(filename,"");
    // OpenFile *executable1 = fileSystem->Open(filename);
    AddrSpace *space;
    // AddrSpace *space1;
    if (executable == NULL) {
	printf("Unable to open file %s\n", filename);
	return;
    }
    // printf("=======初始化第一个线程=============\n");
    space = new AddrSpace(executable);              //二者运行同一个程序，分配用户空间
    // printf("=======初始化第二个线程=============\n");
    // space1 = new AddrSpace(executable1);                 //分配用户空间
    // Thread *thread = new Thread("secondThread",0);  //定义一个新的线程
    // space1->InitRegisters();	   //新定义的线程为运行程序做初始化准备
    // space1->RestoreState();       //新线程将用户页表装载到系统页表
    currentThread->space = space;   //当前线程的space指针指向一个用户空间
    // thread->space = space1;         //新定义的线程space指针指向一个用户空间
    // thread->Fork(simpleThread,1);    //fork中调用了yield
    // currentThread->Yield();         //切换线程，切换到优先级较高的线程
    delete executable;
    // delete executable1;
    space->InitRegisters();		// set the initial register values
    space->RestoreState();		// load page table register
    printf("=========执行第一个程序=========\n");
    machine->Run();			// jump to the user progam
    ASSERT(FALSE);			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}

// Data structures needed for the console test.  Threads making
// I/O requests wait on a Semaphore to delay until the I/O completes.

static Console *console;
static Semaphore *readAvail;
static Semaphore *writeDone;

//----------------------------------------------------------------------
// ConsoleInterruptHandlers
// 	Wake up the thread that requested the I/O.
//----------------------------------------------------------------------

static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

//----------------------------------------------------------------------
// ConsoleTest
// 	Test the console by echoing characters typed at the input onto
//	the output.  Stop when the user types a 'q'.
//----------------------------------------------------------------------

void 
ConsoleTest (char *in, char *out)
{
    char ch;

    console = new Console(in, out, ReadAvail, WriteDone, 0);
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    
    for (;;) {
	readAvail->P();		// wait for character to arrive
	ch = console->GetChar();
	console->PutChar(ch);	// echo it!
	writeDone->P() ;        // wait for write to finish
	if (ch == 'q') return;  // if q, quit
    }
}
