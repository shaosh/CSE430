/*---------------------------------Version Information------------------------------------
Shihuan Shao 1203060451
Version 1.0
1. initialize() is completed except the block 999 for root directory 
Version 1.1
1. cpin correct. 
2. Allocation of inode, directory entry and block is correct. 
3. fread and fwrite are used in writing file into blocks and write blocks into files
Version 1.2
1. Fix the bugs in allocate(file1, file2): measure file size of file1 and add file2's name 
the directory entry, rather than use file1 for both.
2. cpin uses just 1 parameter, not 2. 
3. cd is completed but need test
Version 1.3 
1. Add mkdir and cd. Need to check the reserved directory entry in each newly created dirs.
2. Use currDir to replace currBlk, which is more convenient.
3. Add printDir().
Version 1.4
1. Add write to stdout in cpout
2. Add chmod. Set the flags to ALLO_FLAG | File/Dir_FLAG | mode
3. Add ls.
4. Add mv.
Version 1.5
1. Add pwd and related supportive functions
2. Add find and related supportive functions
3. "/" only exists in root
Version 1.6
1. Add makedisk to copy data in Block[0~999] to a file called disk
Version 1.7
1. load shows there might be some mistakes during makedisk
Version 1.8
1. Just a backup. No progress.
Version 1.9
1. Loading of superblock, directory and texts are correct. But loading inode is not correct.
Version 2.0
1. Change 	struct inode* node[INODENUM] in struct freelist to struct inode node[INODENUM], 
so that all the basic struct doesn't need allocating memory. Corresponding to this change,
currInode = &node[i] rather than node[i]. Now the entire loading works correctly.
Version 2.1
1. Correct inumber calculation in loading
Version 2.2
1. Change currInode and currDir to currInode[] and currDir[], therefore each thread will have its
own pointer to their current position and inode
2. Add pthread_mutex_lock/unlock to limit the modification of allocateFile and mkdir, so that 
global variable inumber and nextBlock will be only modified by one thread each time
3. Split the main thread into k threads in execute()
4. Initialization and first execution is correct. Need to test loading from disk and execution.
Version 2.3
1. Change data block  in the ilist from 0-999 to 51 - 999.
2. Need to add useri-trace.txt.
Version 2.4
1. Add useri-trace.txt. 
2. Change the entry limit in each directory from 20 to 50.
Version 2.5
1. Need to add a printLock. Otherwise the output is a mess. 
Version 2.6
1. Add the printLock to control printing.
2. Setting directory entry number to 50 leads to the mess. Now change it to 32. 
Version 2.7
1. We can assume there are no syntax ERRORs. 
2. One directory may contains more than 1 block, which means 32 entries.
Version 2.8
1. Make it support multi-block directories in pwd, searchDir
2. mkdir still have problems to add dirname correctly.
3. It will be difficult to make find() support multi-block directory -- done.
4. Rest multi-block dir: cpin() -- unnecessary. What need to do here is already done in allocateFile().
5. Add more pthread statements to make it complete
Version 2.9
1. Sometimes stuck at 25th entry. 1st entry in non-first block of a directory name is wrong.
2. Problems: find() file not existed -- solved
3. fread() in cpin also cause some problem, but the extra words will not be written into the disk file
Version 3.0
1. struct direntry is 20 bytes with a int, but it is 16 bytes with a short. Kind of funny because int is 4 bytes
but short is 2 bytes. This is the root for the first entry name in a new directory block, sometimes stucking at the 25
block, some data appearing in the bottom of a new directory block.
Version 3.1
1. Correct bugs in find() which occured after multiblock dir.
2. Add two pthread_mutex for mv and chmod.
3. Don't allow the mv change file name to an existed same file name
4. Correct a bug in pwd(), otherwise there will redundant strings when a dir contains multiple blocks
5. Correct the incorrect loop in find(). Now it really support multi-block dir.
6. Need to test makedisk() and load. Then the proj is done.
Version 3.2
1. Correct the node position calculation. When inumber%16 = 0, listnum needs to decrement.
2. Change the int size in allocate() to short size(), otherwise the struct inode size will not be 32.
3. Add upper bound to max file number 640.
4. Not allow * and ? in file names.
5. mv, chmod support dir now, and no repeated name between files and dirs are allowed. 
Version 3.3
1. Deleted unnecessary comments, no more user_r.
2. Use sem_wait/post to replace pthread_mutex_lock/unlock.
---------------------------------Version Information------------------------------------*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <pthread.h>
#include <Windows.h>
#include <semaphore.h>
#pragma comment(lib, "pthreadVC2.lib")  

#define BLOCKSIZE 512
#define BLOCKNUM 1000
#define SUPER 1
#define ROOT 2
#define NFREE 100
#define ISIZE 40
#define ADDRNUM 8
#define DIRENTRYNUM 32
#define ROOT_I_NUM 1
#define FILENAME 10
#define TRACENAME 20
#define TOKEN 20
#define ROOTBLOCK 999
#define ALLO_FLAG 0100000
#define TWO_FLAG 060000
#define PLAIN_FLAG 000000
#define DIR_FLAG 040000
#define LARGE_FLAG 010000
#define USER_FLAG 004000
#define GROUP_FLAG 002000
#define READ_FLAG 000400
#define WRITE_FLAG 000200
#define EXE_FLAG 000100
#define GRWE_FLAG 000070
#define ORWE_FLAG 000007
#define ENTRYNAME 14
#define SELFDIR 0
#define PARENTDIR 1
#define INODENUM 16
#define FIRSTDATABLOCK 51
#define TRUE 1
#define FALSE 0
#define THREADNUM 100
#define MAXFILENUM 640

struct superblock{
	unsigned short isize;
	unsigned short fsize;
	unsigned short nfree;
	unsigned short free[100];
	unsigned short ninode;
	unsigned short inode[100];
	char flock;
	char ilock;
	char fmod;
};

struct inode{
	unsigned short flags;
	char nlinks;
	char uid;
	char gid;
	char size0;
	unsigned short size1;
	unsigned short addr[8];
	unsigned short actime[2];
	unsigned short modtime[2];
};

struct direntry{
	short dircode;
	char dirname[ENTRYNAME];
};

struct directory{
	struct direntry dirlist[DIRENTRYNUM];
};

struct freelist{
	struct inode node[INODENUM];
};

struct blocklist{
	int freeblock[101];
};

struct block{
	char space[BLOCKSIZE];
};

struct system{
	struct block blocknode[BLOCKNUM];
};

//global variables which don't need updating
struct system sys;
int firstDataBlock; // calculate the block after blocks used to store the free block list
int checkDisk = FALSE;
//global variables which need updating
struct directory* currDir[THREADNUM];//currDirThread and currInodeThread are used to record 
struct inode* currInode[THREADNUM];//the current dir and inode for each thread. Used in ls and pwd
int inumber = 1; // used to record the total file/directory entries
int nextBlock; // used to record which block to allocate
int findFlag = FALSE;
int currinodeFlag = FALSE;
/*
pthread_mutex_t mutex; //Used in cpin and mkdir, changing inumber, nextblock and other global vars
pthread_mutex_t printLock;	 //Used in printf, otherwise printing will be a mess
pthread_mutex_t modifyMv; //Used in mv, in case two functions changing the entry at the same time
pthread_mutex_t modifyChmod; //Used in chmod, in case two functions changing the inode at the same time*/
sem_t mutex;
sem_t printLock;
sem_t modifyMv;
sem_t modifyChmod;

