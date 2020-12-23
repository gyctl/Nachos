// system.h 
//	All global variables used in Nachos are defined here.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#ifndef SYSTEM_H
#define SYSTEM_H

#include "copyright.h"
#include "utility.h"
#include "thread.h"
#include "scheduler.h"
#include "interrupt.h"
#include "stats.h"
#include "timer.h" 
#include "synch.h"


const int  MAXTHREAD = 128;   //CHANGE biggest number of thread

extern List* existThread;   // CHANGE record thread that have existed

extern char* s[];          //CHANGE record status of threads

extern int thread_PID[MAXTHREAD];     //CHANGE 数组值为零的时候代表当前的下标可以作为pid

extern int AllocatePid();  //CHANGE 调用可得到一个pid，若返回-1则表示目前无pid可以使用，即线程数已经达到最大值

extern void RecyclePid(int pid_num);   //CHANGE recycle  pid

extern void TS();  //CHANGE print info of all thread
    

// Initialization and cleanup routines
extern void Initialize(int argc, char **argv); 	// Initialization,
						// called before anything else
extern void Cleanup();				// Cleanup, called when
						// Nachos is done.

extern Thread *currentThread;			// the thread holding the CPU
extern Thread *threadToBeDestroyed;  		// the thread that just finished
extern Scheduler *scheduler;			// the ready list
extern Interrupt *interrupt;			// interrupt status
extern Statistics *stats;			// performance metrics
extern Timer *timer;				// the hardware alarm clock

#ifdef USER_PROGRAM
#include "machine.h"
extern Machine* machine;    	// user program memory and registers
//printf("filesys.hnsssssss");
#endif

#ifdef FILESYS_NEEDED 		// FILESYS or FILESYS_STUB 
#include "filesys.h"
extern FileSystem  *fileSystem;
#endif

#ifdef FILESYS
#include "synchdisk.h"
extern SynchDisk   *synchDisk;
#endif

#ifdef NETWORK
#include "post.h"
extern PostOffice* postOffice;
#endif

#endif // SYSTEM_H
