#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disk.h"
#include "fs.h"

FileDescTable* pFileDescTable = NULL;

int direntry_check(int num){            //디렉토리entry가 꽉 찾는지 확인하는 함수
	char *p = (char *)malloc(BLOCK_SIZE*sizeof(char));
	DevReadBlock(num, p);
	DirEntry *direntry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)p, (DirEntry*)(p + 16), (DirEntry*)(p + 32), (DirEntry*)(p + 48) };	

    for( int i = 0; i < NUM_OF_DIRENT_PER_BLOCK; i++){
        if( direntry[i]->inodeNum == -1){
            return 1;                   //하나라도 비어있음
		}
	}
    return 0;                           //비어있는게 없음
}
void alloc_dirblock(int num){           //빈 블록 할당하는 함수
	char *nb = (char *)malloc(BLOCK_SIZE*sizeof(char));		
	DirEntry *d[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)nb, (DirEntry*)(nb + 16), (DirEntry*)(nb + 32), (DirEntry*)(nb + 48) };	
	strcpy(d[0]->name , "");
	d[0]->inodeNum = -1;
	strcpy(d[1]->name , "");
	d[1]->inodeNum = -1;
	strcpy(d[2]->name , "");
	d[2]->inodeNum = -1;
	strcpy(d[3]->name , "");
	d[3]->inodeNum = -1;
	DevWriteBlock(num, nb);
	free(nb);		
}
void info_change(int n1, int n2, int n3){   //-1이면 아무것도 안함
	//인포변경
	char *nb = (char *)malloc(BLOCK_SIZE*sizeof(char));
	DevReadBlock(FILESYS_INFO_BLOCK, nb);
	FileSysInfo* pFileSysInfo = (FileSysInfo*)nb;

	if( 1 == n1 ) pFileSysInfo->numAllocBlocks++;
    else if( 0 == n1 ) pFileSysInfo->numAllocBlocks--;

    if( 1 == n2 ) pFileSysInfo->numFreeBlocks++;
    else if( 0 == n2) pFileSysInfo->numFreeBlocks--;

    if( 1 == n3 ) pFileSysInfo->numAllocInodes++;
    else if( 0 == n3) pFileSysInfo->numAllocInodes--;

	DevWriteBlock(FILESYS_INFO_BLOCK, nb);
	free(nb);
}
int find_block_num(int inodeNum){
	Inode *pInode = (Inode *)malloc(sizeof(Inode));
	GetInode(inodeNum, pInode);
	//printf("inodeNum : %d\n", inodeNum);
	//printf("0 : %d\n", pInode->dirBlockPtr[0]);
	//printf("1 : %d\n", pInode->dirBlockPtr[1]);
	if( pInode->dirBlockPtr[0] != -1 && pInode->dirBlockPtr[1] == -1 ){
        if(!(direntry_check(pInode->dirBlockPtr[0]))){  //꽉 찾을 경우
            int allocblock = GetFreeBlockNum();
			pInode->size += 64;
            pInode->dirBlockPtr[1] = allocblock;
        	PutInode(inodeNum, pInode);
            //빈 블록할당
            alloc_dirblock(allocblock);
			//비트맵 설정
			SetBlockBitmap(allocblock);
			//인포변경
            info_change(1, 0, -1);
            return pInode->dirBlockPtr[1];
        }else
            return pInode->dirBlockPtr[0];
    }
	else if( pInode->dirBlockPtr[1] != -1 && pInode->indirBlockPtr == -1){
        if(!(direntry_check(pInode->dirBlockPtr[1]))){  //꽉 찾을 경우
            int allocblock = GetFreeBlockNum();
            pInode->indirBlockPtr = allocblock;
			pInode->size += 64;
        	PutInode(inodeNum, pInode);
			//비트맵 설정
			SetBlockBitmap(allocblock);

            //direct pointer 16개 block
            //빈 블록할당
        	char *nb = (char *)malloc(BLOCK_SIZE*sizeof(char));		
        	int** bk_num = (int **)(&nb);
            (*bk_num)[0] = GetFreeBlockNum();

            for(int i = 1; i < 16; i++)
                (*bk_num)[i] = -1;
            DevWriteBlock(allocblock, nb);	
			//인포변경
            info_change(1, 0, -1);

            //directory entry block
            //빈 블록할당
            alloc_dirblock((*bk_num)[0]);
			//비트맵 설정
			SetBlockBitmap((*bk_num)[0]);
			//인포변경
            info_change(1, 0, -1);
            return (*bk_num)[0];
        }else
            return pInode->dirBlockPtr[1];
    }
	else if( pInode->indirBlockPtr != -1){        //indirBlockPtr 확인
        char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
		DevReadBlock(pInode->indirBlockPtr, pblock);
        int** bk_num = (int **)(&pblock);

        int retval, next;
        for(int i = 15; i >=0 ; i--){
            if((*bk_num)[i] != -1){
                retval = (*bk_num)[i];
                next = i + 1;
                break;
            }
        }

        //찾은 블록 direntry확인 
        if(!(direntry_check(retval))){     //꽉 찬것
            int allocblock = GetFreeBlockNum();
			pInode->size += 64;
            (*bk_num)[next] = allocblock;
            DevWriteBlock(pInode->indirBlockPtr, pblock);
            //빈 블록할당
            alloc_dirblock(allocblock);
			//비트맵 설정
			SetBlockBitmap(allocblock);
			//인포변경
            info_change(1, 0, -1);
            
            return (*bk_num)[next];
        }else               //빈 자리가 남았을 때
            return retval;
	}
}
int same_name(int num, char *dirName){
	char *p = (char *)malloc(BLOCK_SIZE*sizeof(char));
	DevReadBlock(num, p);
	int retVal = -1;
	DirEntry *direntry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)p, (DirEntry*)(p + 16), (DirEntry*)(p + 32), (DirEntry*)(p + 48) };	
	for( int i = 0; i < NUM_OF_DIRENT_PER_BLOCK; i++){
		if(!(strcmp(direntry[i]->name, dirName))){
			retVal = direntry[i]->inodeNum;
			free(p);
			return retVal;
		}
	}
	free(p);
	return retVal;
}
int find_parent_inode_num(int inodeNum, char* dirName){
	Inode *pInode = (Inode *)malloc(sizeof(Inode));
	GetInode(inodeNum, pInode);

	if( pInode->dirBlockPtr[0] != -1 && pInode->dirBlockPtr[1] == -1 ){
		return same_name(pInode->dirBlockPtr[0], dirName);
	}
	else if(pInode->dirBlockPtr[1] != -1 && pInode->indirBlockPtr == -1){
		int ret;
		if((ret = same_name(pInode->dirBlockPtr[0], dirName)) == -1){
			return same_name(pInode->dirBlockPtr[1], dirName);
		}else return ret;
	}
	else if( pInode->indirBlockPtr != -1){ 
		int ret;
		if((ret = same_name(pInode->dirBlockPtr[0], dirName)) == -1){
			if((ret = same_name(pInode->dirBlockPtr[1], dirName)) == -1){
				char *p = (char *)malloc(BLOCK_SIZE*sizeof(char));
				DevReadBlock(pInode->indirBlockPtr, p);
				int** bk_num = (int **)(&p);

				for(int i = 0; i < 16; i++)
					if((ret =(same_name((*bk_num)[i], dirName))) != -1) return ret;	
			}
			else return ret;
		}else return ret;
	}
}
int Create(char *path, int count, int ParentInodeNum, int check){
    if(count == 1){

		int freeinode, freeblock;
		//디렉토리 있으면 실패 -1 리턴
		if(check == 2){				//open할 때
			int ret;
			//같은게 없으면 -1
			if((ret = find_parent_inode_num(ParentInodeNum, path)) == -1) return -1;
			else return ret;	//같은게 있으면 inode 값
		}
		
		if((find_parent_inode_num(ParentInodeNum, path)) != -1)	return -1;
		

		int blocknum = find_block_num(ParentInodeNum);		//열 블록 번호 가져오기

		//가져온 블록 읽기
		char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
		DevReadBlock(blocknum, pblock);
		//블록에 데이터 쓰기
		DirEntry *direntry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)pblock, (DirEntry*)(pblock + 16), (DirEntry*)(pblock + 32), (DirEntry*)(pblock + 48) };
		for(int i = 0; i < NUM_OF_DIRENT_PER_BLOCK; i++){
			if(direntry[i]->inodeNum == -1){
				//printf("inode : %d %d\n", i, direntry[i]->inodeNum);
				strcpy(direntry[i]->name , path);
				freeinode = GetFreeInodeNum();
				direntry[i]->inodeNum = freeinode;
				DevWriteBlock(blocknum, pblock);

				if(check == 0){
					//디렉토리 블록 생성
					char *p = (char *)malloc(BLOCK_SIZE*sizeof(char));		
					DirEntry *d[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)p, (DirEntry*)(p + 16), (DirEntry*)(p + 32), (DirEntry*)(p + 48) };		
					strcpy(d[0]->name , ".");
					d[0]->inodeNum = freeinode;
					strcpy(d[1]->name , "..");
					d[1]->inodeNum = ParentInodeNum;
					strcpy(d[2]->name , "");
					d[2]->inodeNum = -1;
					strcpy(d[3]->name , "");
					d[3]->inodeNum = -1;

					freeblock = GetFreeBlockNum();
					DevWriteBlock(freeblock, p);
				}
				
				//inode 설정
				Inode *pInode = (Inode *)malloc(sizeof(Inode));
				GetInode(freeinode, pInode);

				if( check == 0){
					pInode->size = 64;
					pInode->type = FILE_TYPE_DIR;
					pInode->dirBlockPtr[0] = freeblock;
				}
				else if( check == 1){
					pInode->size = 0;
					pInode->type = FILE_TYPE_FILE;
					pInode->dirBlockPtr[0] = -1;
				}
				pInode->dirBlockPtr[1] = -1;
				pInode->indirBlockPtr = -1;
				PutInode(freeinode, pInode);

				//비트맵 설정
				if(check == 0)
					SetBlockBitmap(freeblock);
				SetInodeBitmap(freeinode);

				//인포변경
                if (check == 0)
					info_change(1, 0, 1);
				else if( check == 1)
					info_change(-1,-1, 1);

				if(check == 0)
					return 0;
				else if(check == 1)
					return freeinode;
			}
		}	
    }else if( count > 1){
		//디렉토리명 길이 찾기
		int length = 0;
		char *i = path;
		while( *i != '/'){
			i++;
			length++;
		}
		//디렉토리명 넣기 
		char *dirname = (char *)malloc(length + 1);
		i = path;
		int j = 0;
		while( *i != '/'){
			dirname[j] = *i;
			i++; j++;
		}
		dirname[j] ='\0';

		--count;
		//printf("dir : %s\n", dirname );
		return Create(path + length + 1, count, find_parent_inode_num(ParentInodeNum, dirname), check);
    }
}
int		OpenFile(const char* szFileName, OpenFlag flag)
{
	char *path = (char *)malloc(strlen(szFileName) + 1);
	strcpy(path, szFileName);
	int count = 0;		// "/" 갯수 확인
    char *i = path;
    while( *i != '\0'){
		if( *i == '/'){
			count++;
		}
        i++;
    }
    //디렉토리 이름따기
	char *filename = (char *)malloc(strlen(szFileName));
	int j = 0;
    i = path + 1;
    while( *i != '\0'){
        filename[j] = *i;
        j++; i++;
    }
    filename[j] = '\0';

	if( flag == OPEN_FLAG_CREATE){
		int inode;
		if( (inode = Create(filename, count, 0, 1)) == -1 )
			return -1;
		else{
			for( int i = 0; i < MAX_FD_ENTRY_LEN; i++){
				if(pFileDescTable->file[i].bUsed == 0){
					pFileDescTable->file[i].bUsed = 1;
					pFileDescTable->file[i].fileOffset = 0;
					pFileDescTable->file[i].inodeNum = inode;
					return i;
				}
			}
		}
	}else if( flag == OPEN_FLAG_READWRITE){
		int inode;
		if( (inode = Create(filename, count, 0, 2)) == -1 )	//같은게 없으면
			return -1;
		else{
			for( int i = 0; i < MAX_FD_ENTRY_LEN; i++){
				if(pFileDescTable->file[i].bUsed == 0){
					pFileDescTable->file[i].bUsed = 1;
					pFileDescTable->file[i].fileOffset = 0;
					pFileDescTable->file[i].inodeNum = inode;
					return i;
				}
			}
		}
	}
}