int main(int, char**);
void initialize();
void initNewdir(int, int, int);
void execute(char*);	//Spawn threads
void* executeThread(void*);//execute files in each thread
void cpin(char*, FILE*, int);
void cd(char*, FILE*, int);
void cpout(char*, FILE*, int);
void mkdir(char*, FILE*, int);
void v6chmod(FILE*, int, int);
void ls(FILE*, int);
void mv(char*, char*, FILE*, int);
void pwd(FILE*, int);
void find(char*, FILE*, int);
void exitFile(void*);//execute exit(), but avoid overlapping with build-in exit()
int searchFile(char*, int);
int searchDir(char*, int);
void findInode(int, int);
int allocateFile(char*, char*, int, FILE*);
long fileSize(FILE*);
void printFile(int, int, int, char*);
void printDir(FILE*, int, int);
void dirPath(int, int, char*);
void filePath(char*, FILE*, int, int);
void printFilepath(int, int, char*, char*, FILE*);
void makedisk();
void load();
void updateFree(int, int);
int multiBlockDir(int);

int main(int argc, char* argv[]){	
	int i, j, k;
	if (argc!=3)
		printf("ERROR: Invalid commands!\n");
	else if(access("disk", 0) == -1){
/*		printf("size of directory: %d\n", sizeof(struct directory));
		printf("size of direntry: %d\n", sizeof(struct direntry));
		printf("size of int: %d\n", sizeof(int));
		printf("size of short: %d\n", sizeof(short));
		printf("size of inode: %d\n", sizeof(struct inode));
		printf("size of superblock: %d\n", sizeof(struct superblock));*/
		initialize();
		execute(argv[2]);
		makedisk();
		pthread_exit(NULL);
		////////////////////////////////////////////
/*		for(i = ROOT; i < ISIZE + ROOT; i++){
			printf("\nilist:\n");
			printf("i = %d\n", i);
			for(j = 0; j < INODENUM; j++){
				printf("j = %d\n", j);
				printf("flag = %o\n",((struct freelist*)sys.blocknode[i].space)->node[j].flags);
				printf("nlinks = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].nlinks);
				printf("uid = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].uid);
				printf("gid = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].gid);
				printf("size0 = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].size0);
				printf("size1 = %d\n",((struct freelist*)sys.blocknode[i].space)->node[j].size1);
				printf("address = ");
				for(k = 0; k < 8; k++){
					printf("%d ",((struct freelist*)sys.blocknode[i].space)->node[j].addr[k] );
				}
				printf("\n");
				printf("====================================================\n");
				getchar();
			}
		}
		printf("\nnfree = %d", ((struct superblock*)sys.blocknode[SUPER].space)->nfree);
		printf("\nfreelist %d", ((struct superblock*)sys.blocknode[SUPER].space)->free[((struct superblock*)sys.blocknode[SUPER].space)->nfree]);
		printf("\n");
		for (i = 0; i < DIRENTRYNUM; i++){
			printf("i-number: %d dirname: %s\n", ((struct directory*)sys.blocknode[ROOTBLOCK].space)->dirlist[i].dircode, ((struct directory*)sys.blocknode[ROOTBLOCK].space)->dirlist[i].dirname);
		}
		for (i = 0; i < 6; i++){
			printf("\nsize: %d inode: %d", ((struct freelist*)sys.blocknode[2].space)->node[i].size1, i);
		}
		printf("\nsize of the sys: %d", sizeof(sys.blocknode));
		printf("\ninumber = %d", inumber++);
		printf("\nnextBlock = %d", nextBlock = ((struct superblock*)sys.blocknode[SUPER].space)->free[((struct superblock*)sys.blocknode[SUPER].space)->nfree - 1]);*/
	}
	else{
		checkDisk = TRUE;
		load();
/*		for(i = ROOT; i < ISIZE + ROOT; i++){
			printf("\nilist:\n");
			printf("i = %d\n", i);
			for(j = 0; j < INODENUM; j++){
				printf("j = %d\n", j);
				printf("flag = %o\n",((struct freelist*)sys.blocknode[i].space)->node[j].flags);
				printf("nlinks = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].nlinks);
				printf("uid = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].uid);
				printf("gid = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].gid);
				printf("size0 = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].size0);
				printf("size1 = %d\n",((struct freelist*)sys.blocknode[i].space)->node[j].size1);
				printf("address = ");
				for(k = 0; k < 8; k++){
					printf("%d ",((struct freelist*)sys.blocknode[i].space)->node[j].addr[k] );
				}
				printf("\n");
				printf("====================================================\n");
				getchar();
			}
		}
		printf("\nnfree = %d", ((struct superblock*)sys.blocknode[SUPER].space)->nfree);
		printf("\nfreelist %d\n", ((struct superblock*)sys.blocknode[SUPER].space)->free[((struct superblock*)sys.blocknode[SUPER].space)->nfree - 1]);
		printf("\ninumber = %d", inumber++);
		printf("\nnextBlock = %d", nextBlock = ((struct superblock*)sys.blocknode[SUPER].space)->free[((struct superblock*)sys.blocknode[SUPER].space)->nfree - 1]);
		printf("\n");
		for (i = 0; i < DIRENTRYNUM; i++){
			printf("i-number: %d dirname: %s\n", ((struct directory*)sys.blocknode[ROOTBLOCK].space)->dirlist[i].dircode, ((struct directory*)sys.blocknode[ROOTBLOCK].space)->dirlist[i].dirname);
		}*/
		execute(argv[2]);
		makedisk();
		pthread_exit(NULL);
	}
}

//---------------------------------Initialize the file system------------------------------------
void initialize(){
	int i, j, k;

	//*****************initialize superblock*****************
	((struct superblock*)sys.blocknode[SUPER].space)->nfree = NFREE;
	
	((struct superblock*)sys.blocknode[SUPER].space)->isize = ISIZE;
	for(i = 0; i < NFREE; i++){
		if(i == 0)
			((struct superblock*)sys.blocknode[SUPER].space)->free[i] = ISIZE + ROOT;
		else
			((struct superblock*)sys.blocknode[SUPER].space)->free[NFREE - i] = BLOCKNUM - i;
	}
	nextBlock = ((struct superblock*)sys.blocknode[SUPER].space)->free[((struct superblock*)sys.blocknode[SUPER].space)->nfree - 1];
	 //-------------------------------------test------------------------------
/*	printf("superblock freelist\n");
	for(i = 0; i < NFREE; i++){
		printf("%d ", ((struct superblock*)sys.blocknode[SUPER].space)->free[i]);
	}*/
	//*****************initialize ilist*****************
	for(i = ROOT; i < ISIZE + ROOT; i++){
		for(j = 0; j < INODENUM; j++){
			if(i == ROOT && j == 0){				
				((struct freelist*)sys.blocknode[i].space)->node[j].flags = 
					ALLO_FLAG | DIR_FLAG | READ_FLAG | WRITE_FLAG | EXE_FLAG | GRWE_FLAG | ORWE_FLAG;
				((struct freelist*)sys.blocknode[i].space)->node[j].nlinks = '0';
				((struct freelist*)sys.blocknode[i].space)->node[j].uid = '0';
				((struct freelist*)sys.blocknode[i].space)->node[j].gid = '0';
				((struct freelist*)sys.blocknode[i].space)->node[j].size0 = '0';
				((struct freelist*)sys.blocknode[i].space)->node[j].size1 = 3;

				for(k = 0; k < ADDRNUM; k++){
					if(k == 0)
						((struct freelist*)sys.blocknode[i].space)->node[j].addr[k] = nextBlock;
					else
						((struct freelist*)sys.blocknode[i].space)->node[j].addr[k] = 0;
				}
				initNewdir(nextBlock, ROOT_I_NUM, -1);
				inumber++;
				((struct superblock*)sys.blocknode[SUPER].space)->nfree--;
				nextBlock = ((struct superblock*)sys.blocknode[SUPER].space)->free[((struct superblock*)sys.blocknode[SUPER].space)->nfree - 1];
				
				 //-------------------------------------test------------------------------
/*				printf("\n");
				for (i = 0; i < DIRENTRYNUM; i++){
					printf("i-number: %d dirname: %s\n", ((struct directory*)currBlk.space)->dirlist[i].dircode, ((struct directory*)currBlk.space)->dirlist[i].dirname);
				}*/

			}
			else{
				((struct freelist*)sys.blocknode[i].space)->node[j].flags = 0;
				((struct freelist*)sys.blocknode[i].space)->node[j].nlinks = '0';
				((struct freelist*)sys.blocknode[i].space)->node[j].uid = '0';
				((struct freelist*)sys.blocknode[i].space)->node[j].gid = '0';
				((struct freelist*)sys.blocknode[i].space)->node[j].size0 = '0';
				((struct freelist*)sys.blocknode[i].space)->node[j].size1 = 0;
				for(k = 0; k < ADDRNUM; k++)
					((struct freelist*)sys.blocknode[i].space)->node[j].addr[k] = 0;
			}
		}

		 //-------------------------------------test------------------------------
/*		printf("\nilist:\n");
		printf("i = %d\n", i);
		for(j = 0; j < INODENUM; j++){
			printf("j = %d\n", j);
			printf("flag = %o\n",((struct freelist*)sys.blocknode[i].space)->node[j].flags);
			printf("nlinks = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].nlinks);
			printf("uid = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].uid);
			printf("gid = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].gid);
			printf("size0 = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].size0);
			printf("size1 = %d\n",((struct freelist*)sys.blocknode[i].space)->node[j].size1);
			printf("====================================================\n");
			getchar();
		}
*/				
	}	

	//*****************initialize Block42~50 *****************
	for(i = ISIZE + ROOT, k = BLOCKNUM - NFREE; k >= FIRSTDATABLOCK; i++){
		for(j = 0; j < NFREE && k >= FIRSTDATABLOCK; j++){
			if(j == 0)
				((struct blocklist*)sys.blocknode[i].space)->freeblock[j] = i + 1;
			else {
				((struct blocklist*)sys.blocknode[i].space)->freeblock[NFREE + 1 - j] = k;
				k--;
			}
			if(k < FIRSTDATABLOCK){
				((struct blocklist*)sys.blocknode[i].space)->freeblock[0] = 0;
				break;
			}
		}
		if(k < FIRSTDATABLOCK){
			((struct blocklist*)sys.blocknode[i].space)->freeblock[1] = j;
			break;
		}
		((struct blocklist*)sys.blocknode[i].space)->freeblock[1] = j - 1;
	}
	firstDataBlock = i;
	return;
	//-------------------------------------test------------------------------
/*	printf("\nfree blocks\n");
	for(i = 42; i < 52; i++){
		for(j = 0; j < NFREE+1; j++)
			printf("%d ", ((struct blocklist*)sys.blocknode[i].space)->freeblock[j]);
		printf("\n============================================\n");
	}
	printf("first data block = %d", firstDataBlock);*/
}

