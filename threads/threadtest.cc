// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "elevatortest.h"
#include "synch.h"

// testnum is set in main.cc
int testnum = 5;

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    for (num = 0; num < 10; num++) {
        // TS();  //test
        if (currentThread->getPriority()>3&&num==3){

            char* s[] = {"forked thread 1","forked thread 2"};
            Thread* m = new Thread(s[currentThread->getPriority()-4],currentThread->getPriority()-1);
            m->Fork(SimpleThread, (void*)m->getId());
        }

	    // printf("**** threadid %s looped %d times\n",currentThread->getName(),num);
        currentThread->Yield();
    }
}
//===========================================================================
//锁功能测试，有锁的情况下，进程不会被高优先级进程抢占
Lock* lock = new Lock("don't interrupt");

void 
SynchTest(int which){
    lock->Acquire();
    int p  = currentThread->getPriority();
    if (p > 0){
        // char c = 'a';
        // char* s[] ;
        // s[0] = c;
        printf("pid %d  priority %d  start\n",
            currentThread->getId(),currentThread->getPriority());
        Thread* tmp = new Thread("www",p-1);
        tmp->Fork(SynchTest,1);
    }
    lock->Release();
}

//============================================================================
//信号量机制实现生产者消费者问题
void produce(){
    printf("生产者生产一个产品\n");
}

Semaphore* mutex = new Semaphore("mutex",1);
Semaphore* full = new Semaphore("full",0);
Semaphore* empty = new Semaphore("empty",5);

void Producer(int num){
    printf("生产者进程开始\n");
    for (int i=0;i<num;i++){
        produce();
        empty->P();
        mutex->P();
        printf("生产者将产品放入缓冲区\n");
        mutex->V();
        full->V();
    }
    printf("生产者进程结束\n");
    
}

void Consumer(int num){
    printf("消费者进程开始\n");
    for (int i=0;i<num;i++){
        full->P();
        mutex->P();
        printf("消费者消费一个产品\n");
        mutex->V();
        empty->V();
    }
    printf("消费者进程结束\n");
}

//======================================================================
//条件变量实现生产者消费者问题
Condition* c_full = new Condition("full");
Condition* c_empty = new Condition("empty");
Lock* c_lock = new Lock("don't interrupt");

int count = 0; 
const int N = 5;
//wait操作相当于阻塞，当缓冲区满了想要继续放或者空了想要继续拿都会被阻塞
//signal操作相当于唤醒，当缓冲区出现第一个产品或者第一个空位，都会尝试唤醒可能存在的被阻塞的进程。
//进入wait函数的时候，因为此时的锁是被当前线程所持有，因此需要先释放锁，然后执行线程的切换，并把自己放入到等待队列等待唤醒。被
//唤醒以后，先获得锁，然后再继续执行下面的操作。
//进入signal函数的时候，此时的锁被当前线程所持有，检查等待队列是否存在等待的线程，若存在则将其唤醒。否则不执行其他操作。
void 
c_Producer(int num){
    printf("生产者进程开始\n");
    c_lock->Acquire();   //获得锁    
    for (int i = 0;i < num;i++){
        printf("生产者生产一个产品\n");    
        while (count == N){
            printf("缓冲区已满\n");
            c_full->Wait(c_lock);     
        }
        printf("生产者将产品放入缓冲区\n");
        count++;
        if (count == 1) c_empty->Signal(c_lock);             
    }
    c_lock->Release();   //此位置的释放锁不一定是函数开头加的锁，可能是在wait函数中因被唤醒而重新加的锁
    printf("生产者进程结束\n");
}

void
c_Consumer(int num){
    printf("消费者进程开始\n");
    c_lock->Acquire();   //获得锁
    for (int i = 0;i < num;i++){
        printf("消费者想要消费一个产品\n");
        while (count == 0){
            printf("缓冲区为空\n");
            c_empty->Wait(c_lock);  
        } 
        printf("消费者从缓冲区拿走一个产品\n");
        count--;
        if (count == N-1) c_full->Signal(c_lock);
    }
    c_lock->Release();
    printf("消费者进程结束\n");
}
//=====================================================================
//实现reader/writer lock
Lock* m = new Lock("m");
Lock* visit = new Lock("visit");
int reader_Count = 0;
int tmp = 1;

