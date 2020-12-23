// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -N -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"
#ifdef HOST_SPARC
#include <strings.h>
#endif

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
	noffH->noffMagic = WordToHost(noffH->noffMagic);
	noffH->code.size = WordToHost(noffH->code.size);
	noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
	noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
	noffH->initData.size = WordToHost(noffH->initData.size);
	noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
	noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
	noffH->uninitData.size = WordToHost(noffH->uninitData.size);
	noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
	noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------

AddrSpace::AddrSpace(OpenFile *executable)
{
    NoffHeader noffH;       //定义一个程序的结构？包含代码段，初始数据段，中间数据段
    unsigned int i, size;   
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size   //程序的三个段大小之和加上用户栈大小
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);    //用户需要的页面最小数，PageSize固定等于磁盘块大小
    size = (numPages + 10) * PageSize;    //用户需要的空间准确值,以字节为单位

    // ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    DEBUG('a', "Initializing address space, num pages %d, size %d\n", 
					numPages, size);
// first, set up the translation 
    fileSystem->Create("diskfile",size);    //创建一个用户程序需要大小的文件代替磁盘，将用户程序的不同段读入
    OpenFile *file =  fileSystem->Open("diskfile");
/*将用户程序写入虚拟磁盘文件*/
    if (noffH.code.size > 0){    
        for (int i=0 ; i < noffH.code.size; i++){
            char tmp;
            executable->ReadAt(&(tmp),1,noffH.code.inFileAddr+i);  //将文件中的noffH.code.inFileAddr+i位置文件读出一字节
            file->WriteAt(&(tmp),1,noffH.code.virtualAddr+i);   //将此字节写入虚拟磁盘的noffH.code.virtualAddr+i位置
        }
    }
    if (noffH.initData.size > 0){
        for (int i = 0;i < noffH.initData.size;i++){
            char tmp;
            executable->ReadAt(&(tmp),1,noffH.initData.inFileAddr+i);
            file->WriteAt(&(tmp),1,noffH.initData.virtualAddr+i);
        }
    }
    delete file;

/*
    pageTable = new TranslationEntry[numPages];   //定义用户的页表，大小为用户需要的页面数
   
    for (i = 0; i < numPages; i++) {

	    pageTable[i].virtualPage = i;	// for now, virtual page # = phys page #
        int cur=machine->AllocatePage();
        ASSERT(cur != -1);
        printf("AllocatePage virtualPageNum %d physicalPageNum  %d\n",i,cur);
	    pageTable[i].physicalPage = cur;
	    pageTable[i].valid = false;              //lazing load
	    pageTable[i].use = FALSE;
	    pageTable[i].dirty = FALSE;
	    pageTable[i].readOnly = FALSE;  // if the code segment was entirely on 
					// a separate page, we could set its 
					// pages to be read-only
    }
*/  
// zero out the entire address space, to zero the unitialized data segment 
// and the stack segment
    // bzero(machine->mainMemory, MemorySize);
/*
为实现多线程同时位于内存中，需要将mainmarry分页  
*/

// then, copy in the code and data segments into memory
 /*
    if (noffH.code.size > 0) {   //程序的代码段
        DEBUG('a', "Initializing code segment, at 0x%x, size %d\n", 
			noffH.code.virtualAddr, noffH.code.size);
        int pos = noffH.code.inFileAddr;
        for (int i = 0; i< noffH.code.size; i++){            //将代码段的每个字节都读入到mainMemory中的某个位置，mainMemory是个
                                                              //字符型数组，每个位置存储一个字符，大小在开始已经定义好了
            int vpn_cur = (noffH.code.virtualAddr+i)/PageSize;   //代码段的虚拟首地址加上字节数，对页面大小进行取模运算，可得该字节所处的
                                                 //虚拟页号  

            int vpo_cur = (noffH.code.virtualAddr+i)%PageSize;  // 对页面大小进行取余运算，可得该字节在页面内的偏移

            int paddr = pageTable[vpn_cur].physicalPage*PageSize+vpo_cur;   //pagetable[vpn_cur].physicalPage 
                                          //代表着此虚拟页号对应的物理页框号，乘以页面大小，加上页内偏移，组成物理地址
            executable->ReadAt(&(machine->mainMemory[paddr]),1,pos++); //从代码段所在文件中取出一个
                                                                //字节放入到刚才计算出的主存的物理位置中,同时注意将文件位置指针加一
                                                                //即取下一个字节的内容                                                
        }
        
    }
    if (noffH.initData.size > 0) {    //程序的初始数据段
        DEBUG('a', "Initializing data segment, at 0x%x, size %d\n", 
			noffH.initData.virtualAddr, noffH.initData.size);
        int pos = noffH.initData.inFileAddr;
        for (int i = 0;i < noffH.initData.size ; i++){
            int vpn_cur = (noffH.initData.virtualAddr+i)/PageSize;
            int vpo_cur = (noffH.initData.virtualAddr+i)%PageSize;
            int paddr = pageTable[vpn_cur].physicalPage*PageSize+vpo_cur;
            executable->ReadAt(&(machine->mainMemory[paddr]),1,pos++);
        } 
    }
*/

}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   delete pageTable;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------

void AddrSpace::SaveState()     //此函数在用户程序切换时调用，将系统的TLB表全部置为无效位
                             //防止程序取到不属于自己的错误内容
{   
    // for (int i=0 ; i < TLBSize ; i++){
    //     machine->tlb[i].valid = false;
    // }
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState()    //装载用户页表，当tlb可用时，装载进pageTableForTlb，否则装载进pageTable中
{
    // if (machine->tlb==NULL){
    //     machine->pageTable = pageTable;
    //     machine->pageTableSize = numPages;
    // } else {
    //     machine->pageTableForTlb = pageTable;
    //     machine->pageTableForTlbSize = numPages;
    // }
    
}