//---------------------------------Create new directory------------------------------------
void initNewdir(int blknum, int inum, int parentnum){
	int i;
	for(i = 0; i < DIRENTRYNUM; i++){
		if(i == SELFDIR){
			((struct directory*)sys.blocknode[blknum].space)->dirlist[i].dircode = inum;
			strcpy(((struct directory*)sys.blocknode[blknum].space)->dirlist[i].dirname, ".");
		}
		else if(i == PARENTDIR){
			((struct directory*)sys.blocknode[blknum].space)->dirlist[i].dircode = parentnum;
			strcpy(((struct directory*)sys.blocknode[blknum].space)->dirlist[i].dirname, "..");
		}
		else if(i == 2 && (parentnum == -1 ||  blknum == ROOTBLOCK)){
			((struct directory*)sys.blocknode[blknum].space)->dirlist[i].dircode = ROOT_I_NUM;
			strcpy(((struct directory*)sys.blocknode[blknum].space)->dirlist[i].dirname, "/");
		}
		else if(((struct directory*)sys.blocknode[blknum].space)->dirlist[i].dircode != 0){
			((struct directory*)sys.blocknode[blknum].space)->dirlist[i].dircode = 0;
			strcpy(((struct directory*)sys.blocknode[blknum].space)->dirlist[i].dirname, "");
		}
	}
	//-------------------------------------test------------------------------
/*	printf("\ndirectory test\n");
	for (i = 0; i < DIRENTRYNUM; i++){
		printf("i-number: %d dirname: %s\n", ((struct directory*)sys.blocknode[998].space)->dirlist[i].dircode, ((struct directory*)sys.blocknode[998].space)->dirlist[i].dirname);
	}
	printf("strlen = %d\n", strlen(((struct directory*)sys.blocknode[998].space)->dirlist[29].dirname));
	system("pause");*/
}

//---------------------------------Spawn pthreads to execute functions------------------------------------
void execute(char* k){
	int threadnum = atoi(k), i, rc;
	void *status;

	pthread_t threads[THREADNUM];
	pthread_attr_t attr;

	if(threadnum >= THREADNUM){		
		printf("ERROR: thread num is not enough\n");
		return;
	}

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	sem_init(&mutex, 0, 1);
	sem_init(&printLock, 0, 1);
	sem_init(&modifyMv, 0, 1);
	sem_init(&modifyChmod, 0, 1);
/*	pthread_mutex_init(&mutex, NULL);
	pthread_mutex_init(&printLock, NULL);
	pthread_mutex_init(&modifyMv, NULL);
	pthread_mutex_init(&modifyChmod, NULL);*/

	for(i = 1; i <= threadnum; i++){
		currDir[i] = (struct directory*)sys.blocknode[ROOTBLOCK].space;
		rc = pthread_create(&threads[i], &attr, executeThread, (void *)i);
		if (rc){
			printf("ERROR; return code from pthread_create() is %d\n", rc);
			exit(-1);
		}
	}
	pthread_attr_destroy(&attr);

	for(i = 1; i <= threadnum; i++){
		rc = pthread_join(threads[i], &status);
		if (rc){
			printf("ERROR; return code from pthread_join() is %d\n", rc);
			exit(-1);
		}
		printf("Main: completed join with thread %d having a status of %d\n", i, (int)status);
	}
	printf("Main: program completed. Exiting.\n");
	sem_destroy(&mutex);
	sem_destroy(&printLock);
	sem_destroy(&modifyMv);
	sem_destroy(&modifyChmod);
/*	pthread_mutex_destroy(&mutex);
	pthread_mutex_destroy(&printLock);
	pthread_mutex_destroy(&modifyMv);
	pthread_mutex_destroy(&modifyChmod);*/
	return;
}

