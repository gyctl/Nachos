// filehdr.h 
//	Data structures for managing a disk file header.  
//
//	A file header describes where on disk to find the data in a file,
//	along with other information about the file (for instance, its
//	length, owner, etc.)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FILEHDR_H
#define FILEHDR_H

#include "disk.h"
#include "bitmap.h"

// #define NumDirect 	((SectorSize - 2 * sizeof(int)) / sizeof(int))
// #define MaxFileSize 	(NumDirect * SectorSize) 

#define NumTotal  ((SectorSize - 2 * sizeof(int) - 96) / sizeof(int))     
#define NumDirect 	((SectorSize - ( 2 + NumSecondDirect ) * sizeof(int) - 96) / sizeof(int))    //直接索引不够用.因此留出间接索引的位置
#define MaxFileSize 	( (NumDirect +  NumSecondDirect * NumIndex)* SectorSize)
#define NumSecondDirect 2   //二级索引节点数目
#define NumIndex (SectorSize / sizeof(int) ) //每个二级索引节点可以存放的地址数

// The following class defines the Nachos "file header" (in UNIX terms,  
// the "i-node"), describing where on disk to find all of the data in the file.
// The file header is organized as a simple table of pointers to
// data blocks. 
//
// The file header data structure can be stored in memory or on disk.
// When it is on disk, it is stored in a single sector -- this means
// that we assume the size of this data structure to be the same
// as one disk sector.  Without indirect addressing, this
// limits the maximum file length to just under 4K bytes.
//
// There is no constructor; rather the file header can be initialized
// by allocating blocks for the file (if it is a new file), or by
// reading it from disk.

class FileHeader {
  public:
    bool Allocate(BitMap *bitMap, int fileSize);// Initialize a file header, 
						//  including allocating space 
						//  on disk for the file data
    void Deallocate(BitMap *bitMap);  		// De-allocate this file's 
						//  data blocks

    void FetchFrom(int sectorNumber); 	// Initialize file header from disk
    void WriteBack(int sectorNumber); 	// Write modifications to file header
					//  back to disk

    int ByteToSector(int offset);	// Convert a byte offset into the file
					// to the disk sector containing
					// the byte

    int FileLength();			// Return the length of the file 
					// in bytes
    bool Append(BitMap *freeMap,int bytes);
    void Print();			// Print the contents of the file.
    void set_Creat_time();
    void set_Visit_time();
    void set_Modified_time();

    char path[20];
    int sector_pos;

  private:
    int numBytes;			// Number of bytes in the file
    int numSectors;			// Number of data sectors in the file
    char creat_time[25];
    char visit_time[25];
    char modified_time[25];  
    int dataSectors[NumTotal];		// Disk sector numbers for each data 
					// block in the file
};

#endif // FILEHDR_H
