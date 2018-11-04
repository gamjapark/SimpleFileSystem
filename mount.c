#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fs.h"
#include "disk.h"


FileSysInfo* pFileSysInfo = NULL;


void		Mount(MountType type)
{

	if(type == MT_TYPE_FORMAT){

		FileSysInit();

		for(int i = 0; i < 19; i++) SetBlockBitmap(i);
		
		int freeblock = GetFreeBlockNum(), freeinode = GetFreeInodeNum();
		char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));

		DirEntry *direntry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)pblock, (DirEntry*)(pblock + 16), (DirEntry*)(pblock + 32), (DirEntry*)(pblock + 48) };

		strcpy(direntry[0]->name , ".");
		direntry[0]->inodeNum = freeinode;
		strcpy(direntry[1]->name , "");
		direntry[1]->inodeNum = -1;
		strcpy(direntry[2]->name , "");
		direntry[2]->inodeNum = -1;
		strcpy(direntry[3]->name , "");
		direntry[3]->inodeNum = -1;

		DevWriteBlock(freeblock, pblock);


		pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
		pFileSysInfo = (FileSysInfo*)pblock;

		pFileSysInfo->blocks = 512;
		pFileSysInfo->rootInodeNum = 0;
		pFileSysInfo->diskCapacity = FS_DISK_CAPACITY;
		pFileSysInfo->numAllocBlocks = 0;
		pFileSysInfo->numFreeBlocks = 512 - 19;
		pFileSysInfo->numAllocInodes = 0;
		pFileSysInfo->blockBitmapBlock = BLOCK_BITMAP_BLOCK_NUM;
		pFileSysInfo->inodeBitmapBlock = INODE_BITMAP_BLOCK_NUM;
		pFileSysInfo->inodeListBlock = INODELIST_BLOCK_FIRST;
		pFileSysInfo->dataRegionBlock = 19;

		pFileSysInfo->numAllocBlocks++;
		pFileSysInfo->numFreeBlocks--;
		pFileSysInfo->numAllocInodes++;


		DevWriteBlock(FILESYS_INFO_BLOCK, pblock);


		SetBlockBitmap(freeblock);
		SetInodeBitmap(freeinode);

		Inode *pInode = (Inode *)malloc(sizeof(Inode));
		GetInode(freeinode, pInode);

		pInode->size = 64;
		pInode->type = FILE_TYPE_DIR;
		pInode->dirBlockPtr[0] = freeblock;
		pInode->dirBlockPtr[1] = -1;
		pInode->indirBlockPtr = -1;
		PutInode(freeinode, pInode);

	}else if(type == MT_TYPE_READWRITE){
		DevOpenDisk();

		pFileDescTable = (FileDescTable *)malloc(sizeof(FileDescTable));

		for( int i = 0; i < MAX_FD_ENTRY_LEN; i++){
			pFileDescTable->file[i].bUsed = 0;
			pFileDescTable->file[i].fileOffset = 0;
			pFileDescTable->file[i].inodeNum = -1;
		}
	}
}
void		Unmount(void)
{
	DevCloseDisk();
}