int Write(int inodeNum, char* buf, int offset ,int length){	 //쓰고 offset넘겨줌
	Inode *inode = (Inode *)malloc(sizeof(Inode));
	GetInode(inodeNum, inode);

	if(offset < 1152){
		int ptr = offset / 64;
		if(ptr == 0){
			if(offset == 0 && inode->dirBlockPtr[0] == -1){			//블록이 할당이 안되어있을 때
				inode->dirBlockPtr[0] = GetFreeBlockNum();
				PutInode(inodeNum, inode);

				char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
				int i;

				for(i = 0; offset < 64 && length != 0; offset++, i++ ,length-- ){
					pblock[offset] = buf[i];
					//printf("%c",buf[i]);
				}
				DevWriteBlock(inode->dirBlockPtr[0], pblock);
				free(pblock);

				SetBlockBitmap(inode->dirBlockPtr[0]);
				info_change(1, 0, -1);

				if(length > 0)
					return Write(inodeNum, buf + i, offset, length);
				else return offset;
			}else if(inode->dirBlockPtr[0] != -1){					//블록이 할당되어 있을 때
				char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
				DevReadBlock(inode->dirBlockPtr[0], pblock);
				int i;
				for(i = 0; offset < 64 && length != 0; offset++, i++ ,length-- ){
					pblock[offset] = buf[i];
					//printf("%c",buf[i]);
				}
				DevWriteBlock(inode->dirBlockPtr[0], pblock);
				free(pblock);

				if(length > 0)
					return Write(inodeNum, buf + i, offset, length);
				else return offset;
			}
		}else if( ptr == 1){
			if(offset == 64 && inode->dirBlockPtr[1] == -1){			//블록이 할당이 안되어있을 때
				inode->dirBlockPtr[1] = GetFreeBlockNum();
				PutInode(inodeNum, inode);

				char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
				int i;
				for(i = 0; offset < 128 && length != 0; offset++, i++ ,length-- ){
					pblock[offset % 64] = buf[i];
				//printf("%c",buf[i]);			
				}
				DevWriteBlock(inode->dirBlockPtr[1], pblock);
				free(pblock);

				SetBlockBitmap(inode->dirBlockPtr[1]);
				info_change(1, 0, -1);

				if(length > 0)
					return Write(inodeNum, buf + i, offset, length);
				else return offset;
			}else if(inode->dirBlockPtr[1] != -1){					//블록이 할당되어 있을 때
				char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
				DevReadBlock(inode->dirBlockPtr[1], pblock);
				int i;
				for(i = 0; offset < 128 && length != 0; offset++, i++ ,length-- ){
					pblock[offset % 64] = buf[i];
				//printf("%c",buf[i]);
				}
				DevWriteBlock(inode->dirBlockPtr[1], pblock);
				free(pblock);

				if(length > 0)
					return Write(inodeNum, buf + i, offset, length);
				else return offset;
			}
		}else if (ptr > 1){
			if(offset == 128 && inode->indirBlockPtr == -1){			//할당 안됬을 때
				inode->indirBlockPtr = GetFreeBlockNum();
				PutInode(inodeNum, inode);

				SetBlockBitmap(inode->indirBlockPtr);

				//direct pointer 16개 block
				//빈 블록할당
				char *nb = (char *)malloc(BLOCK_SIZE*sizeof(char));		
				int** bk_num = (int **)(&nb);
				(*bk_num)[0] = GetFreeBlockNum();
				for(int i = 1; i < 16; i++)
					(*bk_num)[i] = -1;
           		DevWriteBlock(inode->indirBlockPtr, nb);

				info_change(1, 0, -1);

				//directory entry block
				char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
				int i;
				for(i = 0; offset < 192 && length != 0; offset++, i++ ,length-- ){
					pblock[offset % 64] = buf[i];
					//printf("%c",buf[i]);
				}
				DevWriteBlock((*bk_num)[0], pblock);
				free(pblock);

				SetBlockBitmap((*bk_num)[0]);
				info_change(1, 0, -1);

				if(length > 0)
					return Write(inodeNum, buf + i, offset, length);
				else return offset;
			}else if( inode->indirBlockPtr != -1){						//할당 됬을때
				char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
				DevReadBlock(inode->indirBlockPtr, pblock);
				int** bk_num = (int **)(&pblock);
				//printf("ptr : %d  %d \n", ptr - 2,(*bk_num)[ptr - 2] );
				if( (*bk_num)[ptr - 2] == -1 ){							//할당 안됬을 때
					(*bk_num)[ptr - 2] = GetFreeBlockNum();
					int num = (*bk_num)[ptr - 2];
					//printf("num : %d",num);
					DevWriteBlock(inode->indirBlockPtr, pblock);
					//free(pblock);

					char* p = (char *)malloc(BLOCK_SIZE*sizeof(char));
					int i;
					for(i = 0; (offset % 64) != 63 && length != 0; offset++, i++ ,length-- ){
						p[offset % 64] = buf[i];
						//printf("%c",buf[i]);
					}

					if( ((offset % 64) == 63) && (length > 0)){
						p[offset % 64] = buf[i];
						//printf("%c",buf[i]);
						offset++; i++; length--;
					}
				
					DevWriteBlock(num, p);
					free(p);

					SetBlockBitmap(num);
					info_change(1, 0, -1);

					if(length > 0)
						return Write(inodeNum, buf + i, offset, length);
					else return offset;

				}else if( (*bk_num)[ptr - 2] != -1){					//할당 됬을 때
					int num = (*bk_num)[ptr - 2];

					char* p = (char *)malloc(BLOCK_SIZE*sizeof(char));
					DevReadBlock(num, p);
					int i;
					for(i = 0; ((offset % 64) != 63) && (length != 0); offset++, i++ ,length-- ){
						p[offset % 64] = buf[i];
					//printf("%c",buf[i]);
					}

					if( ((offset % 64) == 63) && (length > 0)){
						p[offset % 64] = buf[i];
						//printf("%c",buf[i]);
						offset++; i++; length--;
					}
				
					DevWriteBlock(num, p);
					free(p);
					if(length > 0)
						return Write(inodeNum, buf + i, offset, length);
					else return offset;
				}
			}		
		}
	}

}
int		WriteFile(int fileDesc, char* pBuffer, int length)
{
	if(pFileDescTable->file[fileDesc].bUsed == 1){
		int fileInode = pFileDescTable->file[fileDesc].inodeNum;
		int offset = pFileDescTable->file[fileDesc].fileOffset;
		//int min_len = (strlen(pBuffer) + 1) <= length ? (strlen(pBuffer) + 1) : length;
		if(offset + length <= 1152){			//저장할 수 있는 파일 크기 넘지 않을 때 
			//printf("file : %d\n", fileInode);
			pFileDescTable->file[fileDesc].fileOffset = Write(fileInode, pBuffer, offset, length);

			//printf("\n");
						//printf("offset :  %d\n", pFileDescTable->file[fileDesc].fileOffset);
			Inode *inode = (Inode *)malloc(sizeof(Inode));
			GetInode(fileInode, inode);
			inode->size += length;
			PutInode(fileInode, inode);
						//printf("getfree : %d\n", GetFreeBlockNum());
			return length;
		}else{									//저장할 수 있는 파일 크기 넘으면 오류
			return -1;
		}
	}else{										//파일이 열려 있지 않을 때
		return -1;
	}
}
int Read(int inodeNum, char* buf, int offset ,int length){	 //읽고 offset넘겨줌
	Inode *inode = (Inode *)malloc(sizeof(Inode));
	GetInode(inodeNum, inode);

	if(offset < 1152){
		int ptr = offset / 64;
		if(ptr == 0){
			if(inode->dirBlockPtr[0] != -1){					//블록이 할당되어 있을 때
				char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
				DevReadBlock(inode->dirBlockPtr[0], pblock);
				int i;
				for(i = 0; offset < 64 && length != 0; offset++, i++ ,length-- )
					buf[i] = pblock[offset];

				if(length > 0)
					return Read(inodeNum, buf + i, offset, length);
				else return offset;
			}
		}else if( ptr == 1){
			if(inode->dirBlockPtr[1] != -1){					//블록이 할당되어 있을 때
				char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
				DevReadBlock(inode->dirBlockPtr[1], pblock);
				int i;
				for(i = 0; offset < 128 && length != 0; offset++, i++ ,length-- )
					buf[i] = pblock[offset % 64];

				if(length > 0)
					return Read(inodeNum, buf + i, offset, length);
				else return offset;
			}
		}else if (ptr > 1){
			if( inode->indirBlockPtr != -1){						//할당 됬을때
				char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
				DevReadBlock(inode->indirBlockPtr, pblock);
				int** bk_num = (int **)(&pblock);
				//printf("ptr : %d  %d \n", ptr - 2,(*bk_num)[ptr - 2] );
				if( (*bk_num)[ptr - 2] != -1){					//할당 됬을 때
					int num = (*bk_num)[ptr - 2];

					char* p = (char *)malloc(BLOCK_SIZE*sizeof(char));
					DevReadBlock(num, p);
					int i;
					for(i = 0; ((offset % 64) != 63) && (length != 0); offset++, i++ ,length-- )
						buf[i] = p[offset % 64];

					if( ((offset % 64) == 63) && (length > 0)){
						buf[i] = p[offset % 64];
						offset++; i++; length--;
					}
					if(length > 0)
						return Read(inodeNum, buf + i, offset, length);
					else return offset;
				}
			}		
		}
	}

}
int		ReadFile(int fileDesc, char* pBuffer, int length)
{
	if(pFileDescTable->file[fileDesc].bUsed == 1){
		int fileInode = pFileDescTable->file[fileDesc].inodeNum;
		int offset = pFileDescTable->file[fileDesc].fileOffset;

		//Inode *inode = (Inode *)malloc(sizeof(Inode));
		//GetInode(fileInode, inode);
		//int len = inode->size - offset;
		//int min_len = len <= length ? len : length;

		pFileDescTable->file[fileDesc].fileOffset = Read(fileInode, pBuffer, offset, length);
		return length;
	}else{										//파일이 열려 있지 않을 때
		return -1;
	}
}
int		CloseFile(int fileDesc)
{
	if(pFileDescTable->file[fileDesc].bUsed == 1){
		pFileDescTable->file[fileDesc].bUsed = 0;
		pFileDescTable->file[fileDesc].fileOffset = 0;
		pFileDescTable->file[fileDesc].inodeNum = -1;
		return 0;
	}
	return -1;
}
void MoveLast(int inodeNum, int childinodeNum,char *Lname, int LinodeNum){
	
	Inode * iod = (Inode *)malloc(sizeof(Inode));
	GetInode(inodeNum, iod);
	//direct[0]
	if( iod->dirBlockPtr[0] != -1 ){
		char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
		DevReadBlock(iod->dirBlockPtr[0], pblock);
		int len = 0;
		DirEntry *direntry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)pblock, (DirEntry*)(pblock + 16), (DirEntry*)(pblock + 32), (DirEntry*)(pblock + 48) };
		for(int i = 0; i < NUM_OF_DIRENT_PER_BLOCK ; i++){
			if(direntry[i]->inodeNum == childinodeNum){
				//printf("my : %s , %d\n",direntry[i]->name, direntry[i]->inodeNum);
				strcpy(direntry[i]->name, Lname);
				direntry[i]->inodeNum = LinodeNum;
				DevWriteBlock(iod->dirBlockPtr[0],pblock);
				return;
			}
		}
	}
	//direct[1]
	if( iod->dirBlockPtr[1] != -1 ){
		char *pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
		DevReadBlock(iod->dirBlockPtr[1], pblock);
		DirEntry *dir[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)pblock, (DirEntry*)(pblock + 16), (DirEntry*)(pblock + 32), (DirEntry*)(pblock + 48) };
		for(int i = 0; i < NUM_OF_DIRENT_PER_BLOCK ; i++){
			if(dir[i]->inodeNum == childinodeNum){
				strcpy(dir[i]->name, Lname);
				dir[i]->inodeNum = LinodeNum;
				DevWriteBlock(iod->dirBlockPtr[1],pblock);
				return;
			}
		}
	}
	//indirect
	if( iod->indirBlockPtr != -1 ){
		char *pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
		DevReadBlock(iod->indirBlockPtr, pblock);
		int** bk_num = (int **)(&pblock);

		for(int i = 0; i < 16; i++){
			if((*bk_num)[i] != -1){
				char * p =(char *)malloc(BLOCK_SIZE*sizeof(char));
				DevReadBlock((*bk_num)[i], p);

				DirEntry * entry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry *)p, (DirEntry *)(p + 16), (DirEntry *)(p + 32), (DirEntry *)(p + 48)};
				for(int i = 0; i < NUM_OF_DIRENT_PER_BLOCK ; i++){
					if(entry[i]->inodeNum == childinodeNum){
						strcpy(entry[i]->name, Lname);
						entry[i]->inodeNum = LinodeNum;
						DevWriteBlock((*bk_num)[i],pblock);
						return;
					}
				}
			}
		}
	}
}
int		RemoveFile(const char* szFileName)
{
	char *path = (char *)malloc(strlen(szFileName) + 1);
	strcpy(path, szFileName);
	int count = 0;		// "/" 갯수 확인
    char *i = path;
    while( *i != '\0'){
		if( *i == '/'){
			count++;
		}
        i++;
    }

    //디렉토리 이름따기
	char *dirname = (char *)malloc(strlen(szFileName));
	int j = 0;
    i = path + 1;
    while( *i != '\0'){
        dirname[j] = *i;
        j++; i++;
    }
    dirname[j] = '\0';

	int inode;
	if( (inode = Create(dirname, count, 0, 2)) == -1 )	//같은게 없으면 - (2)
		return -1;
	else{
		//마지막 이름따기
		int numCount = 0;
		while(dirname[j] != '/'){
			j--;
			numCount++;
		}
		j++;

		char name[numCount];
		int index = 0;
		while(dirname[j] != '\0'){
			name[index] = dirname[j];
			j++; index++;
		}
		name[index] = '\0';

		char parent[(strlen(szFileName) - numCount)];
		for( int i = 0; i < (strlen(szFileName) - numCount) - 1; i++){
			parent[i] = dirname[i];
		}
		parent[(strlen(szFileName) - numCount) - 1] = '\0';

		int parentInode = Create(parent, count - 1, 0, 2);

		//부모에서 마지막 블록 찾아와서 해당자리 지우기 
		//printf("p: %d", d[1]->inodeNum);
		Inode * iod = (Inode *)malloc(sizeof(Inode));
		GetInode(parentInode, iod);

		char Lname[MAX_NAME_LEN];
		int LinodeNum = -1;

		//indirect
		int indir_bk_num = 0;
		if( iod->indirBlockPtr != -1 ){
			char *pb = (char *)malloc(BLOCK_SIZE*sizeof(char));
			DevReadBlock(iod->indirBlockPtr, pb);
			int** bk_num = (int **)(&pb);
			//printf("indir : %d \n", iod->indirBlockPtr);
			//printf("dir : %d \n", iod->dirBlockPtr[0]);
			int Chck = 0;
			for(int i = 15; i >=0 ; i--){
				if((*bk_num)[i] != -1){
										//printf("bk : %d \n", (*bk_num)[i]);
					char * p =(char *)malloc(BLOCK_SIZE*sizeof(char));
					DevReadBlock((*bk_num)[i], p);

					DirEntry * entry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry *)p, (DirEntry *)(p + 16), (DirEntry *)(p + 32), (DirEntry *)(p + 48)};
					for(int j = NUM_OF_DIRENT_PER_BLOCK - 1; j >= 0 ; j--){
						if(entry[j]->inodeNum != -1){
							//printf("3: name : %s", entry[j]->name);
							strcpy(Lname, entry[j]->name);
							LinodeNum = entry[j]->inodeNum;							
							strcpy(entry[j]->name, "");
							entry[j]->inodeNum = -1;
							DevWriteBlock((*bk_num)[i],p);
							Chck = 1;
							if( i == 0 && j == 0 ){
								int block = (*bk_num)[i];
								char * reset = (char *)calloc(BLOCK_SIZE, sizeof(char));
								DevWriteBlock(block, reset);
								ResetBlockBitmap(block);
								//인포변경
								info_change(0, 1, -1);
								
								int num = iod->indirBlockPtr;
								reset = (char *)calloc(BLOCK_SIZE, sizeof(char));
								DevWriteBlock(num, reset);
								ResetBlockBitmap(num);

								iod->indirBlockPtr = -1;
								PutInode(parentInode, iod);
								//인포변경
								info_change(0, 1, -1);

							}else if( j == 0  && i > 0){
								int block = (*bk_num)[i];
								(*bk_num)[i] = -1;
								DevWriteBlock(iod->indirBlockPtr, pb);

								char * reset = (char *)calloc(BLOCK_SIZE, sizeof(char));
								DevWriteBlock(block, reset);
								ResetBlockBitmap(block);
								//인포변경
								info_change(0, 1, -1);
							}
							break;
						}
					}
				}
				if(Chck == 1) break;
			}
		}
		//direct[1]
		if( iod->indirBlockPtr == -1 && iod->dirBlockPtr[1] != -1 && LinodeNum == -1){
			char* p = (char *)malloc(BLOCK_SIZE*sizeof(char));
			DevReadBlock(iod->dirBlockPtr[1], p);
			DirEntry *dir[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)p, (DirEntry*)(p + 16), (DirEntry*)(p + 32), (DirEntry*)(p + 48) };
			for(int i = NUM_OF_DIRENT_PER_BLOCK - 1; i >= 0 ; i--){
				if(dir[i]->inodeNum != -1){
					strcpy(Lname, dir[i]->name);
					LinodeNum = dir[i]->inodeNum;							
					strcpy(dir[i]->name, "");
					dir[i]->inodeNum = -1;
					DevWriteBlock(iod->dirBlockPtr[1],p);
					if( i == 0){
						char * reset = (char *)calloc(BLOCK_SIZE, sizeof(char));

						int num = iod->dirBlockPtr[1];
						DevWriteBlock(num, reset);
						ResetBlockBitmap(num);

						iod->dirBlockPtr[1] = -1;
						PutInode(parentInode, iod);
						//인포변경
						info_change(0, 1, -1);
					}
					break;
				}
			}

		}
		//direct[0]
		if( iod->indirBlockPtr == -1 && iod->dirBlockPtr[1] == -1 && iod->dirBlockPtr[0] != -1 && LinodeNum == -1){
			char* p = (char *)malloc(BLOCK_SIZE*sizeof(char));
			DevReadBlock(iod->dirBlockPtr[0], p);
			int len = 0;
			DirEntry *direntry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)p, (DirEntry*)(p + 16), (DirEntry*)(p + 32), (DirEntry*)(p + 48) };
			for(int i = NUM_OF_DIRENT_PER_BLOCK - 1 ; i >= 1  ; i--){
				if(direntry[i]->inodeNum != -1 && strcmp(direntry[i]->name, "..") != 0 ){
					strcpy(Lname, direntry[i]->name);
					LinodeNum = direntry[i]->inodeNum;
					strcpy(direntry[i]->name, "");
					direntry[i]->inodeNum = -1;
					DevWriteBlock(iod->dirBlockPtr[0],p);
					break;
				}
			}
		}

		//printf("name : %s,p : % d, inode : %d\n", Lname, parentInode ,inode);
		//부모에서 마지막 찾은 애 위치 바꾸기 
		if( LinodeNum != inode)
			MoveLast(parentInode, inode, Lname, LinodeNum);


		Inode * pInode = (Inode *)malloc(sizeof(Inode));
		GetInode(inode, pInode);
		
		if(pInode->dirBlockPtr[0] != -1){
			char * reset = (char *)calloc(BLOCK_SIZE, sizeof(char));
			DevWriteBlock(pInode->dirBlockPtr[0], reset);
			ResetBlockBitmap(pInode->dirBlockPtr[0]);
			info_change(0, 1, -1);			//인포변경
		}
		if(pInode->dirBlockPtr[1] != -1){
			char * reset = (char *)calloc(BLOCK_SIZE, sizeof(char));
			DevWriteBlock(pInode->dirBlockPtr[1], reset);
			ResetBlockBitmap(pInode->dirBlockPtr[1]);
			info_change(0, 1, -1);			//인포변경
		}
		if(pInode->indirBlockPtr != -1){
			char *pb = (char *)malloc(BLOCK_SIZE*sizeof(char));
			DevReadBlock(pInode->indirBlockPtr, pb);
			int** bk_num = (int **)(&pb);

			for(int i = 0; i <= 15 ; i++){
				if((*bk_num)[i] != -1){
					char * reset = (char *)calloc(BLOCK_SIZE, sizeof(char));
					DevWriteBlock((*bk_num)[i], reset);
					ResetBlockBitmap((*bk_num)[i]);
					info_change(0, 1, -1);			//인포변경
				}
			}
			char * reset = (char *)calloc(BLOCK_SIZE, sizeof(char));
			DevWriteBlock(pInode->indirBlockPtr, reset);
			ResetBlockBitmap(pInode->indirBlockPtr);
			info_change(0, 1, -1);			//인포변경
		}

		//아이노드 삭제
		Inode * node = (Inode *)calloc(sizeof(Inode), sizeof(char));
		PutInode(inode, node);
		ResetInodeBitmap(inode);
		info_change(-1, -1, 0);			//인포변경

		return 0;
	}
}
int		MakeDir(const char* szDirName)
{

	char *path = (char *)malloc(strlen(szDirName) + 1);
	strcpy(path, szDirName);
	int count = 0;		// "/" 갯수 확인
    char *i = path;
    while( *i != '\0'){
		if( *i == '/'){
			count++;
		}
        i++;
    }
    //디렉토리 이름따기
	char *dirname = (char *)malloc(strlen(szDirName));
	int j = 0;
    i = path + 1;
    while( *i != '\0'){
        dirname[j] = *i;
        j++; i++;
    }
    dirname[j] = '\0';

	return Create(dirname, count, 0, 0);
}
int		RemoveDir(const char* szDirName)
{
	char *path = (char *)malloc(strlen(szDirName) + 1);
	strcpy(path, szDirName);
	int count = 0;		// "/" 갯수 확인
    char *i = path;
    while( *i != '\0'){
		if( *i == '/'){
			count++;
		}
        i++;
    }

    //디렉토리 이름따기
	char *dirname = (char *)malloc(strlen(szDirName));
	int j = 0;
    i = path + 1;
    while( *i != '\0'){
        dirname[j] = *i;
        j++; i++;
    }
    dirname[j] = '\0';

	int inode;
	if( (inode = Create(dirname, count, 0, 2)) == -1 )	//같은게 없으면 - (2)
		return -1;
	else{
		//printf("inode : %d\n", inode);
		Inode * pInode = (Inode *)malloc(sizeof(Inode));
		GetInode(inode, pInode);
		if(pInode->dirBlockPtr[0] != -1 && pInode->dirBlockPtr[1] == -1){		//조건 확인하고 제거
			int my_free_block = pInode->dirBlockPtr[0];
			char * pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
			DevReadBlock(pInode->dirBlockPtr[0], pblock);
			
			DirEntry *d[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)pblock, (DirEntry*)(pblock + 16), (DirEntry*)(pblock + 32), (DirEntry*)(pblock + 48) };
			if(d[2]->inodeNum == -1){											//., ..뺴고 index = 2가 비어있으면 삭제
				//마지막 이름따기
				int numCount = 0;
				while(dirname[j] != '/'){
					j--;
					numCount++;
				}
				j++;

				char name[numCount];
				int index = 0;
				while(dirname[j] != '\0'){
					name[index] = dirname[j];
					j++; index++;
				}
				name[index] = '\0';

				//부모에서 마지막 블록 찾아와서 해당자리 지우기 
				//printf("p: %d", d[1]->inodeNum);
				Inode * iod = (Inode *)malloc(sizeof(Inode));
				GetInode(d[1]->inodeNum, iod);

				char Lname[MAX_NAME_LEN];
				int LinodeNum = -1;

				//indirect
				int indir_bk_num = 0;
				if( iod->indirBlockPtr != -1 ){
					char *pb = (char *)malloc(BLOCK_SIZE*sizeof(char));
					DevReadBlock(iod->indirBlockPtr, pb);
					int** bk_num = (int **)(&pb);
					//printf("indir : %d \n", iod->indirBlockPtr);
					//printf("dir : %d \n", iod->dirBlockPtr[0]);
					int Chck = 0;
					for(int i = 15; i >=0 ; i--){
						if((*bk_num)[i] != -1){
												//printf("bk : %d \n", (*bk_num)[i]);
							char * p =(char *)malloc(BLOCK_SIZE*sizeof(char));
							DevReadBlock((*bk_num)[i], p);

							DirEntry * entry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry *)p, (DirEntry *)(p + 16), (DirEntry *)(p + 32), (DirEntry *)(p + 48)};
							for(int j = NUM_OF_DIRENT_PER_BLOCK - 1; j >= 0 ; j--){
								if(entry[j]->inodeNum != -1){
									//printf("3: name : %s", entry[j]->name);
									strcpy(Lname, entry[j]->name);
									LinodeNum = entry[j]->inodeNum;							
									strcpy(entry[j]->name, "");
									entry[j]->inodeNum = -1;
									DevWriteBlock((*bk_num)[i],p);
									Chck = 1;
									if( i == 0 && j == 0 ){
										int block = (*bk_num)[i];
										char * reset = (char *)calloc(BLOCK_SIZE, sizeof(char));
										DevWriteBlock(block, reset);
										ResetBlockBitmap(block);
										//인포변경
										info_change(0, 1, -1);
										
										int num = iod->indirBlockPtr;
										reset = (char *)calloc(BLOCK_SIZE, sizeof(char));
										DevWriteBlock(num, reset);
										ResetBlockBitmap(num);

										iod->indirBlockPtr = -1;
										PutInode(d[1]->inodeNum, iod);
										//인포변경
										info_change(0, 1, -1);

									}else if( j == 0  && i > 0){
										int block = (*bk_num)[i];
										(*bk_num)[i] = -1;
										DevWriteBlock(iod->indirBlockPtr, pb);

										char * reset = (char *)calloc(BLOCK_SIZE, sizeof(char));
										DevWriteBlock(block, reset);
										ResetBlockBitmap(block);
										//인포변경
										info_change(0, 1, -1);
									}
									break;
								}
							}
						}
						if(Chck == 1) break;
					}
				}
				//direct[1]
				if( iod->indirBlockPtr == -1 && iod->dirBlockPtr[1] != -1 && LinodeNum == -1){
					char* p = (char *)malloc(BLOCK_SIZE*sizeof(char));
					DevReadBlock(iod->dirBlockPtr[1], p);
					DirEntry *dir[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)p, (DirEntry*)(p + 16), (DirEntry*)(p + 32), (DirEntry*)(p + 48) };
					for(int i = NUM_OF_DIRENT_PER_BLOCK - 1; i >= 0 ; i--){
						if(dir[i]->inodeNum != -1){
							strcpy(Lname, dir[i]->name);
							LinodeNum = dir[i]->inodeNum;							
							strcpy(dir[i]->name, "");
							dir[i]->inodeNum = -1;
							DevWriteBlock(iod->dirBlockPtr[1],p);
							if( i == 0){
								char * reset = (char *)calloc(BLOCK_SIZE, sizeof(char));

								int num = iod->dirBlockPtr[1];
								DevWriteBlock(num, reset);
								ResetBlockBitmap(num);

								iod->dirBlockPtr[1] = -1;
								PutInode(d[1]->inodeNum, iod);
								//인포변경
								info_change(0, 1, -1);
							}
							break;
						}
					}

				}
				//direct[0]
				if( iod->indirBlockPtr == -1 && iod->dirBlockPtr[1] == -1 && iod->dirBlockPtr[0] != -1 && LinodeNum == -1){
					char* p = (char *)malloc(BLOCK_SIZE*sizeof(char));
					DevReadBlock(iod->dirBlockPtr[0], p);
					int len = 0;
					DirEntry *direntry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)p, (DirEntry*)(p + 16), (DirEntry*)(p + 32), (DirEntry*)(p + 48) };
					for(int i = NUM_OF_DIRENT_PER_BLOCK - 1 ; i >= 1  ; i--){
						if(direntry[i]->inodeNum != -1 && strcmp(direntry[i]->name, "..") != 0 ){
							strcpy(Lname, direntry[i]->name);
							LinodeNum = direntry[i]->inodeNum;
							strcpy(direntry[i]->name, "");
							direntry[i]->inodeNum = -1;
							DevWriteBlock(iod->dirBlockPtr[0],p);
							break;
						}
					}
				}

				//printf("name : %s,p : % d, inode : %d\n", Lname, d[1]->inodeNum,inode);
				//부모에서 마지막 찾은 애 위치 바꾸기 
				if( LinodeNum != inode){
					MoveLast(d[1]->inodeNum, inode, Lname, LinodeNum);
					//printf("a");
				}

				iod->size -= 64;
				PutInode(d[1]->inodeNum, iod);
				//블록 디렉토리 블록 삭제W
				char * reset = (char *)calloc(BLOCK_SIZE, sizeof(char));
				DevWriteBlock(my_free_block, reset);
				ResetBlockBitmap(my_free_block);
				//아이노드 삭제
				Inode * node = (Inode *)calloc(sizeof(Inode), sizeof(char));
				PutInode(inode, node);
				ResetInodeBitmap(inode);
				//인포변경
				info_change(0, 1, 0);

				return 0;
			}else{																//뭔가 있는경우 제거하면 안됨 - (1)
				return -1;
			}

		}else if( pInode->dirBlockPtr[0] != -1 || pInode->indirBlockPtr != -1){	//뭔가 있는경우 제거하면 안됨 - (1)
			return -1;
		}
	}
}
int Find_Count(int inodeNum){
	Inode *pInode = (Inode *)malloc(sizeof(Inode));
	GetInode(inodeNum, pInode);

	int len = 0;
	if( pInode->dirBlockPtr[0] == -1 ) return -1;
	//가져온 블록 읽기
	char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
	DevReadBlock(pInode->dirBlockPtr[0], pblock);

	DirEntry *direntry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)pblock, (DirEntry*)(pblock + 16), (DirEntry*)(pblock + 32), (DirEntry*)(pblock + 48) };
	for(int i = 0; i < NUM_OF_DIRENT_PER_BLOCK ; i++){
		//printf("dir : %d\n", direntry[i]->inodeNum);
		if(direntry[i]->inodeNum != -1)
			++len;
		else if(direntry[i]->inodeNum == -1) return len;
	}

	if( pInode->dirBlockPtr[1] == -1 ) return len;
	pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
	DevReadBlock(pInode->dirBlockPtr[1], pblock);

	DirEntry *dir[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)pblock, (DirEntry*)(pblock + 16), (DirEntry*)(pblock + 32), (DirEntry*)(pblock + 48) };
	for(int i = 0; i < NUM_OF_DIRENT_PER_BLOCK ; i++){
		if(dir[i]->inodeNum != -1)
			++len;
		else if(dir[i]->inodeNum == -1) return len;
	}

	if( pInode->indirBlockPtr == -1 ) return len;
	pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
	DevReadBlock(pInode->indirBlockPtr, pblock);
	int** bk_num = (int **)(&pblock);

	for(int i = 0; i < 16; i++){
		if((*bk_num)[i] != -1){
			char * p =(char *)malloc(BLOCK_SIZE*sizeof(char));
			DevReadBlock((*bk_num)[i], p);

			DirEntry * entry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry *)p, (DirEntry *)(p + 16), (DirEntry *)(p + 32), (DirEntry *)(p + 48)};
			for(int i = 0; i < NUM_OF_DIRENT_PER_BLOCK ; i++){
				if(entry[i]->inodeNum != -1)
					++len;
				else if(entry[i]->inodeNum == -1) return len;
			}
		}else if((*bk_num)[i] == -1) return len;
	}
}
int ObtainInfo(int inodeNum, DirEntryInfo* pDirEntry, int dirEntrys){
	Inode *pInode = (Inode *)malloc(sizeof(Inode));
	GetInode(inodeNum, pInode);

	if( pInode->dirBlockPtr[0] == -1 ) return -1;
	//가져온 블록 읽기
	char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
	DevReadBlock(pInode->dirBlockPtr[0], pblock);
	int len = 0;
	//블록에 데이터 쓰기
	DirEntry *direntry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)pblock, (DirEntry*)(pblock + 16), (DirEntry*)(pblock + 32), (DirEntry*)(pblock + 48) };
	for(int i = 0; i < NUM_OF_DIRENT_PER_BLOCK ; i++){
		if(direntry[i]->inodeNum != -1){
			strcpy(pDirEntry[len].name, direntry[i]->name);
			pDirEntry[len].inodeNum = direntry[i]->inodeNum;
			Inode *inode = (Inode *)malloc(sizeof(Inode));
			GetInode(pDirEntry[len].inodeNum, inode);
			pDirEntry[len].type = inode->type;
			len++; dirEntrys--;
			if(dirEntrys == 0) return len;
		}else if(direntry[i]->inodeNum == -1) return len;
	}

	if( pInode->dirBlockPtr[1] == -1 ) return len;
	pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
	DevReadBlock(pInode->dirBlockPtr[1], pblock);
	//블록에 데이터 쓰기

	DirEntry *dir[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry*)pblock, (DirEntry*)(pblock + 16), (DirEntry*)(pblock + 32), (DirEntry*)(pblock + 48) };
	for(int i = 0; i < NUM_OF_DIRENT_PER_BLOCK ; i++){
		if(dir[i]->inodeNum != -1){

			strcpy(pDirEntry[len].name, dir[i]->name);
			pDirEntry[len].inodeNum = dir[i]->inodeNum;
			Inode *inode = (Inode *)malloc(sizeof(Inode));
			GetInode(pDirEntry[len].inodeNum, inode);
			pDirEntry[len].type = inode->type;
			len++; dirEntrys--;
			if(dirEntrys == 0) return len;
		}else if(dir[i]->inodeNum == -1) return len;
	}

	if( pInode->indirBlockPtr == -1 ) return len;
	pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
	DevReadBlock(pInode->indirBlockPtr, pblock);
	int** bk_num = (int **)(&pblock);

	for(int i = 0; i < 16; i++){
		if((*bk_num)[i] != -1){
			char * p =(char *)malloc(BLOCK_SIZE*sizeof(char));
			DevReadBlock((*bk_num)[i], p);

			DirEntry * entry[NUM_OF_DIRENT_PER_BLOCK] = { (DirEntry *)p, (DirEntry *)(p + 16), (DirEntry *)(p + 32), (DirEntry *)(p + 48)};
			for(int i = 0; i < NUM_OF_DIRENT_PER_BLOCK ; i++){
				if(entry[i]->inodeNum != -1){
					strcpy(pDirEntry[len].name, entry[i]->name);
					pDirEntry[len].inodeNum = entry[i]->inodeNum;
					Inode *inode = (Inode *)malloc(sizeof(Inode));
					GetInode(pDirEntry[len].inodeNum, inode);
					pDirEntry[len].type = inode->type;
					len++; dirEntrys--;
					if(dirEntrys == 0) return len;
				}else if(entry[i]->inodeNum == -1) return len;
			}
		}else if((*bk_num)[i] == -1) return len;
	}
}
int 	EnumerateDirStatus(const char* szDirName, DirEntryInfo* pDirEntry, int dirEntrys )
{
	char *path = (char *)malloc(strlen(szDirName) + 1);
	strcpy(path, szDirName);
	int count = 0;		// "/" 갯수 확인
    char *i = path;
    while( *i != '\0'){
		if( *i == '/'){
			count++;
		}
        i++;
    }
    //디렉토리 이름따기
	char *dirname = (char *)malloc(strlen(szDirName));
	int j = 0;
    i = path + 1;
    while( *i != '\0'){
        dirname[j] = *i;
        j++; i++;
    }
    dirname[j] = '\0';

	int inode;
	if( (inode = Create(dirname, count, 0, 2)) == -1 )	//같은게 없으면
		return -1;
	else{											//같은게 있으면 정보 받아오기
		int entrycount = Find_Count(inode);
		int min = entrycount <= dirEntrys ? entrycount : dirEntrys;
		ObtainInfo(inode, pDirEntry, min);
	}
}


