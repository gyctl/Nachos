// system.cc 
//	Nachos initialization and cleanup routines.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"

// This defines *all* of the global data structures used by Nachos.
// These are all initialized and de-allocated by this file.

Thread *currentThread;			// the thread we are running now
Thread *threadToBeDestroyed;  		// the thread that just finished
Scheduler *scheduler;			// the ready list
Interrupt *interrupt;			// interrupt status
Statistics *stats;			// performance metrics
Timer *timer;				// the hardware timer device,
					// for invoking context switches

Semaphore *num_mutex;
Semaphore *sector_mutex[1024];
int num_Reader[1024];

void
ReadBegin(int sector){
    num_mutex->P();
    num_Reader[sector]++;
    if (num_Reader[sector] == 1 ){
        sector_mutex[sector]->P();
    }
    num_mutex->V();
}
void
ReadEnd(int sector){
    num_mutex->P();
    num_Reader[sector]--;
    if (num_Reader[sector] == 0){
        sector_mutex[sector]->V();
    }
    num_mutex->V();
}


void
WriteBegin(int sector){
    sector_mutex[sector]->P();
}

void
WriteEnd(int sector){
    sector_mutex[sector]->V();
}


#ifdef FILESYS_NEEDED
FileSystem  *fileSystem;
#endif

#ifdef FILESYS
SynchDisk   *synchDisk;
#endif

#ifdef USER_PROGRAM	// requires either FILESYS or FILESYS_STUB
Machine *machine;	// user program memory and registers
#endif

#ifdef NETWORK
PostOffice *postOffice;
#endif


// External definition, to allow us to take a pointer to this function
extern void Cleanup();

//CHANGE
int thread_PID[MAXTHREAD];        
List* existThread = new List;
char* s[] = {"JUST_CREATED","RUNNING","READY","BLOCKED"};
//CHANGE allocate a usable pid
int 
AllocatePid(){
    int n = existThread->NumInList();
    ASSERT(n<=128);
    for(int i=0;i<MAXTHREAD;i++){
        if(thread_PID[i]==0){
            thread_PID[i]=1;
            return i;
        } 
    } 
}
//CHANGE recycle a pid
void RecyclePid(int pid_num){
    ASSERT( pid_num>=0 && pid_num<MAXTHREAD );
    thread_PID[pid_num]=0;
}
//CHANGE  display status of all threads
void TS(){
    printf("==================CALL========TS====================\n");
    ListElement* head = existThread->getFirst();
    for (;head!=NULL;head=head->next) {
        Thread* tmp=(Thread*)head->item;
        printf("threadname %s  threadid = %d  threadstatus %s  priority  %d\n",tmp->getName(),tmp->getId(),s[tmp->getStatus()],tmp->getPriority());
    }
    printf("==================TS========END====================\n");
}
//----------------------------------------------------------------------
// TimerInterruptHandler
// 	Interrupt handler for the timer device.  The timer device is
//	set up to interrupt the CPU periodically (once every TimerTicks).
//	This routine is called each time there is a timer interrupt,
//	with interrupts disabled.
//
//	Note that instead of calling Yield() directly (which would
//	suspend the interrupt handler, not the interrupted thread
//	which is what we wanted to context switch), we set a flag
//	so that once the interrupt handler is done, it will appear as 
//	if the interrupted thread called Yield at the point it is 
//	was interrupted.
//
//	"dummy" is because every interrupt handler takes one argument,
//		whether it needs it or not.
//----------------------------------------------------------------------

static void
TimerInterruptHandler(int dummy)
{
    if (interrupt->getStatus() != IdleMode)
	interrupt->YieldOnReturn();
}