//---------------------------------Analyze commands and call corresponding function------------------------------------
void* executeThread(void* fileID){
	int i, mode, flag, j;
	char num[10];
	char token1[TOKEN];
	char token2[TOKEN];
	char token3[TOKEN];
	char filename[FILENAME];
	char tracename[TRACENAME];
	FILE* filelist; 
	FILE* tracefile;

	i = (int)fileID;
//	pthread_mutex_lock(&printLock);
	sem_wait(&printLock);
	printf("Thread %d starting...\n", i);
	sem_post(&printLock);
//	pthread_mutex_unlock(&printLock);

	if(checkDisk == FALSE){
		strcpy(filename, "user");
		strcpy(tracename, "user");
	}
	else if(checkDisk == TRUE){
		strcpy(filename, "user");
		strcpy(tracename, "user");
	}

	itoa(i, num, 10);
	strcat(filename, num);
	strcat(tracename, num);
	strcat(filename, ".txt");
	strcat(tracename, "-trace.txt");
	filelist = fopen(filename, "rb");
	tracefile = fopen(tracename, "wb");
	fscanf(filelist, "%s", token1);
	while(strcmp(token1, "exit") != 0){
		if(strcmp(token1, "cpin") == 0){
			fscanf(filelist, "%s", token2);
			fscanf(filelist, "%s", token3);	
			fprintf(tracefile, "cpin %s %s\n", token2, token3);
//			pthread_mutex_lock(&printLock);
			sem_wait(&printLock);
			printf("cpin %s %s (Thread %d)\n", token2, token3, i);
			if(strstr(token2, "*") != NULL || strstr(token2, "?") != NULL ||
			strstr(token3, "*") != NULL || strstr(token3, "?") != NULL){
				printf("ERROR: No wildcards allowed in the file name! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\t No wildcards allowed in the file name!\n");
			}
			else if(inumber > MAXFILENUM){
				printf("ERROR: No free inode! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tNo free inode!\n");
			}
			else if((access(token2, 0) == 0) && searchFile(token3, i) == 0 &&  searchDir(token3, i) == 0){
/*				pthread_mutex_unlock(&printLock);
				pthread_mutex_lock(&mutex);*/
				sem_post(&printLock);
				sem_wait(&mutex);
				flag = allocateFile(token2, token3, i, tracefile);
				if (flag == TRUE)
					cpin(token2, tracefile, i);
//				pthread_mutex_unlock(&mutex);
				sem_post(&mutex);
			}
			else{				
				printf("ERROR: File existed / not existed! (Thread %d)\n", i);
//				pthread_mutex_unlock(&printLock);
				sem_post(&printLock);
				fprintf(tracefile, "\tFile existed / not existed!\n");
			}
		}
		else if(strcmp(token1, "cd") == 0){
			fscanf(filelist, "%s", token2);
			fprintf(tracefile, "cd %s\n", token2);
//			pthread_mutex_lock(&printLock);
			sem_wait(&printLock);
			printf("cd %s (Thread %d)\n", token2, i);	
			if(strstr(token2, "*") != NULL || strstr(token2, "?") != NULL){
				printf("ERROR: No wildcards allowed in the file name! (Thread %d)\n", i);
//				pthread_mutex_unlock(&printLock);
				sem_post(&printLock);
				fprintf(tracefile, "\t No wildcards allowed in the file name!\n");
			}
			else if(searchDir(token2, i) > 0){
//				pthread_mutex_unlock(&printLock);
				sem_post(&printLock);
				cd(token2, tracefile, i);
			}
			else{				
				printf("ERROR: Invalid directory! (Thread %d)\n", i);
//				pthread_mutex_unlock(&printLock);
				sem_post(&printLock);
				fprintf(tracefile, "\tInvalid directory!\n");
			}
		}
		else if(strcmp(token1, "cpout") == 0){
			fscanf(filelist, "%s", token2);
			fscanf(filelist, "%s", token3);	
			fprintf(tracefile, "cpout %s %s\n", token2, token3);
//			pthread_mutex_lock(&printLock);
			sem_wait(&printLock);
			printf("cpout %s %s (Thread %d)\n", token2, token3, i);
			if(strstr(token2, "*") != NULL || strstr(token2, "?") != NULL||
			strstr(token3, "*") != NULL || strstr(token3, "?") != NULL){
				printf("ERROR: No wildcards allowed in the file name! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tNo wildcards allowed in the file name!\n");
			}
			else if(searchFile(token2, i) > 0){	
				cpout(token3, tracefile, i);
			}
			else{
				printf("ERROR: File not existed! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tFile not existed!\n");
			}			
		}
		else if(strcmp(token1, "mkdir") == 0){			
			fscanf(filelist, "%s", token2);
			fprintf(tracefile, "mkdir %s\n", token2);
			sem_wait(&printLock);
//			pthread_mutex_lock(&printLock);
			printf("mkdir %s (Thread %d)\n", token2, i);		
			if(inumber > MAXFILENUM){
				printf("ERROR: No free inode! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tNo free inode!\n");
			}
			else if(strstr(token2, "*") != NULL || strstr(token2, "?") != NULL){
				printf("ERROR: No wildcards allowed in the file name! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\t No wildcards allowed in the file name!\n");
			}
			else if(searchDir(token2, i) == 0 && searchFile(token2, i) == 0){	
/*				pthread_mutex_unlock(&printLock);
				pthread_mutex_lock(&mutex);*/
				sem_post(&printLock);
				sem_wait(&mutex);
				mkdir(token2, tracefile, i);
				sem_post(&mutex);
//				pthread_mutex_unlock(&mutex);
			}
			else{
				printf("ERROR: File with the same name existed! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tFile with the same name existed!\n");
			}			
		}
		else if(strcmp(token1, "chmod") == 0){
			fscanf(filelist, "%o", &mode);
			fscanf(filelist, "%s", token3);	
			fprintf(tracefile, "chmode %o %s\n", mode, token3);
//			pthread_mutex_lock(&printLock);
			sem_wait(&printLock);
			printf("chmode %o %s (Thread %d)\n", mode, token3, i);		
			if(mode > 07777 ){				
				printf("ERROR: Mode out of bound! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tMode out of bound!\n");
			}
			else if(strstr(token3, "*") != NULL || strstr(token3, "?") != NULL){
				printf("ERROR: No wildcards allowed in the file name! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tNo wildcards allowed in the file name!\n");
			}
			else if(searchFile(token3, i) > 0 || searchDir(token3, i) > 0){
/*				pthread_mutex_unlock(&printLock);
				pthread_mutex_lock(&modifyChmod);*/
				sem_post(&printLock);
				sem_wait(&modifyChmod);
				v6chmod(tracefile, mode, i);
				sem_post(&modifyChmod);
//				pthread_mutex_unlock(&modifyChmod);
			}
			else{				
				printf("ERROR: File not existed! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tFile not existed!\n");
			}	
			
		}
		else if(strcmp(token1, "ls") == 0){
			fprintf(tracefile, "ls\n");
//			pthread_mutex_lock(&printLock);
			sem_wait(&printLock);
			printf("ls (Thread %d)\n", i);			
			ls(tracefile, i);
		}
		else if(strcmp(token1, "mv") == 0){
			fscanf(filelist, "%s", token2);
			fscanf(filelist, "%s", token3);	
			fprintf(tracefile, "mv %s %s\n", token2, token3);
//			pthread_mutex_lock(&printLock);
			sem_wait(&printLock);
			printf("mv %s %s (Thread %d)\n", token2, token3, i);
			if(strstr(token2, "*") != NULL || strstr(token2, "?") != NULL||
			strstr(token3, "*") != NULL || strstr(token3, "?") != NULL){
				printf("ERROR: No wildcards allowed in the file name! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tNo wildcards allowed in the file name!\n");
			}
			else if(searchFile(token3, i) > 0 || searchDir(token3, i) > 0){
				printf("ERROR: File with the same name existed! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tFile with the same name existed!\n");
			}
			else if(searchFile(token2, i) > 0 || searchDir(token2, i) > 0){
/*				pthread_mutex_unlock(&printLock);
				pthread_mutex_lock(&modifyMv);*/
				sem_post(&printLock);
				sem_wait(&modifyMv);
				mv(token2, token3, tracefile, i);
				sem_post(&modifyMv);
//				pthread_mutex_unlock(&modifyMv);
			}
			else{
				printf("ERROR: File not existed! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tFile not existed!\n");
			}			
		}
		else if(strcmp(token1, "pwd") == 0){
			fprintf(tracefile, "pwd\n");
//			pthread_mutex_lock(&printLock);
			sem_wait(&printLock);
			printf("pwd (Thread %d)\n", i);			
			pwd(tracefile, i);
		}
		else if(strcmp(token1, "find") == 0){
			fscanf(filelist, "%s", token2);
			fprintf(tracefile, "find %s\n", token2);
//			pthread_mutex_lock(&printLock);
			sem_wait(&printLock);
			printf("find %s(Thread %d)\n", token2, i);
			if(strstr(token2, "*") != NULL || strstr(token2, "?") != NULL){
				printf("ERROR: No wildcards allowed in the file name! (Thread %d)\n", i);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				fprintf(tracefile, "\tNo wildcards allowed in the file name!\n");
			}
			else {
				find(token2, tracefile, i);
			}				
		}
		else{
//			pthread_mutex_lock(&printLock);
			sem_wait(&printLock);
			printf("ERROR: Invalid commands! (Thread %d)\n", i);
			sem_post(&printLock);
//			pthread_mutex_unlock(&printLock);
			fprintf(tracefile, "Invalid commands!\n");
			fprintf(tracefile, "%s\n", token1);
		}
		fscanf(filelist, "%s", token1);
	}
	if(strcmp(token1, "exit") == 0){
//		pthread_mutex_lock(&printLock);
		sem_wait(&printLock);
		printf("exit (Thread %d)\n", i);
		printf("Thread %d done.\n", i);
		sem_post(&printLock);
//		pthread_mutex_unlock(&printLock);
		fprintf(tracefile, "exit\n");
		fprintf(tracefile, "Thread %d done.\n", i);
		exitFile(fileID);		
	}
	fclose(tracefile);
	fclose(filelist);
	return;
}

