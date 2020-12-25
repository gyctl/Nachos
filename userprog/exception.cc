// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"

#include "translate.h"    //CHANGE




//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void Exec_func(char *name){
    OpenFile *executable = fileSystem->Open(name,"");    //打开名为name的文件，并记录文件指针
    AddrSpace *space = new AddrSpace(executable);             //为该文件申请地址空间
    currentThread->space = space;                           //将此地址空间赋值给进程的地址空间
    space->InitRegisters();  //初始化
    space->RestoreState();   //用户页表载入machine页表,此功能在实现缺页中断的时候已被关闭，因为不需要将用户页表
                            //装载到机器页表，缺页的时候重新调页即可
    machine->Run();         //运行
}

void Fork_func(int space){                        //此函数的目的是复制父进程的地址空间，然后在此地址空间上执行子进程程序                  
    currentThread->space = (AddrSpace *) space;  //子进程复制父进程的地址空间
    machine->WriteRegister(PCReg,currentThread->space->PC);  //更改当前machine的pc指针，指向子进程的程序
    machine->WriteRegister(NextPCReg,currentThread->space->PC+sizeof(int)); //更改下一程序的指针指向子进程程序的下一条指令
    machine->Run();  //运行
}


void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
	    DEBUG('a', "Shutdown, initiated by user program.\n");
        printf("=====LRU====\n");
        // printf("=====FIFO====\n");
        printf("tlb_hit %.0f  tlb_miss  %.0f  tlb_miss_rate  %.4f\n",
            machine->tlb_hit,machine->tlb_miss,machine->tlb_miss/(machine->tlb_miss+machine->tlb_hit));
        // printf("========程序运行结束=============\n");
        machine->RecyclePage();
        
   	    interrupt->Halt();
        
    } else if (which == PageFaultException) {
        // printf("===========缺页中断发生=========\n");
        int vpn = (unsigned) machine->registers[BadVAddrReg]/PageSize ; //缺页中断发生的虚拟地址，也就是此页未找到对应的物理页面？
        int pageNum = machine->AllocatePage();            //判断内存中是否还有空间
        OpenFile *file = fileSystem->Open("diskfile","");
        // printf("========pagenNum%d======\n",pageNum);
        if(pageNum != -1){
            // printf("===========有空闲物理页面=========\n");
            file->ReadAt(&(machine->mainMemory[pageNum*PageSize]),PageSize,vpn*PageSize);
            //此处使用vpn*PageSize而非直接使用machine->registers[BadVAddrReg]，是因为需要页面的首地址
            machine->pageTableForAll[pageNum].virtualPage = vpn;
            machine->pageTableForAll[pageNum].pid = currentThread->getId();
            machine->pageTableForAll[pageNum].valid = true;
            machine->pageTableForAll[pageNum].use = false;
            machine->pageTableForAll[pageNum].dirty = false;
            machine->pageTableForAll[pageNum].readOnly = false;
            machine->pageTableForAll[pageNum].lru = 16;
            // printf("*****====allocatevpn=%d====\n",vpn);
        } else {                                          //当内存中没有空闲页面时，采用lru
                                                           //置换算法将页面置换出内存，类似tlb
            // printf("===========无空闲物理页面lru算法启动=========\n");
            int j=0;
            for (int i=0;i<NumPhysPages;i++){
                if (machine->pageTableForAll[i].lru < machine->pageTableForAll[j].lru){
                    j = i; 
                }
            }
            if (machine->pageTableForAll[i].dirty == true){                  //若文件中间被修改过，则将其写回到磁盘中
                // printf("========写回磁盘===============\n");
                int vpn_tmp = machine->pageTableForAll[i].virtualPage;
                file->WriteAt(&(machine->mainMemory[j*PageSize]),PageSize,vpn_tmp*PageSize);
            }
            file->ReadAt(&(machine->mainMemory[j*PageSize]),PageSize,vpn*PageSize);
            machine->pageTableForAll[j].virtualPage = vpn;
            machine->pageTableForAll[j].pid = currentThread->getId();
            machine->pageTableForAll[j].valid = true;
            machine->pageTableForAll[j].use = false;
            machine->pageTableForAll[j].dirty = false;
            machine->pageTableForAll[j].readOnly = false;
            machine->pageTableForAll[j].lru = 16;
            // printf("======allocatevpn=%d====\n",vpn);
        }

/*        if (machine->pageTable!=NULL){
            // printf("====pagetable=====pageFault=============\n");
            int pos = -1;
            for (int i=0;i<TLBSize;i++){
                if (machine->tlb[i].valid==false){
                    pos = i;
                    break;
                }
            }
            if (pos==-1) pos++;
            machine->tlb[pos].valid = true;
            machine->tlb[pos].virtualPage = vpn;
            machine->tlb[pos].physicalPage = machine->pageTable[vpn].physicalPage;  
            machine->tlb[pos].use = false;
            machine->tlb[pos].dirty = false;
            machine->tlb[pos].readOnly = false;
        }
        // =================LRU=====================
        if (machine->tlb!=NULL){
            int pos = 0;
            for (int i = 0 ; i < TLBSize ; i++){
                if (machine->tlb[i].valid == false){
                    pos = i;
                    break;
                }
                if (machine->tlb[i].lru<machine->tlb[pos].lru) pos=i;
            }
            // TranslationEntry tmp = machine->tlb[pos];    WRONG!!!!!!!!!!
            machine->tlb[pos].valid = true;
            machine->tlb[pos].virtualPage = vpn;
            machine->tlb[pos].physicalPage = machine->pageTableForTlb[vpn].physicalPage;  
            machine->tlb[pos].use = false;
            machine->tlb[pos].dirty = false;
            machine->tlb[pos].readOnly = false;
            machine->tlb[pos].lru = 16;
            //=================FIFO=====================
            // int pos = -1;
            // for (int i = 0 ;i < TLBSize ; i++){
            //     if (machine->tlb[i].valid == false){
            //         pos = i;
            //         break;
            //     }
            // } 
            // if (pos == -1){
            //     for (pos=0;pos<TLBSize-1;pos++){
            //         machine->tlb[pos]=machine->tlb[pos+1];
            //     }
            // }
            // machine->tlb[pos].valid = true;
            // machine->tlb[pos].virtualPage = vpn;
            // machine->tlb[pos].physicalPage =machine->pageTableForTlb[vpn].physicalPage;  
            // machine->tlb[pos].use = false;
            // machine->tlb[pos].dirty = false;
            // machine->tlb[pos].readOnly = false;
        }
*/    
    } else if ((which == SyscallException) && (type == SC_Create)){
        printf("SC_Create\n");
        int name = machine->ReadRegister(4);    //此参数是一个数组地址，因此是整型数据
        int tmp = name;
        int value = 1;
        int count = 0;
        while (value != 0){                     //判断数组的长度
            machine->ReadMem(tmp++,1,&value);   //因为是char型数组，因此每次加1即可
            count++;
        }
        // printf("文件名的长度为%d\n",count);
        char fileName[count];                   //存放文件名
        for (int i = 0;i < count; i++ ){
            machine->ReadMem(name++,1,(int *)&fileName[i]);   //将文件名写入filename
        }
        
        fileSystem->Create(fileName,256,"");    //创建文件，大小为256字节
        printf("已创建文件名为%s的文件\n",fileName);

        machine->PCAdvance();     //PC前进

    } else if ((which == SyscallException) && (type == SC_Open)){
        printf("SC_Open\n");
        int name = machine->ReadRegister(4);     
        int tmp = name;
        int value = 1;
        int count = 0;
        while (value != 0){
            machine->ReadMem(tmp++,1,&value);
            count++;
        }
        char fileName[count];
        for (int i = 0;i < count; i++ ){
            machine->ReadMem(name++,1,(int *)&fileName[i]);
        }                                                            //同上等到文件名
        OpenFile *openfile = fileSystem->Open(fileName,"");             //打开文件
        if (openfile == NULL){
            printf("file *%s* is not exit\n",fileName);
        } else {
            machine->WriteRegister(2,(int)openfile);                 //将文件指针放入二号寄存器
            printf("openfile *%s* pointer %d open\n",fileName,(int)openfile);
        }
        // printf("r2 %d\n",machine->ReadRegister(2));
        machine->PCAdvance();                                        //PC前进
        return (OpenFileId)openfile;                                 //返回文件指针

    } else if ((which == SyscallException) && (type == SC_Close)){
        // printf("r2 %d\n",machine->ReadRegister(2));
        printf("SC_Close\n");
        OpenFileId file = machine->ReadRegister(4);          //传递的参数按顺序分别存放在4，5，6，7寄存器
        // printf("openfile pointer %d will be closed\n",base);
        OpenFile *openfile = (OpenFile *) file;              
        if (openfile != NULL){
            delete openfile;
            printf("openfile pointer %d close\n",file);
        } else {
            printf("文件不存在\n");
        }
        machine->PCAdvance();
    } else if ((which == SyscallException) && (type == SC_Write)){
        printf("SC_Write\n");
        int buffer = machine->ReadRegister(4);                         //中转数组
        int size = machine->ReadRegister(5);                           //准备写的字节数
        OpenFileId file = machine->ReadRegister(6);                    //准备写的目标文件
        OpenFile *openfile = (OpenFile *) file;
        if (openfile == NULL){
            printf("文件不存在\n");
        } else {
            printf("从主存 %d 读取 %d 字节到文件 %d\n",buffer,size,file);
            char content[size];
            for (int i=0;i<size;i++){
                machine->ReadMem(buffer+i,1,(int)&content[i]);        //将中转数组的内容提取
            }
            openfile->Write(content,size);                            //写入目标文件
        }
        machine->PCAdvance();
    } else if ((which == SyscallException) && (type == SC_Read)){
        printf("SC_Read\n"); 
        int buffer = machine->ReadRegister(4);                 //中转数组
        int size = machine->ReadRegister(5);                   //准备读取的字节数
        OpenFileId file = machine->ReadRegister(6);            //准备读取的源文件
        OpenFile *openfile = (OpenFile *)file;                 //源文件的文件指针
        if (openfile == NULL){
            printf("文件不存在\n");
        } else {
            printf("从文件 %d 处读取 %d 字节的数据到主存 %d\n",file,size,buffer);
            char content[size+1];                              //暂存读取的数据
            content[size] = '\0';            //防止访问越界
            openfile->Read(content,size);    //从文件openfile中读取size大小的字节到content中
            printf("content %s\n",content);  
            for (int i=0;i<size;i++){
                machine->WriteMem(buffer+i,1,(int)content[i]);     //将content中的内容写入到buffer中
            }
        }
        machine->PCAdvance();                 //PC前进
    } else if ((which == SyscallException) && (type == SC_Exec)){   //功能是执行一个用户程序
        printf("SC_Exec\n");
        int name = machine->ReadRegister(4);    //此参数是一个数组地址，因此是整型数据，表示文件的地址
        int tmp = name;
        int value = 1;
        int count = 0;
        int i;
        while (value != 0){                     //判断数组的长度
            machine->ReadMem(tmp++,1,&value);   //因为是char型数组，因此每次加1即可
            count++;
        }
        printf("文件名的长度为%d\n",count);
        char fileName[count+1];                   //存放文件名
        for (i = 0;i < count; i++ ){
            machine->ReadMem(name++,1,(int *)&fileName[i]);   //将文件名写入filename
        }
        fileName[count] = '\0';                        //界定数组边界
        Thread *thread = new Thread("second thread",1);
        for (i=0;i<MAXCHILD;i++){
            if (currentThread->childThread[i] == NULL){
                currentThread->childThread[i] = thread;
                thread->fatherThread = currentThread;
                // printf("进程 %s 的子进程 %s\n",currentThread->getName(),currentThread->childThread[i]->getName());
                // printf("进程 %s 的父进程 %s\n",thread->getName(),thread->fatherThread->getName());
                machine->WriteRegister(2,(int)thread);
                break;
            }
        }
        if (i==MAXCHILD){
            printf("Exec failed\n");
            machine->PCAdvance();
            return -1;
        }    
        thread->Fork(Exec_func,&fileName);
        printf("fork finish currentthread %s\n",currentThread->getName());
        machine->PCAdvance();
        return (SpaceId)thread;
    } else if ((which == SyscallException) && (type == SC_Join)){
        printf("SC_Join\n");
        // printf("当前线程%s\n",currentThread->getName());
        Thread *child =(Thread *)machine->ReadRegister(4);   //给定的子线程
        // printf("参数线程%s\n",child->getName());
        int i;
        for (i=0;i<MAXCHILD;i++){
            if (currentThread->childThread[i] == child){     //判断此线程是否是当前线程的子线程
                break;
            }
        } 
        if (i != MAXCHILD){                      //是当前线程的子线程
            printf("Join failed\n");             //等待结束
            while (currentThread->childThread[i] != NULL){    //直到子线程结束
                currentThread->Yield();                        
            }
            // machine->PCAdvance();
        }
        printf("join succeed\n");
        machine->PCAdvance();
        return 0;
    } else if ((which == SyscallException) && (type == SC_Exit)){
        // printf("==========程序运行结束==========\n");
        printf("SC_Exit\n");
        printf("线程 %s 结束,释放物理页面\n",currentThread->getName());
        if (currentThread->space != NULL){
            machine->RecyclePage();
        }  
        if(currentThread->fatherThread != currentThread){    //说明当前线程是子线程   
            for (int i = 0;i < MAXCHILD;i++){
                if (currentThread->fatherThread->childThread[i] == currentThread){
                    // printf("当前线程 %s 是子线程 父线程为 %s\n",currentThread->getName(),currentThread->fatherThread->getName());
                    currentThread->fatherThread->childThread[i] = NULL;
                    currentThread->fatherThread = currentThread;
                    break;
                }
            }  
        } 
        currentThread->Finish();
        machine->PCAdvance();
        //else machine->PCAdvance();
           
    /*    
        if (currentThread->space != NULL) {
            machine->RecyclePage();          
            int next_pc = machine->ReadRegister(NextPCReg);
            machine->WriteRegister(PCReg,next_pc);
            currentThread->Finish() ;
        }
    */        
    } else if ((which == SyscallException) && (type == SC_Yield)){
        
        printf("SC_Yield\n");
        machine->PCAdvance();
        currentThread->Yield();

    } else if ((which == SyscallException) && (type == SC_Fork)){
       
        printf("SC_Fork\n");
        int pc_now = machine->ReadRegister(4);           //将要执行的程序的指令的地址
        Thread *thread = new Thread("second thread",1);  //创建子进程
        int i;
        for (i=0;i<MAXCHILD;i++){                        //将父子进程相互连接
            if (currentThread->childThread[i] == NULL){
                currentThread->childThread[i] = thread;
                thread->fatherThread = currentThread;
                machine->WriteRegister(2,(int)thread);
                break;
            }
        }
        if (i == MAXCHILD){                              //父进程的子进程已满
            printf("Fork failed\n");
            machine->PCAdvance();
            return ;
        }
        currentThread->space->PC = pc_now;           //AddrSpace 中新增记录当前进程PC值的变量
        thread->Fork(Fork_func,(int)currentThread->space);
        machine->PCAdvance();

    } else {
        printf("Unexpected user mode exception %d %d\n", which, type);
	    ASSERT(FALSE);
    }
}