//-------------------------internal functions-------------------------
unsigned char Bit(unsigned char dest_data, unsigned char bitnum, unsigned char check)
{
	unsigned char bit_state = 0;
	unsigned char bit_num = 7 - bitnum;
	if(bit_num < 8){
		if(check == 1) dest_data = dest_data | (0x01 << bit_num);	//set => check == 1
		else if(check == 0) dest_data = dest_data & ~(0x01 << bit_num); //reset => check == 0
		else if(check == 2){						//get => check == 2
			bit_state = dest_data & (0x01 << bit_num);
			bit_state = bit_state >> bit_num;
			return bit_state;
		}
	}
	return dest_data;
}
void Set_Reset_Bitmap(int blkno, int s_blkno, unsigned char set_check)
{

	char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));

	DevReadBlock(blkno, pblock);

	if(set_check == 1)		//set => set_check == 1
		*(pblock + (s_blkno / 8)) = Bit(*(pblock + (s_blkno / 8)), (s_blkno % 8), 1);
	else if(set_check == 0)		//reset => set_check == 0
		*(pblock + (s_blkno / 8)) = Bit(*(pblock + (s_blkno / 8)), (s_blkno % 8), 0);

	DevWriteBlock(blkno, pblock);
}
int GetFreeNum(int blkno){

	char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
	
	DevReadBlock(blkno, pblock);

	unsigned char bit_state;


	if(blkno == 1){
		for( int j = 3 ; j < 8; j++){
			if((bit_state = Bit(pblock[2], j, 2)) == 0){
				return (2 * 8) + j;
			}
		}
		for(int i = 3; i < BLOCK_SIZE; i++ ){
			for(int j = 0; j < 8; j++ ) {
				if((bit_state = Bit(pblock[i], j, 2)) == 0){
					return (i * 8) + j;
				}
			}
		}
	}
	else{
		for(int i = 0; i < BLOCK_SIZE; i++ ){
			for(int j = 0; j < 8; j++ ) {
				if((bit_state = Bit(pblock[i], j, 2)) == 0){
					return (i * 8) + j;
				}
			}
		}
	}

}
void FileSysInit(void)
{	
	DevCreateDisk();

	char* pblock = (char *)calloc(BLOCK_SIZE, sizeof(char));

	for(int i = 0; i < 512; i++)
		DevWriteBlock(i, pblock);

	free(pblock);

	pFileDescTable = (FileDescTable *)malloc(sizeof(FileDescTable));

	for( int i = 0; i < MAX_FD_ENTRY_LEN; i++){
		pFileDescTable->file[i].bUsed = 0;
		pFileDescTable->file[i].fileOffset = 0;
		pFileDescTable->file[i].inodeNum = -1;
	}
}
void SetInodeBitmap(int inodeno)
{
	Set_Reset_Bitmap(2, inodeno, 1);
}
void ResetInodeBitmap(int inodeno)
{
	Set_Reset_Bitmap(2, inodeno, 0);
}
void SetBlockBitmap(int blkno)
{
	Set_Reset_Bitmap(1, blkno, 1);
}
void ResetBlockBitmap(int blkno)
{
	Set_Reset_Bitmap(1, blkno, 0);
}
void PutInode(int inodeno, Inode* pInode)
{
	char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
	DevReadBlock(((inodeno / NUM_OF_INODE_PER_BLOCK) + 3), pblock);
	memcpy((pblock + ((inodeno % NUM_OF_INODE_PER_BLOCK) << 4)), pInode, sizeof(Inode));
	DevWriteBlock(((inodeno / NUM_OF_INODE_PER_BLOCK) + 3), pblock);
}
void GetInode(int inodeno, Inode* pInode)
{
	char* pblock = (char *)malloc(BLOCK_SIZE*sizeof(char));
	DevReadBlock(((inodeno / NUM_OF_INODE_PER_BLOCK) + 3), pblock);
	memcpy( pInode, (pblock + ((inodeno % NUM_OF_INODE_PER_BLOCK) << 4)), sizeof(Inode));
}
int GetFreeInodeNum(void)
{
	return GetFreeNum(2);
}
int GetFreeBlockNum(void)
{
	return GetFreeNum(1);
}