//---------------------------------cpin: size <= 512 and size > 512------------------------------------
void cpin(char* file, FILE* tracefile, int threadnum){
	int startBlk = 0, endBlk = 0, i, j, size, blk, dircode;
	FILE* pFile = fopen(file, "rb");
	if(currinodeFlag ==TRUE){
		findInode(inumber - 1, threadnum);
		currinodeFlag = FALSE;
	}
/*	printf("current Directory 1: \n");
	for(i = 0; i < DIRENTRYNUM; i++){
		printf("dircode = %d, dirname = %s\n", currDir[threadnum]->dirlist[i].dircode, currDir[threadnum]->dirlist[i].dirname);
	}*/

	startBlk = currInode[threadnum]->addr[0];
	for(i = 1; i < ADDRNUM; i++){
		if(currInode[threadnum]->addr[i] == 0){
			endBlk = currInode[threadnum]->addr[i - 1];
			break;
		}
	}
//	printf("inumber = %d, start = %d, end = %d\n", inumber - 1, startBlk, endBlk);
	if(endBlk == 0)
		endBlk = currInode[threadnum]->addr[ADDRNUM - 1];
	size = (int)fileSize(pFile);
	
	fprintf(tracefile, "\tSize: %d byte\n", currInode[threadnum]->size1);
	
	fseek(pFile,0,SEEK_SET);//pFile is set to the end of the file¡£need to use fseek(fs,0,SEEK_SET) to set it to the beginning of the file
/*	printf("current Directory 1: \n");
	for(i = 0; i < DIRENTRYNUM; i++){
		printf("dircode = %d, dirname = %s\n", currDir[threadnum]->dirlist[i].dircode, currDir[threadnum]->dirlist[i].dirname);
	}*/
	if(startBlk == endBlk){
		fread(sys.blocknode[startBlk].space, size, 1, pFile);
		fprintf(tracefile, "\tBlock: %d\n", startBlk);
	}
	else{
		blk= startBlk;
		fprintf(tracefile, "\tBlock: %d - %d\n", endBlk, startBlk);
		for(i = blk; i >= endBlk; i-- ){	
			if(i != endBlk){
				fread(sys.blocknode[i].space, BLOCKSIZE, 1, pFile);
			}
			else{
				fread(sys.blocknode[i].space, size - BLOCKSIZE * (startBlk - endBlk), 1, pFile);
			}
		}
	}
	dircode = currDir[threadnum]->dirlist[SELFDIR].dircode;
	fprintf(tracefile, "\ti-number: %d\n", inumber - 1);
	findInode(dircode, threadnum);
	currInode[threadnum]->size1++;	
	fclose(pFile);
	return;
}

//---------------------------------allocate inode, directory entry, inumber, and blocks to new files------------------------------------
int allocateFile(char* file1, char* file2, int threadnum, FILE* tracefile){
	unsigned short size, blkNum, i; 
	FILE* pFile = fopen(file1, "rb");
	size = (unsigned short)(fileSize(pFile));
	if(size > BLOCKSIZE * ADDRNUM){
//		pthread_mutex_lock(&printLock);
		sem_wait(&printLock);
		printf("ERROR: file size > 4k (Thread %d)\n", threadnum);
		sem_post(&printLock);
//		pthread_mutex_unlock(&printLock);
		fprintf(tracefile, "\tfile size > 4k\n");
		fclose(pFile);
		return FALSE;
	}
	blkNum = ((int)size) / BLOCKSIZE + 1;  //how many blocks will this file take
	findInode(inumber, threadnum);
	//*****************set the inode for the new file*****************
	if(nextBlock - blkNum > firstDataBlock - 1){
		currInode[threadnum]->flags = ALLO_FLAG | READ_FLAG | WRITE_FLAG | EXE_FLAG;
		currInode[threadnum]->nlinks = '0';
		currInode[threadnum]->uid = '0';
		currInode[threadnum]->gid = '0';
		currInode[threadnum]->size0 = '0';
		currInode[threadnum]->size1 = size;
		for(i = 0; i < ADDRNUM && i < blkNum; i++){
			currInode[threadnum]->addr[i] = nextBlock;
			nextBlock--;
		}
		//*****************update free[] in the superblock. Actually the free[] is useless if we keep tracking the nextBlock*****************
		updateFree(TRUE, blkNum);
	}
	else{
//		pthread_mutex_lock(&printLock);
		sem_wait(&printLock);
		printf("ERROR: No free blocks. (Thread %d)\n", threadnum);
		sem_post(&printLock);
//		pthread_mutex_unlock(&printLock);
		fprintf(tracefile, "\tNo free blocks.\n");
		fclose(pFile);
		return FALSE;
	}

	//*****************create new directory entry for this new file*****************
	i = multiBlockDir(threadnum);
	if (i >= 0){
		currDir[threadnum]->dirlist[i].dircode = inumber;
		strcpy(currDir[threadnum]->dirlist[i].dirname, file2);

		inumber++;
		fclose(pFile);
		return TRUE;
	}
	else{
		fprintf(tracefile, "The directory %d is full!\n", currDir[i]->dirlist[SELFDIR].dircode);
		fclose(pFile);
		return FALSE;
	}
}

void mkdir(char* dirname, FILE* tracefile, int threadnum){
	int parentnum, i = 0, dircode;
//	char temp[ENTRYNAME];

	//*****************allocate a new inode to the new directory*****************
	findInode(inumber, threadnum);
	if(nextBlock > firstDataBlock){
		currInode[threadnum]->flags = ALLO_FLAG | DIR_FLAG | READ_FLAG | WRITE_FLAG | EXE_FLAG | GRWE_FLAG | ORWE_FLAG;
		currInode[threadnum]->uid = '0';
		currInode[threadnum]->gid = '0';
		currInode[threadnum]->size0 = '0';
		currInode[threadnum]->size1 = 2;
		currInode[threadnum]->addr[0] = nextBlock;
		parentnum = currDir[threadnum]->dirlist[SELFDIR].dircode;
		//*****************initialize the block of the new directory*****************
		initNewdir(nextBlock, inumber, parentnum);
		nextBlock--;
		fprintf(tracefile, "\tBlock: %d\n", nextBlock + 1);
		//*****************update free[] in the superblock. Actually the free[] is useless if we keep tracking the nextBlock*****************
		updateFree(FALSE, 0);

		//*****************add the new directory entry to current directory*****************
		i = multiBlockDir(threadnum);

		if(i >= 0){
			currDir[threadnum]->dirlist[i].dircode = inumber;
			fprintf(tracefile, "\ti-number: %d\n", currDir[threadnum]->dirlist[i].dircode);
			strcpy(currDir[threadnum]->dirlist[i].dirname, dirname);

			dircode = currDir[threadnum]->dirlist[SELFDIR].dircode;
			findInode(dircode, threadnum);
			currInode[threadnum]->size1++;
			inumber++;
		}
		else{
			fprintf(tracefile, "The directory %d is full!\n", currDir[i]->dirlist[SELFDIR].dircode);
		}
	}
	else{
//		pthread_mutex_lock(&printLock);
		sem_wait(&printLock);
		printf("ERROR: No free blocks. (Thread %d)\n ", threadnum);
		sem_post(&printLock);
//		pthread_mutex_unlock(&printLock);
		fprintf(tracefile, "\tNo free blocks.\n");
	}
	return;
}