// static void 
// TimerTickFunc(int which){
//     printf("==============进入时钟中断处理程序======================\n");
//     printf("totalticks  %d  userticks  %d  systemticks  %d  idleticks  %d  \n",
//            stats->totalTicks,stats->userTicks,stats->systemTicks,stats->idleTicks);
//     Thread* tmp =scheduler->FindNext() ;
//     if (tmp != NULL) { 
//         if (interrupt->getStatus() != IdleMode)
// 	    interrupt->YieldOnReturn(); 
//     }
//     printf("==============退出时钟中断处理程序======================\n");
// }
//----------------------------------------------------------------------
// Initialize
// 	Initialize Nachos global data structures.  Interpret command
//	line arguments in order to determine flags for the initialization.  
// 
//	"argc" is the number of command line arguments (including the name
//		of the command) -- ex: "nachos -d +" -> argc = 3 
//	"argv" is an array of strings, one for each command line argument
//		ex: "nachos -d +" -> argv = {"nachos", "-d", "+"}
//----------------------------------------------------------------------
void
Initialize(int argc, char **argv)
{
    int argCount;
    char* debugArgs = "";
    bool randomYield = FALSE;

#ifdef USER_PROGRAM
    bool debugUserProg = FALSE;	// single step user program
#endif
#ifdef FILESYS_NEEDED
    bool format = FALSE;	// format disk
#endif
#ifdef NETWORK
    double rely = 1;		// network reliability
    int netname = 0;		// UNIX socket name
#endif
    
    for (argc--, argv++; argc > 0; argc -= argCount, argv += argCount) {
	argCount = 1;
	if (!strcmp(*argv, "-d")) {
	    if (argc == 1)
		debugArgs = "+";	// turn on all debug flags
	    else {
	    	debugArgs = *(argv + 1);
	    	argCount = 2;
	    }
	} else if (!strcmp(*argv, "-rs")) {
	    ASSERT(argc > 1);
	    RandomInit(atoi(*(argv + 1)));	// initialize pseudo-random
						// number generator
	    randomYield = TRUE;
	    argCount = 2;
	}
#ifdef USER_PROGRAM
	if (!strcmp(*argv, "-s"))
	    debugUserProg = TRUE;
#endif
#ifdef FILESYS_NEEDED
	if (!strcmp(*argv, "-f"))
	    format = TRUE;
#endif
#ifdef NETWORK
	if (!strcmp(*argv, "-l")) {
	    ASSERT(argc > 1);
	    rely = atof(*(argv + 1));
	    argCount = 2;
	} else if (!strcmp(*argv, "-m")) {
	    ASSERT(argc > 1);
	    netname = atoi(*(argv + 1));
	    argCount = 2;
	}
#endif
    }

    DebugInit(debugArgs);			// initialize DEBUG messages
    stats = new Statistics();			// collect statistics
    interrupt = new Interrupt;			// start up interrupt handling
    scheduler = new Scheduler();		// initialize the ready queue
    if (randomYield)				// start the timer (if needed)
	timer = new Timer(TimerInterruptHandler, 0, true/*randomYield*/);

    // timer = new Timer(TimerTickFunc,0, false);

    threadToBeDestroyed = NULL;
    memset(thread_PID,0,MAXTHREAD);    //CHANGE:初始化PID数组

    // We didn't explicitly allocate the current thread we are running in.
    // But if it ever tries to give up the CPU, we better have a Thread
    // object to save its state. 
    currentThread = new Thread("main");		//CHANGE
    currentThread->setStatus(RUNNING);
    existThread->Append((void*)currentThread);  // CHANGE

    interrupt->Enable();
    CallOnUserAbort(Cleanup);			// if user hits ctl-C
    
#ifdef USER_PROGRAM
    machine = new Machine(debugUserProg);	// this must come first
#endif

#ifdef FILESYS
    synchDisk = new SynchDisk("DISK");
#endif

#ifdef FILESYS_NEEDED
    fileSystem = new FileSystem(format);
#endif

#ifdef NETWORK
    postOffice = new PostOffice(netname, rely, 10);
#endif
}

//----------------------------------------------------------------------
// Cleanup
// 	Nachos is halting.  De-allocate global data structures.
//----------------------------------------------------------------------
void
Cleanup()
{
    printf("\nCleaning up...\n");
#ifdef NETWORK
    delete postOffice;
#endif
    
#ifdef USER_PROGRAM
    delete machine;
#endif

#ifdef FILESYS_NEEDED
    delete fileSystem;
#endif

#ifdef FILESYS
    delete synchDisk;
#endif
    
    delete timer;
    delete scheduler;
    delete interrupt;
    
    Exit(0);
}