void 
reader(int which){
    m->Acquire();        //保护reader_Count
    reader_Count++;              //读者数加1 
    if (reader_Count == 1){          //看是否有读者正在读，若无说明是第一个读者
        visit->Acquire();    //  看是否有写者正在写 
    }   
    m->Release();
    printf("reader_Count=%d\n",reader_Count);
    printf("读者%s进入\n",currentThread->getName());
    while (tmp > 0){
        tmp--;
        Thread* a = new Thread("reader2",0);
        a->Fork(reader,1);  
    }
    printf("读者%s读完准备离开\n",currentThread->getName());
    m->Acquire();
    reader_Count--;
    m->Release();
    if (reader_Count == 0){
        printf("release\n");
        visit->Release();    //若是最后一个读者，则释放控制权
    } 
    printf("读者%s离开\n",currentThread->getName());
}

void
writer(int which){
    visit->Acquire();
    printf("写者%s进入准备写\n",currentThread->getName());
    printf("写者%s写完准备离开\n",currentThread->getName());
    visit->Release();
}


//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    // Thread *t = new Thread("forked thread t",AllocatePid(),7);
    // Thread *a = new Thread("forked thread a",AllocatePid(),4);
    // Thread *b = new Thread("forked thread b",AllocatePid(),6);
    // Thread *c = new Thread("forked thread c",AllocatePid(),2);
    // Thread *d = new Thread("forked thread d",AllocatePid(),1);
    // Thread *e = new Thread("forked thread e",AllocatePid(),0);

    // t->Fork(SimpleThread, (void*)t->getId());
    // a->Fork(SimpleThread, (void*)a->getId());
    // b->Fork(SimpleThread, (void*)b->getId());
    // c->Fork(SimpleThread, (void*)c->getId());
    // d->Fork(SimpleThread, (void*)c->getId());
    // e->Fork(SimpleThread, (void*)c->getId());

    SimpleThread(currentThread->getId());
}

/*
ThreadTest2():测试同步互斥机制的测试,testnum = 2;
*/

void
ThreadTest2(){
    DEBUG('t', "Entering ThreadTest2");
    Thread* t1 = new Thread("Thread1",4);
    t1->Fork(SynchTest,1);
}
//============================================================
//testnum = 3
//测试信号量实现生产者消费者问题
void
semaphTest3(){
    Thread* a = new Thread("Producer");
    Thread* b = new Thread("Consumer");

    a->Fork(Producer,8);
    b->Fork(Consumer,9);
}

//===========================================================
//测试条件变量实现的生产者消费者问题  testnum = 4;
void 
semaphTest4(){
    Thread* a = new Thread("produce1");
    Thread* b = new Thread("consumer1");
    Thread* c = new Thread("produce2");
    Thread* d = new Thread("produce2");

    a->Fork(c_Producer,7);
    b->Fork(c_Consumer,7);
    c->Fork(c_Producer,6);
    d->Fork(c_Consumer,6);

}
//=============================================================
//测试读写锁  testnum=5
void
semaphTest5(){
    Thread* a = new Thread("reader1");
     Thread* b = new Thread("write2");
    Thread* c = new Thread("writer1");

    a->Fork(reader,1);
     b->Fork(writer,1);
    c->Fork(writer,1);
    

}

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    // Timer* timer = new Timer(TimerTickFunc,1,false);
    switch (testnum) {
    case 1:
	ThreadTest1();
    break;
    case 2:
    ThreadTest2();
	break;
    case 3:
    semaphTest3();
    break;
    case 4:
    semaphTest4();
    break;
    case 5:
    semaphTest5();
    break;
    default:
	printf("No test specified.\n");
	break;
    }
}