void cd(char* dirname, FILE* tracefile, int threadnum){
	int exist, listpos, nodepos, blk;

	exist = searchDir(dirname, threadnum);
	if(exist > 0){
		listpos = exist / (BLOCKSIZE / sizeof(struct inode));  //list and node is the node position of this file in the ilist
		listpos += 2;
		nodepos = exist % (BLOCKSIZE / sizeof(struct inode));
		if(nodepos == 0){
			listpos--;
			nodepos = INODENUM - 1;
		}
		else
			nodepos--;
		blk = currInode[threadnum]->addr[0];
		currDir[threadnum] = (struct directory*)sys.blocknode[blk].space;
		if(tracefile != NULL){
//			pthread_mutex_lock(&printLock);
			sem_wait(&printLock);
			printDir(tracefile, threadnum, FALSE);
		}
	}
	else{
//		pthread_mutex_lock(&printLock);
		sem_wait(&printLock);
		printf("ERROR: Invalid directory! (Thread %d)\n", threadnum);
		sem_post(&printLock);
//		pthread_mutex_unlock(&printLock);
		if(tracefile != NULL){
			fprintf(tracefile, "\tInvalid directory!\n");
		}
	}	
	return;
}

void cpout(char* outputfile, FILE* tracefile, int threadnum){
	int start, end = 0, size, i, oristart, orisize;
	FILE* pFile;

	start = currInode[threadnum]->addr[0];
	oristart = start;
	for(i = 1; i < ADDRNUM; i++){
/*		printf("///////////\n");
		printf("addr[%d] = %d\n", i-1, currInode[threadnum]->addr[i-1]);
		printf("///////////\n");*/
		if(currInode[threadnum]->addr[i] == 0){
			end = currInode[threadnum]->addr[i - 1];
			break;
		}
	}
//	printf("end = %d\n", end);
	if(end == 0)
		end = currInode[threadnum]->addr[ADDRNUM - 1];
	size = currInode[threadnum]->size1;
	orisize = size;

	if(strcmp(outputfile, "stdout") != 0){
//		pthread_mutex_unlock(&printLock);
		sem_post(&printLock);
		pFile = fopen(outputfile, "wb");		
		if(start == end){
			fprintf(tracefile, "\tSource FIle Size: %d byte\n", size);
			fprintf(tracefile, "\tSource Block: %d\n", start);			
			fwrite(sys.blocknode[start].space, size, 1, pFile);
		}
		else{
			fprintf(tracefile, "\tSource FIle Size: %d byte\n", size);
			fprintf(tracefile, "\tSource Block: %d - %d\n", end, start);			
			while(start != end){
				fwrite(sys.blocknode[start].space, BLOCKSIZE, 1, pFile);
				start--;
				size -= BLOCKSIZE; 
			}
			fwrite(sys.blocknode[start].space, size, 1, pFile);
		}
		fclose(pFile);
	}
	else{
		if(start == end){
			fwrite(sys.blocknode[start].space, size, 1, stdout);
			printf("\n");
			sem_post(&printLock);
//			pthread_mutex_unlock(&printLock);
			fprintf(tracefile, "\tSource FIle Size: %d byte\n", size);
			fprintf(tracefile, "\tSource Block: %d\n", start);			
		}
		else{
			while(start != end){
				fwrite(sys.blocknode[start].space, BLOCKSIZE, 1, stdout);
				start--;
				size -= BLOCKSIZE; 
			}
			fwrite(sys.blocknode[start].space, size, 1, stdout);
			printf("\n");
			sem_post(&printLock);
//			pthread_mutex_unlock(&printLock);
			fprintf(tracefile, "\tSource FIle Size: %d byte\n", orisize);
			fprintf(tracefile, "\tSource Block: %d - %d\n", end, oristart);			
		}
	}
	return;
}

void v6chmod(FILE* tracefile, int mode, int threadnum){
	fprintf(tracefile, "\tFlags before chmod: %o\n", currInode[threadnum]->flags);
	currInode[threadnum]->flags = (currInode[threadnum]->flags / 010000 * 010000) | mode;
	fprintf(tracefile, "\tFlags after chmod: %o\n", currInode[threadnum]->flags);
	return;
}

void ls(FILE* tracefile, int threadnum){
	printDir(tracefile, threadnum, TRUE);
	return;
}

void mv(char* oldname, char* newname, FILE* tracefile, int threadnum){
	int i;
	fprintf(tracefile, "\tFile name before mv:\n");
//	pthread_mutex_lock(&printLock);
	sem_wait(&printLock);
	printDir(tracefile, threadnum, FALSE);
	for (i = 0; i < DIRENTRYNUM; i++){
		if(strcmp(currDir[threadnum]->dirlist[i].dirname, oldname) == 0){
			memset(currDir[threadnum]->dirlist[i].dirname, 0, sizeof(char) * ENTRYNAME);
			strcpy(currDir[threadnum]->dirlist[i].dirname, newname);
		}
	}
	fprintf(tracefile, "\tFile name after mv:\n");
//	pthread_mutex_lock(&printLock);
	sem_wait(&printLock);
	printDir(tracefile, threadnum, FALSE);
	return;
}

void pwd(FILE* tracefile, int threadnum){
	char pathname[1000];
	int inum, parentnum;
	inum = currDir[threadnum]->dirlist[SELFDIR].dircode;
	parentnum = currDir[threadnum]->dirlist[PARENTDIR].dircode;
	strcpy(pathname, "/");
	if(parentnum != -1)
		dirPath(inum, parentnum, pathname);
	printf("\t%s\n", pathname);
	sem_post(&printLock);
//	pthread_mutex_unlock(&printLock);
	fprintf(tracefile, "\t%s\n", pathname);
	
}

void dirPath(int inum, int parentnum, char* pathstr){
	int listpos, nodepos, i, newinum, newparentnum, blk, j;
	char slash[] = "/";
	struct inode* node;
	struct directory* dir;

	listpos =  parentnum / (BLOCKSIZE / sizeof(struct inode)); 
	listpos += 2;
	nodepos =  parentnum % (BLOCKSIZE / sizeof(struct inode));
	if(nodepos == 0){
		listpos--;
		nodepos = INODENUM - 1;
	}
	else
		nodepos--;
	node = &(((struct freelist*)sys.blocknode[listpos].space)->node[nodepos]);

	for(j = 0; j < ADDRNUM && node->addr[j] != 0; j++){
		strcpy(pathstr, "/");
		blk = node->addr[j];	
		dir =	(struct directory*)sys.blocknode[blk].space;

		newinum = ((struct directory*)sys.blocknode[blk].space)->dirlist[SELFDIR].dircode;
		newparentnum = ((struct directory*)sys.blocknode[blk].space)->dirlist[PARENTDIR].dircode;
		if(newinum != ROOT_I_NUM){
			dirPath(newinum, newparentnum, pathstr);

			strcat(pathstr, slash);
		}
		for(i = 2; i < DIRENTRYNUM; i++){
			if(((struct directory*)sys.blocknode[blk].space)->dirlist[i].dircode == inum){
				strcat(pathstr, ((struct directory*)sys.blocknode[blk].space)->dirlist[i].dirname);
				break;
			}
		}
	}
	return;
}

void find(char* file, FILE* tracefile, int threadnum){
	int tempDirBlk, oriDirBlk;	
	//*****************store the currDir inumber into a temp int*****************/
	//*****************Set currDir to Root dir*****************/
	//*****************After the finding completed, restore the currDir using the tempDir*****************/
	tempDirBlk = currDir[threadnum]->dirlist[SELFDIR].dircode;	
	currDir[threadnum] = (struct directory*)sys.blocknode[ROOTBLOCK].space;
	filePath(file, tracefile, ROOT_I_NUM, threadnum);
	if(findFlag == FALSE){
		printf("Can't find file %s.\n", file);
		fprintf(tracefile, "\tCan't find file %s.\n", file);	
	}
	findFlag = FALSE;
//	pthread_mutex_unlock(&printLock);
	sem_post(&printLock);
	findInode(tempDirBlk, threadnum);
	oriDirBlk = currInode[threadnum]->addr[0];
	currDir[threadnum] = (struct directory*)sys.blocknode[oriDirBlk].space;
	return;
}

