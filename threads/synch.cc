// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(char* debugName, int initialValue)   //定义信号量以及信号量的PV操作
{
    name = debugName;
    value = initialValue;
    queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    
    while (value == 0) { 			// semaphore not available
	queue->Append((void *)currentThread);	// so go to sleep
	currentThread->Sleep();  //此处使用 while 是因为当此线程被唤醒的时候还是要检查下资源是否可用，若不可用
                              //则继续自我阻塞
    } 
    value--; 					// semaphore available, 
						// consume its value
    
    (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
                                        //此处的操作不一定是由此函数完成，可能是其他的新创建的进程或者调用yield
                                        //之后又被换上cpu的进程
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = (Thread *)queue->Remove();
    if (thread != NULL)	   // make thread ready, consuming the V immediately
	scheduler->ReadyToRun(thread);
    value++;
    (void) interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
Lock::Lock(char* debugName) {                
    name = debugName;
    owner = NULL;             //因为只有上锁的线程可以解锁，因此上锁的线程需要记录下
    lock = new Semaphore(debugName,1);   //创建一个名为lock的信号量对象
}

Lock::~Lock() {
    delete lock;
}

bool Lock::isHeldByCurrentThread(){
    return currentThread == owner;
}

void Lock::Acquire() {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);  //因为P操作与owner赋值操作必须顺序完成
                                                    //不可中断，因此需要将其包装成原子操作。若没有此
                                                    //步骤，可能在P操作完成后发生线程切换，那么下次再赋值的
                                                    //线程就不是加锁的线程了。
    lock->P();
    owner = currentThread;
    (void) interrupt->SetLevel(oldLevel);
}


void Lock::Release() {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    ASSERT(isHeldByCurrentThread());                   //判断当前线程是不是加锁的线程
    lock->V();                                         //是则执行开锁操作，此处包装成原子操作的原因与上一样
    (void) interrupt->SetLevel(oldLevel);
}



Condition::Condition(char* debugName) {
    name = debugName ;
    queue = new List;

}


Condition::~Condition() {
    delete queue;
}


void Condition::Wait(Lock* conditionLock) {      //只有到达某些边界条件时才会使用wait与single操作
                                         // 例如：缓存空间满了的时候要往里加(wait)，或者加完之后容量为1,代表之前是空的(single);
                                         //缓存空间为空的时候要减去(wait)，或者减去完之后等于N-1,表示之前是满的(single)。
    //  ASSERT(FALSE);
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    ASSERT(conditionLock->isHeldByCurrentThread());
    conditionLock->Release();
    queue->Append((void*)currentThread);
    currentThread->Sleep();      //当前线程执行到这里进入阻塞，然后被single唤醒的时候需要再次获得锁才可以执行。
                                  //谁在临界区执行谁持有锁？
    conditionLock->Acquire();
    (void) interrupt->SetLevel(oldLevel);
}


void Condition::Signal(Lock* conditionLock) {
    ASSERT(conditionLock->isHeldByCurrentThread());
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    if (!queue->IsEmpty()){                      //nachos使用的是Mesa，因此唤醒某进程后仍是先执行完当前的进程，只是将唤醒的进程放入到就绪队列
        scheduler->ReadyToRun((Thread*)queue->Remove());
    }
    (void) interrupt->SetLevel(oldLevel);
}


void Condition::Broadcast(Lock* conditionLock) { 
    ASSERT(conditionLock->isHeldByCurrentThread());
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    while (!queue->IsEmpty()){
        scheduler->ReadyToRun((Thread*)queue->Remove());
    }
    (void) interrupt->SetLevel(oldLevel);
}
