// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{ 
    /*numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
	return FALSE;		// not enough space

    for (int i = 0; i < numSectors; i++)
	dataSectors[i] = freeMap->Find();
    return TRUE;*/

    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
	    return FALSE;		// not enough space

    int leftSector = numSectors - NumDirect;   
    if (numSectors <= NumDirect) {          //先把直接地址分配完毕
        for (int i = 0;i < numSectors;i++){
            dataSectors[i] = freeMap->Find();
        }
    } else {
        for (int i = 0;i < NumDirect;i++){
            dataSectors[i] = freeMap->Find();
        }
    }
    if (leftSector > 0){                   //若直接地址不够用,则使用二级索引
        int num = divRoundUp(leftSector,NumIndex);  //需要num个二级索引
        for (int i =0;i<num;i++){ 
            dataSectors[NumDirect+i] = freeMap->Find();  //为每个二级索引分配一个扇区
            int Indexnum;                               //记录此次循环需要分配多少个扇区
            if (i == num-1) Indexnum = leftSector;      
            else Indexnum = NumIndex;
            int* index = new int[Indexnum];
            for (int j = 0 ;j<Indexnum;j++){
                index[i] = freeMap->Find();
            } 
            leftSector -=NumIndex;
            synchDisk->WriteSector(dataSectors[NumDirect + i],(char*) index);
            delete index;
        }
    }   
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void 
FileHeader::Deallocate(BitMap *freeMap)
{
    /*for (int i = 0; i < numSectors; i++) {
	ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	freeMap->Clear((int) dataSectors[i]);
    } */

    int leftSectors = numSectors - NumDirect;
    if (leftSectors <= 0){
        for (int i = 0; i < numSectors; i++) {
	        ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	        freeMap->Clear((int) dataSectors[i]);
        }
        return ;
    } else {
        for (int i = 0;i<NumDirect;i++){
            ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
	        freeMap->Clear((int) dataSectors[i]);
        }
        int num = divRoundUp(leftSectors,NumIndex);
        for (int j = 0;j<num;j++){
            int Indexnum ;
            if (j == num-1) Indexnum = leftSectors;
            else Indexnum = NumIndex;
            char* temp = new char[Indexnum];
            synchDisk->ReadSector(dataSectors[NumDirect+j],temp);
            for (int i = 0;i<Indexnum;i++){
                ASSERT(freeMap->Test((int)temp[i]));  // ought to be marked!
	            freeMap->Clear((int)temp[i]);
            }
            ASSERT(freeMap->Test((int) dataSectors[NumDirect +j]));  // ought to be marked!
	        freeMap->Clear((int) dataSectors[NumDirect + j]);
            leftSectors -= NumIndex;
        }
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    //return(dataSectors[offset / SectorSize]);

    int cur_sector = offset / SectorSize;      //所在扇区数
    if (cur_sector < NumDirect){               //通过直接索引就可找到扇区
        return (dataSectors[cur_sector]);
    } else {
        cur_sector -= NumDirect;              //去掉直接索引可找到的扇区
        int temp =divRoundUp(cur_sector,NumIndex);   //找到所在的二级索引位置
        char* ch = new char[SectorSize];             //记录二级索引对应的扇区中的记录
        synchDisk->ReadSector(dataSectors[NumDirect+temp-1],ch);  //将该扇区中的所有记录读入到ch中
        offset = (offset - NumDirect) % NumIndex;                        //找到扇区内的偏移
        temp = (int)ch[offset-1];
        delete ch;
        return temp;
    }
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (i = 0; i < numSectors; i++)
	printf("%d ", dataSectors[i]);
    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors; i++) {
	synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n"); 
    }
    delete [] data;
}