void exitFile(void* t){
	pthread_exit(t);
	return;
}
void filePath(char* filename, FILE* tracefile, int blknum, int threadnum){
	int i, flag, j, tempBlk;
	int tempAddr[ADDRNUM];
	char path[1000];
	char parent[] = "..";
	struct directory* dirP;

	findInode(blknum, threadnum);
	for(i = 0; i < ADDRNUM; i++)
		tempAddr[i] = currInode[threadnum]->addr[i];
	for(j = 0; j < ADDRNUM && tempAddr[j] != 0; j++){
		tempBlk = tempAddr[j];
		dirP = (struct directory*)sys.blocknode[tempBlk].space;
		for(i = 2; dirP->dirlist[i].dircode != 0 && i < DIRENTRYNUM; i++){
			findInode(dirP->dirlist[i].dircode, threadnum);
			flag = currInode[threadnum]->flags;
			if(strcmp(dirP->dirlist[i].dirname, filename) == 0){			
				if(flag / 010000 == 016 || flag / 010000 == 010){
					findInode(currDir[threadnum]->dirlist[SELFDIR].dircode, threadnum);
					printFilepath(dirP->dirlist[i].dircode, currInode[threadnum]->addr[0], filename, path, tracefile);
					findFlag = TRUE;
				}
			}
			else{
				if(flag / 010000 == 014 && dirP->dirlist[i].dircode != 1){
					cd(dirP->dirlist[i].dirname, NULL, threadnum);
					filePath(filename, tracefile, currDir[threadnum]->dirlist[SELFDIR].dircode, threadnum);
				}
			}		
		}
	}
	if(dirP->dirlist[PARENTDIR].dircode > 0){
		cd(parent, NULL, threadnum);
		return;
	}
	else{
		return;
	}
}

void printFilepath(int fileinum, int blknum, char* filename, char* path, FILE* tracefile){
	int inum, parentnum;
	inum = ((struct directory*)sys.blocknode[blknum].space)->dirlist[SELFDIR].dircode;
	parentnum = ((struct directory*)sys.blocknode[blknum].space)->dirlist[PARENTDIR].dircode;
	strcpy(path, "/");
	if(parentnum > 0){
		dirPath(inum, parentnum, path);
		strcat(path, "/");
	}
	strcat(path, filename);
	printf("\t%s\n", path);
	fprintf(tracefile, "\t%s\n", path);
	return;
}

void makedisk(){
	int i;
	char name[] = "disk";
	FILE* pFile = fopen(name, "wb");
	for(i = 0; i < BLOCKNUM; i++){
		fwrite(sys.blocknode[i].space, BLOCKSIZE, 1, pFile);
//		fprintf(pFile, "\n=======================================\n");
	}
	fclose(pFile);
	return;
}

void load(){
	int i, j, inumFlag = FALSE;
	char name[] = "disk";
	FILE* pFile = fopen(name, "rb");

	//*****************initialize the system, allocate 512*1000 bytes*****************
	for(i = 0; i < BLOCKNUM; i++){
		fread(sys.blocknode[i].space, BLOCKSIZE, 1, pFile);
//		printf("%d\n", i);
		
	}
	//*****************initialize superblock*****************

	 //-------------------------------------test------------------------------
	printf("superblock freelist\n");
	for(i = 0; i < NFREE; i++){
		printf("%d ", ((struct superblock*)sys.blocknode[SUPER].space)->free[i]);
	}
	//*****************initialize ilist*****************
	inumber = 0;
	for(i = ROOT; i < ROOT + ISIZE; i++){
		for(j = 0; j < INODENUM; j++){
			if( ((struct freelist*)sys.blocknode[i].space)->node[j].size1 != 0 ){
				
				inumber++;
			}
			else{
				inumFlag = TRUE;
				break;
			}
		}
		if(inumFlag == TRUE)
			break;
	}
/*	printf("inumber=%d\n", inumber);
	system("pause");*/
	nextBlock = ((struct superblock*)sys.blocknode[SUPER].space)->free[((struct superblock*)sys.blocknode[SUPER].space)->nfree - 1];
	 //-------------------------------------test------------------------------
/*	printf("\nilist:\n");
	printf("i = %d\n", i);
	for(j = 0; j < INODENUM; j++){
		printf("j = %d\n", j);
		printf("flag = %o\n",((struct freelist*)sys.blocknode[i].space)->node[j].flags);
		printf("nlinks = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].nlinks);
		printf("uid = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].uid);
		printf("gid = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].gid);
		printf("size0 = %c\n",((struct freelist*)sys.blocknode[i].space)->node[j].size0);
		printf("size1 = %d\n",((struct freelist*)sys.blocknode[i].space)->node[j].size1);
		printf("address = ");
		for(k = 0; ((struct freelist*)sys.blocknode[i].space)->node[j].addr[k] != 0; k++){
			printf("%d ",((struct freelist*)sys.blocknode[i].space)->node[j].addr[k] );
		}
		printf("\n");
		printf("====================================================\n");
		getchar();
	}*/

	//*****************initialize Block42~51 *****************
	firstDataBlock = FIRSTDATABLOCK;
	fclose(pFile);
	//-------------------------------------test------------------------------
	printf("free blocks\n");
	for(i = 42; i < FIRSTDATABLOCK; i++){
		for(j = 0; j < NFREE+1; j++)
			printf("%d ", ((struct blocklist*)sys.blocknode[i].space)->freeblock[j]);
		printf("\n============================================\n");
	}
	printf("first data block = %d", firstDataBlock);
	printf("\n");
	for (i = 0; i < DIRENTRYNUM; i++){
		if(((struct directory*)sys.blocknode[ROOTBLOCK].space)->dirlist[i].dircode > 0)
			printf("i-number: %d dirname: %s\n", ((struct directory*)sys.blocknode[ROOTBLOCK].space)->dirlist[i].dircode, ((struct directory*)sys.blocknode[ROOTBLOCK].space)->dirlist[i].dirname);
	}
}

long fileSize(FILE* p){
	fseek(p, 0L, SEEK_END);
	return ftell(p);
}

int searchFile(char* filename, int threadnum){
	int i, j, listpos, nodepos, flag, tempBlk;
	findInode(currDir[threadnum]->dirlist[SELFDIR].dircode, threadnum);
	for(j = 0; j < ADDRNUM && currInode[threadnum]->addr[j] != 0; j++){
		tempBlk = currInode[threadnum]->addr[j];
		currDir[threadnum] = (struct directory*)sys.blocknode[tempBlk].space;

		for(i = 0; i < DIRENTRYNUM && (currDir[threadnum])->dirlist[i].dircode != 0 && 
			strcmp(currDir[threadnum]->dirlist[i].dirname, "") != 0; i++){
			if(strcmp((currDir[threadnum])->dirlist[i].dirname, filename) == 0){
				listpos = currDir[threadnum]->dirlist[i].dircode / (BLOCKSIZE / sizeof(struct inode));  //list and node is the node position of this file in the ilist
				listpos += 2;
				nodepos = currDir[threadnum]->dirlist[i].dircode % (BLOCKSIZE / sizeof(struct inode));
				if(nodepos == 0){
					listpos--;
					nodepos = INODENUM - 1;
				}
				else
					nodepos--;
				flag = ((struct freelist*)sys.blocknode[listpos].space)->node[nodepos].flags;
				if(flag / 010000 == 016 || flag / 010000 == 010){
					currInode[threadnum] = &(((struct freelist*)sys.blocknode[listpos].space)->node[nodepos]);
					return currDir[threadnum]->dirlist[i].dircode;	
				}
			}
		}
	}	
	return 0;
}

int searchDir(char* dirname, int threadnum){
	int i, j, listpos, nodepos, flag, tempBlk;

	findInode(currDir[threadnum]->dirlist[SELFDIR].dircode, threadnum);
	for(j = 0; j < ADDRNUM && currInode[threadnum]->addr[j] != 0; j++){
		tempBlk = currInode[threadnum]->addr[j];
		currDir[threadnum] = (struct directory*)sys.blocknode[tempBlk].space;

		for(i = 0; i < DIRENTRYNUM && currDir[threadnum]->dirlist[i].dircode != 0 && 
			strcmp(currDir[threadnum]->dirlist[i].dirname, "") != 0; i++){
			if(strcmp(currDir[threadnum]->dirlist[i].dirname, dirname) == 0){
				listpos =  currDir[threadnum]->dirlist[i].dircode / (BLOCKSIZE / sizeof(struct inode));  //list and node is the node position of this file in the ilist
				listpos += 2;
				nodepos =  currDir[threadnum]->dirlist[i].dircode % (BLOCKSIZE / sizeof(struct inode));
				if(nodepos == 0){
					listpos--;
					nodepos = INODENUM - 1;
				}
				else
					nodepos--;
				flag = ((struct freelist*)sys.blocknode[listpos].space)->node[nodepos].flags;
				if(flag / 010000 == 014){
					currInode[threadnum] = &(((struct freelist*)sys.blocknode[listpos].space)->node[nodepos]);
					return currDir[threadnum]->dirlist[i].dircode;			
				}
			}
		}
	}
	return 0;
}

void findInode(int num, int threadnum){
	int listpos, nodepos;
	listpos =  num / (BLOCKSIZE / sizeof(struct inode)); 
	listpos += 2;
	nodepos =  num % (BLOCKSIZE / sizeof(struct inode));
	if(nodepos == 0){
		listpos--;
		nodepos = INODENUM - 1;
	}
	else
		nodepos--;
	currInode[threadnum] = &(((struct freelist*)sys.blocknode[listpos].space)->node[nodepos]);
	return;
}

void printFile(int start, int end, int size, char* name){
	FILE* pFile = fopen(name, "wb");
	if(start == end)
		fwrite(sys.blocknode[start].space, size, 1, pFile);	//write the string into the .txt file
	else{
		while(start != end){
			fwrite(sys.blocknode[start].space, BLOCKSIZE, 1, pFile);
			start--;
			size -= BLOCKSIZE; 
		}
		fwrite(sys.blocknode[start].space, size, 1, pFile);
	}
	fclose(pFile);
	return;
}

void printDir(FILE* tracefile, int threadnum, int printStdout){
	int i, j, tempBlk;
	findInode(currDir[threadnum]->dirlist[SELFDIR].dircode, threadnum);
	for(j = 0; j < ADDRNUM && currInode[threadnum]->addr[j] != 0; j++){
		tempBlk = currInode[threadnum]->addr[j];
		currDir[threadnum] = (struct directory*)sys.blocknode[tempBlk].space;
		if(printStdout == TRUE){
			printf("current Dir: \n");
			for (i = 0; i < DIRENTRYNUM; i++){
				if(currDir[threadnum]->dirlist[i].dircode != 0 && currDir[threadnum]->dirlist[i].dircode <= inumber
					&& strcmp(currDir[threadnum]->dirlist[i].dirname, "") != 0)
					printf("\ti-number: %d dirname: %s\n", currDir[threadnum]->dirlist[i].dircode, currDir[threadnum]->dirlist[i].dirname);
			}
		}
		sem_post(&printLock);
//		pthread_mutex_unlock(&printLock);
		fprintf(tracefile, "\tcurrent Dir:\n");	
		for (i = 0; i < DIRENTRYNUM; i++){
			if(currDir[threadnum]->dirlist[i].dircode != 0 && currDir[threadnum]->dirlist[i].dircode <= inumber
				&& strcmp(currDir[threadnum]->dirlist[i].dirname, "") != 0){
				fprintf(tracefile, "\t\ti-number: %d dirname: %s\n", currDir[threadnum]->dirlist[i].dircode, currDir[threadnum]->dirlist[i].dirname);
			}
		}
	}
	return;
}

void updateFree(int isFile, int blkNum){ //if isFile = 1, update free array for file; if isFile = 0, update free array for dir
	int nextList, i;
	if(nextBlock < ((struct superblock*)sys.blocknode[SUPER].space)->free[1]){
		nextList = ((struct superblock*)sys.blocknode[SUPER].space)->free[0];
		for(i = 0; i < NFREE; i++){
			if(i == 0)
				((struct superblock*)sys.blocknode[SUPER].space)->free[i] = ((struct blocklist*)sys.blocknode[nextList].space)->freeblock[i];
		else
				((struct superblock*)sys.blocknode[SUPER].space)->free[i] = ((struct blocklist*)sys.blocknode[nextList].space)->freeblock[i + 1];
		}
		if(isFile == TRUE)
			((struct superblock*)sys.blocknode[SUPER].space)->nfree = (NFREE - 1) - (blkNum - ((struct superblock*)sys.blocknode[SUPER].space)->nfree);
		else
			((struct superblock*)sys.blocknode[SUPER].space)->nfree = NFREE - 1;
	}
	else{
		if(isFile == TRUE)
			((struct superblock*)sys.blocknode[SUPER].space)->nfree = ((struct superblock*)sys.blocknode[SUPER].space)->nfree - blkNum;
		else
			((struct superblock*)sys.blocknode[SUPER].space)->nfree = ((struct superblock*)sys.blocknode[SUPER].space)->nfree - 1;
	}
	return;
}

int multiBlockDir(int threadnum){
	int tempBlk, i = 0, j;
	if(currDir[threadnum]->dirlist[DIRENTRYNUM - 1].dircode == 0 || strcmp(currDir[threadnum]->dirlist[DIRENTRYNUM - 1].dirname, "") == 0){
		for(i = 0; i < DIRENTRYNUM; i++){
			if(currDir[threadnum]->dirlist[i].dircode == 0 || strcmp(currDir[threadnum]->dirlist[i].dirname, "") == 0){
				break;
			}
		}
	}
	else{
		findInode(currDir[threadnum]->dirlist[SELFDIR].dircode, threadnum);
		currinodeFlag = TRUE;
		if(currInode[threadnum]->addr[ADDRNUM - 1] != 0){//if addr[7] != 0, check if there are any empty entries
			tempBlk = currInode[threadnum]->addr[ADDRNUM - 1];
			currDir[threadnum] = (struct directory*)sys.blocknode[tempBlk].space;
			if(currDir[threadnum]->dirlist[DIRENTRYNUM - 1].dircode != 0 &&
				strcmp(currDir[threadnum]->dirlist[DIRENTRYNUM - 1].dirname, "") != 0){// no empty entries in addr[7], which means the dir is full
//				pthread_mutex_lock(&printLock);
				sem_wait(&printLock);
				printf("ERROR: The directory %d is full! (Thread %d)\n", currDir[threadnum]->dirlist[SELFDIR].dircode, threadnum);
				sem_post(&printLock);
//				pthread_mutex_unlock(&printLock);
				return -1;
			}
		}
		else{
			for(j = 0; j < ADDRNUM; j++){
				tempBlk = currInode[threadnum]->addr[j];
				if(tempBlk == 0){
					//*****************allocate one more block to current directory and initialize the new block*****************
					currInode[threadnum]->addr[j] = nextBlock;
					tempBlk = currInode[threadnum]->addr[j];
					initNewdir(nextBlock, currDir[threadnum]->dirlist[SELFDIR].dircode, currDir[threadnum]->dirlist[PARENTDIR].dircode);
					currDir[threadnum] = (struct directory*)sys.blocknode[tempBlk].space;
					for(i = 0; i < DIRENTRYNUM; i++){
						if(currDir[threadnum]->dirlist[i].dircode == 0 || strcmp(currDir[threadnum]->dirlist[i].dirname, "") == 0){
							break;
						}
					}
					nextBlock--;						
					updateFree(FALSE, 0);
				}	
				if(currDir[threadnum]->dirlist[i].dircode == 0 || strcmp(currDir[threadnum]->dirlist[i].dirname, "") == 0)							
					break;
			}
		}
	}
	return i;
}