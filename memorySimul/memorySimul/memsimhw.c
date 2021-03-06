//
// Virual Memory Simulator Homework
// One-level page table system with FIFO and LRU
// Two-level page table system with LRU
// Inverted page table with a hashing system 
// Submission Year:
// Student Name:
// Student Number:
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes

struct pageTableEntry {
	int level;				// page table level (1 or 2)
	char valid;
	struct pageTableEntry *secondLevelPageTable;	// valid if this entry is for the first level page table (level = 1)
	unsigned int frameNumber;								// valid if this entry is for the second level page table (level = 2)
};

struct invertedPageTableEntry {
	int pid;					// process id
	int virtualPageNumber;		// virtual page number
	int frameNumber;			// frame number allocated
	struct invertedPageTableEntry *next;
	struct invertedPageTableEntry *pre;
};

struct framePage {
	unsigned int number;			// frame number
	int pid;			// Process id that owns the frame
	unsigned int virtualPageNumber;			// virtual page number using the frame
	struct framePage *lruLeft;	// for LRU circular doubly linked list
	struct framePage *lruRight; // for LRU circular doubly linked list
	struct invertedPageTableEntry * invertedFrame;
};

struct procEntry {
	char *traceName;			// the memory trace name
	int pid;					// process (trace) id
	int ntraces;				// the number of memory traces
	int num2ndLevelPageTable;	// The 2nd level page created(allocated);
	int numIHTConflictAccess; 	// The number of Inverted Hash Table Conflict Accesses
	int numIHTNULLAccess;		// The number of Empty Inverted Hash Table Accesses
	int numIHTNonNULLAcess;		// The number of Non Empty Inverted Hash Table Accesses
	int numPageFault;			// The number of page faults
	int numPageHit;				// The number of page hits
	struct pageTableEntry *firstLevelPageTable;
	FILE *tracefp;
};


struct framePage *oldestFrame; // the oldest frame pointer

struct pageTableEntry **PageEntry;//one-level의 virtual memory = process수 * 2^20엔트리

int simType, firstLevelBits, phyMemSizeBits, numProcess;
int s_flag = 0;
unsigned int Vaddr;
unsigned int Paddr;
unsigned int phyFrameNum;

void initPageTable(struct pageTableEntry ***PageEntry, int numProcess,unsigned int size) {
	int i,j;
	*PageEntry = (struct pageTableEntry**)malloc(sizeof(struct pageTableEntry*)*numProcess);
	for (i = 0; i < numProcess; i++) {
		*(*PageEntry+i) = (struct pageTableEntry*)calloc(size,sizeof(struct pageTableEntry));
	}
}

void FreePageTable(struct pageTableEntry ***PageEntry, int numProcess) {
	int i, j;
	for (i = 0; i < numProcess; i++) {
		free(*(*PageEntry + i));
	}
	free(*PageEntry);
}

void initPhyMem(struct framePage *phyMem, int nFrame) {
	int i;
	for(i = 0; i < nFrame; i++) {
		phyMem[i].number = i;
		phyMem[i].pid = -1;
		phyMem[i].virtualPageNumber = -1;
		phyMem[i].invertedFrame = NULL;
		phyMem[i].lruLeft = &phyMem[(i-1+nFrame) % nFrame];
		phyMem[i].lruRight = &phyMem[(i+1+nFrame) % nFrame];
	}

	oldestFrame = &phyMem[0];

}
void initializeProc(struct procEntry * procTable) {
	int i;
	// initialize procTable for the simulation
	for (i = 0; i < numProcess; i++) {
		// initialize procTable fields
		// rewind tracefilesa
		procTable[i].pid = i;
		procTable[i].ntraces = 0;				// the number of memory traces
		procTable[i].num2ndLevelPageTable = 0;	// The 2nd level page created(allocated);
		procTable[i].numIHTConflictAccess = 0; 	// The number of Inverted Hash Table Conflict Accesses
		procTable[i].numIHTNULLAccess = 0;		// The number of Empty Inverted Hash Table Accesses
		procTable[i].numIHTNonNULLAcess = 0;		// The number of Non Empty Inverted Hash Table Accesses
		procTable[i].numPageFault = 0;			// The number of page faults
		procTable[i].numPageHit = 0;

		rewind(procTable[i].tracefp);
	}
}

//one-level만 고려했을떄 fifo
/*void fifo(struct framePage *first, unsigned int newPageNum,int newPid) {
	int outVaddr =  first->virtualPageNumber; //비워줄 프레임이 원래 담고 있던 Virtual addr
	int outPid = first->pid;				//비워줄 프레임이 원래 담고있단 pid
	int inframNumber = first->number;		//바꿔줄 프레임의 물리번호
	//원래 first가 가르키던 PageEntry는 무효화시킨다.
	PageEntry[outPid][outVaddr].valid = 0;

	//새로 들어올 예정인 Virtual 주소에 first framNumber를 배정해준다.
	PageEntry[newPid][newPageNum].valid = 1;
	PageEntry[newPid][newPageNum].frameNumber = inframNumber;
	//first frame에 새로운 vitual을 배정해준다.
	first->pid = newPid;
	first->virtualPageNumber = newPageNum;
}*/

//two-level도 고려한 fifo
void fifo(struct pageTableEntry * outPET, struct pageTableEntry * newPET,struct framePage *first,int newPid,int newPageNum) {
	//원래 first가 가르키던 PageEntry는 무효화시킨다.
	outPET->valid = 0;
	//새로 들어올 예정인 Virtual 주소에 first framNumber를 배정해준다.
	newPET->valid = 1;
	newPET->frameNumber = first->number;
	//first frame에 새로운 vitual을 배정해준다.
	first->pid = newPid;
	first->virtualPageNumber = newPageNum;
}



void oneLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, int FIFOorLRU) {
	initializeProc(procTable);
	initPageTable(&PageEntry, numProcess,(1<<20));
	//initPhyMem(phyMemFrames, phyFrameNum);
	int i;
	unsigned int phyFrameCount = 0;
	//FiFo 방식은 first만 사용
	struct framePage * first = &phyMemFrames[0];
	//LRU는 새로운 addr을 읽을때마다 last가 변한다. 만약 새로부른게 first일 수 있으므로 확인후 갱신이 필요!
	struct framePage * last =&phyMemFrames[0];
	// -s option print statement
		char gar; //W or R 
		unsigned int PageNum;
		while(!feof(procTable[numProcess - 1].tracefp)){
			for (i = 0; i < numProcess; i++) {
				fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &gar);
				if (feof(procTable[i].tracefp))continue;
				Paddr = Vaddr & 0x00000FFF; // 물리주소 부분
				PageNum = Vaddr >> 12; //가상페이지의 넘버
				procTable[i].ntraces++;
				
				// Page Fault
				if (PageEntry[i][PageNum].valid != 1) {
					
					procTable[i].numPageFault++;
					//Phy Frame에 아직 공간이 남아있으면..
					if(phyFrameCount < phyFrameNum) {
						PageEntry[i][PageNum].valid = 1;
						PageEntry[i][PageNum].frameNumber = phyMemFrames[phyFrameCount].number;
						phyMemFrames[phyFrameCount].virtualPageNumber = PageNum;
						last = &phyMemFrames[phyFrameCount];
						phyMemFrames[phyFrameCount++].pid = i;
					}
					//피지컬 메모리가 꽉찬 상태의 page fault 처리 부분
					else {
						//fifo(first, PageNum, i);
						fifo(&PageEntry[first->pid][first->virtualPageNumber], &PageEntry[i][PageNum],first,i,PageNum);
						if (!FIFOorLRU) { first = first->lruRight; }	//FIFO 방식 그다음 아웃될 first는 다음으로 간다.
						else {
							//꽉찼고, 원형이기때문에 앞으로 한칸씩만 밀면 LRU성립
							first = first->lruRight;
							last = last->lruRight;				
						}
					}
				}

				//HIT
				else {
					procTable[i].numPageHit++;
					//LRU 처리
					if (FIFOorLRU) {
						unsigned int curFrameN =  PageEntry[i][PageNum].frameNumber;
						
						//현재 hit된게 LRU의 first였다면 first를 그 다음으로 바꿔준다.
						if (first == &phyMemFrames[curFrameN]) {
							first = first->lruRight;
						}
						if (last != &phyMemFrames[curFrameN]) {

							//현재 hit된 frame은 last로 빠지기 위해 자신의 좌우를 서로 연결해준다.
							phyMemFrames[curFrameN].lruLeft->lruRight = phyMemFrames[curFrameN].lruRight;
							phyMemFrames[curFrameN].lruRight->lruLeft = phyMemFrames[curFrameN].lruLeft;

							//현재 frame의 좌우를 새로 갱신해준다.
							phyMemFrames[curFrameN].lruLeft = last; //현재 frame의 왼쪽이 last
							phyMemFrames[curFrameN].lruRight = last->lruRight; //현재 frame의 오른쪽은 last의 오른쪽

							//last와 last의 오른쪽노드의 정보를 현재 frame연결로 갱신하고 last도 갱신한다.
							last->lruRight->lruLeft = &phyMemFrames[curFrameN]; //last의 오른쪽의 왼쪾은 현재 frame
							last->lruRight = &phyMemFrames[curFrameN]; //last의 오른쪽은 현재 frame
							last = &phyMemFrames[curFrameN];			//last는 현재 frame
						}
					}
				}
				if (s_flag) {
					Paddr = (PageEntry[i][PageNum].frameNumber << 12) + Paddr;
					printf("One-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
				}
			}
		}

		
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		//assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
	FreePageTable(&PageEntry, numProcess);
}

void twoLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames) {
	initializeProc(procTable);
	initPageTable(&PageEntry, numProcess,(1<<firstLevelBits));
	initPhyMem(phyMemFrames, phyFrameNum);

	int i;
	unsigned int phyFrameCount = 0;
	struct framePage * first = &phyMemFrames[0];
	//LRU는 새로운 addr을 읽을때마다 last가 변한다. 만약 새로부른게 first일 수 있으므로 확인후 갱신이 필요!
	struct framePage * last = &phyMemFrames[0];
	char gar; //W or R 
	unsigned int firstbit;
	unsigned int secondbit;
	unsigned int PageNum;
	while (!feof(procTable[numProcess - 1].tracefp)) {
		for (i = 0; i < numProcess; i++) {
			fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &gar);
			if (feof(procTable[i].tracefp))continue;
			Paddr = Vaddr & 0x00000FFF; // 물리주소 부분
			PageNum = Vaddr >> 12; //가상페이지의 넘버
			firstbit = Vaddr >> (32 - firstLevelBits); //first level table number
			secondbit = (Vaddr << firstLevelBits) >> (firstLevelBits + 12); //second level table nubmer
			procTable[i].ntraces++;

			if (!PageEntry[i][firstbit].secondLevelPageTable) {
				unsigned int size = (1 << (20 - firstLevelBits));
				//printf("size: %d\n", size);
				PageEntry[i][firstbit].secondLevelPageTable = (struct pageTableEntry *)calloc((size), sizeof(struct pageTableEntry));
				procTable[i].num2ndLevelPageTable++;
			}
			// Page Fault
			if (PageEntry[i][firstbit].secondLevelPageTable[secondbit].valid != 1) {
				//printf("Page fault 도 먹고..!\n");
				procTable[i].numPageFault++;
				//Phy Frame에 아직 공간이 남아있으면..
				if (phyFrameCount < phyFrameNum) {
					PageEntry[i][firstbit].secondLevelPageTable[secondbit].valid = 1;
					PageEntry[i][firstbit].secondLevelPageTable[secondbit].frameNumber = phyMemFrames[phyFrameCount].number;
					phyMemFrames[phyFrameCount].virtualPageNumber = PageNum;
					last = &phyMemFrames[phyFrameCount];
					phyMemFrames[phyFrameCount++].pid = i;
				}
				//피지컬 메모리가 꽉찬 상태의 page fault 처리 부분
				else {
					//빠질 first가 저장하고 있는 pageTableEntry를 뽑아낸다.
					unsigned int Tfirstbit = (first->virtualPageNumber) >> (20 - firstLevelBits);
					unsigned int Tsecondbit = ((first->virtualPageNumber) << (12+firstLevelBits)) >> (12+firstLevelBits);
					fifo(&PageEntry[first->pid][Tfirstbit].secondLevelPageTable[Tsecondbit], &PageEntry[i][firstbit].secondLevelPageTable[secondbit], first, i,PageNum );
						//꽉찼고, 원형이기때문에 앞으로 한칸씩만 밀면 LRU성립
						first = first->lruRight;
						last = last->lruRight;
				}
			}

			//HIT
			else {
				procTable[i].numPageHit++;
				//LRU 처리
					unsigned int curFrameN = PageEntry[i][firstbit].secondLevelPageTable[secondbit].frameNumber;

					//현재 hit된게 LRU의 first였다면 first를 그 다음으로 바꿔준다.
					if (first == &phyMemFrames[curFrameN]) {
						first = first->lruRight;
					}
					if (last != &phyMemFrames[curFrameN]) {

						//현재 hit된 frame은 last로 빠지기 위해 자신의 좌우를 서로 연결해준다.
						phyMemFrames[curFrameN].lruLeft->lruRight = phyMemFrames[curFrameN].lruRight;
						phyMemFrames[curFrameN].lruRight->lruLeft = phyMemFrames[curFrameN].lruLeft;

						//현재 frame의 좌우를 새로 갱신해준다.
						phyMemFrames[curFrameN].lruLeft = last; //현재 frame의 왼쪽이 last
						phyMemFrames[curFrameN].lruRight = last->lruRight; //현재 frame의 오른쪽은 last의 오른쪽

						//last와 last의 오른쪽노드의 정보를 현재 frame연결로 갱신하고 last도 갱신한다.
						last->lruRight->lruLeft = &phyMemFrames[curFrameN]; //last의 오른쪽의 왼쪾은 현재 frame
						last->lruRight = &phyMemFrames[curFrameN]; //last의 오른쪽은 현재 frame
						last = &phyMemFrames[curFrameN];			//last는 현재 frame
					}
			}
			// -s option print statement
			if (s_flag) {
				Paddr = (PageEntry[i][firstbit].secondLevelPageTable[secondbit].frameNumber << 12) + Paddr;
				printf("Two-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
			}
		}
	}
		
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of second level page tables allocated %d\n",i,procTable[i].num2ndLevelPageTable);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
	FreePageTable(&PageEntry, numProcess);
}

int findhash(struct invertedPageTableEntry *invertEntry,int pid, unsigned int PageNum,int *fault_flag,int *empty_flag,unsigned int *frameNumber) {
	int i = 0;
	*fault_flag = 1;
	*empty_flag = 1;
	struct invertedPageTableEntry *first = invertEntry;
	//hash의 다음을 찾으면서 카운팅
	while (invertEntry->next) {
		*empty_flag = 0;
		i++;
		invertEntry = invertEntry->next;
		if ((invertEntry->pid == pid) && (invertEntry->virtualPageNumber == PageNum)) {
			*fault_flag = 0;
			*frameNumber = invertEntry->frameNumber;
			break;
		}
	}
	//fault가 나면 삽입
	/*if (fault_flag == 1) {
		struct invertedPageTableEntry *insertEntry = (struct invertedPageTableEntry *)malloc(sizeof(struct invertedPageTableEntry));
		
		insertEntry->next = first->next;
		first->next = insertEntry;
	}*/
	return i;
}

void insert(struct invertedPageTableEntry * invertEntry,int pid,unsigned int frameNumber,unsigned int PageNum, struct framePage *phyMemFrames,unsigned int hash_index) {
	// 새로운 invertedPage 할당후 phyFrame을 배정
	struct invertedPageTableEntry *insertEntry = (struct invertedPageTableEntry *)malloc(sizeof(struct invertedPageTableEntry));
	insertEntry->frameNumber = phyMemFrames[frameNumber].number;
	insertEntry->virtualPageNumber = PageNum;
	insertEntry->pid = pid;

	//삽입하는 엔트리는 해쉬맵의 맨앞에 연결해준다.
	insertEntry->next = invertEntry[hash_index].next;
	if (invertEntry[hash_index].next)
		invertEntry[hash_index].next->pre = insertEntry;
	invertEntry[hash_index].next = insertEntry;
	insertEntry->pre = &invertEntry[hash_index];

	//물리주소 값배정, inverted page 포인터로 가르킴
	phyMemFrames[frameNumber].virtualPageNumber = PageNum;
	phyMemFrames[frameNumber].invertedFrame = insertEntry;
	phyMemFrames[frameNumber].pid = pid;
}

void invertedPageVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, int nFrame) {
	initializeProc(procTable);
	struct invertedPageTableEntry *invertEntry = (struct invertedPageTableEntry *)calloc(nFrame, sizeof(struct invertedPageTableEntry));
	initPhyMem(phyMemFrames, phyFrameNum);

	int i;
	unsigned int phyFrameCount = 0;
	struct framePage * first = &phyMemFrames[0];
	//LRU는 새로운 addr을 읽을때마다 last가 변한다. 만약 새로부른게 first일 수 있으므로 확인후 갱신이 필요!
	struct framePage * last = &phyMemFrames[0];
	char gar; //W or R 
	unsigned int PageNum;
	unsigned int hash_index;
	int fault_flag = 0;
	int empty_flag = 1;
	unsigned CurFrameNumber;
	while (!feof(procTable[numProcess - 1].tracefp)) {
		for (i = 0; i < numProcess; i++) {
			fscanf(procTable[i].tracefp, "%x %c", &Vaddr, &gar);
			if (feof(procTable[i].tracefp))continue;
			Paddr = Vaddr & 0x00000FFF; // 물리주소 부분
			PageNum = Vaddr >> 12; //가상페이지의 넘버
			hash_index = (PageNum +i) % nFrame;
			procTable[i].ntraces++;

			//hashmap을 찾아본다.
			//findhash함수는 hashtable의 접근수를 return하고 flag값들과 물리주소를 포인터를 통해 가져온다.
			procTable[i].numIHTConflictAccess += findhash(&invertEntry[hash_index], i, PageNum, &fault_flag,&empty_flag,&CurFrameNumber);
			
			//찾아본 해쉬 맵이 비어있었는지 확인
			if (empty_flag) procTable[i].numIHTNULLAccess++;
			else procTable[i].numIHTNonNULLAcess++;
			
			// Page Fault
			if (fault_flag) {
				procTable[i].numPageFault++;
				//PhyFrame에 아직 공간이 남아있으면..
				if (phyFrameCount < nFrame) {
					//hashMap에 insert
					insert(invertEntry, i, phyFrameCount, PageNum, phyMemFrames, hash_index);
					last = &phyMemFrames[phyFrameCount++];	//가장 마지막에 access한 물리 프레임.		
				}
				//피지컬 메모리가 꽉찬 상태의 page fault 처리 부분
				else {
					//printf("fault인데 lRU뺴야함 \n");
					struct invertedPageTableEntry * delinverted = first->invertedFrame; //first가 가르키고 있던 삭제할 invertedFrame
					//삭제할 노드의 좌우를 결합.
					struct invertedPageTableEntry * Pre = delinverted->pre;
					struct invertedPageTableEntry * Next = delinverted -> next;
					if(Pre)
						Pre->next = Next; 
					if(Next)
						Next->pre = Pre;
					free(delinverted);
					//invertEntry에 새로운 virtual frame 삽입.
					insert(invertEntry, i, first->number, PageNum, phyMemFrames, hash_index);
					//꽉찼고, 원형이기때문에 앞으로 한칸씩만 밀면 LRU성립
					first = first->lruRight;
					last = last->lruRight;
				}
			}

			//HIT
			else {
			//	printf("HIT\n");
				procTable[i].numPageHit++;
				//LRU 처리
				unsigned int curFrameN = CurFrameNumber;

				//현재 hit된게 LRU의 first였다면 first를 그 다음으로 바꿔준다.
				if (first == &phyMemFrames[curFrameN]) {
					first = first->lruRight;
				}
				if (last != &phyMemFrames[curFrameN]) {

					//현재 hit된 frame은 last로 빠지기 위해 자신의 좌우를 서로 연결해준다.
					phyMemFrames[curFrameN].lruLeft->lruRight = phyMemFrames[curFrameN].lruRight;
					phyMemFrames[curFrameN].lruRight->lruLeft = phyMemFrames[curFrameN].lruLeft;

					//현재 frame의 좌우를 새로 갱신해준다.
					phyMemFrames[curFrameN].lruLeft = last; //현재 frame의 왼쪽이 last
					phyMemFrames[curFrameN].lruRight = last->lruRight; //현재 frame의 오른쪽은 last의 오른쪽

					//last와 last의 오른쪽노드의 정보를 현재 frame연결로 갱신하고 last도 갱신한다.
					last->lruRight->lruLeft = &phyMemFrames[curFrameN]; //last의 오른쪽의 왼쪾은 현재 frame
					last->lruRight = &phyMemFrames[curFrameN]; //last의 오른쪽은 현재 frame
					last = &phyMemFrames[curFrameN];			//last는 현재 frame
				}
			}
			// -s option print statement
			if (s_flag) {
				printf("IHT procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces, Vaddr, Paddr);
			}
		}
	}
		
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n",i,procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNonNULLAcess);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAcess == procTable[i].ntraces);
	}
	
}

int checkValidStart(int argc,char *argv[],int *pindex) {
	if (argc >= 5) {
		if (argv[1][0] == '-') {
			if (argv[1][1] != 's') {
				printf("%s: invalid option --'%c'\n", argv[0],argv[1][1]);
				return 0;
			}
			if (argc <= 5) return 0;
			*pindex += 1;
			s_flag = 1;
		}
		int ii;
		for (ii = *pindex; ii < *pindex + 3; ii++) {
			if (argv[ii][0] > '9' || argv[ii][0] < '0') {
				printf("Usage : simType firstLevelBits PhysicalMemorySizeBits must be \"unsign Int\"\n");
				return 0;
			}
		}
	return 1;
	 }
	return 0;
}




int main(int argc, char *argv[]) {
	int i;
	struct procEntry *procTable;
	struct framePage * phyMemFrames; //물리주소의 프레임 배열
	int pindex=1; //argv parameter 시작 인덱스 -s 옵션이 있을경우 +1
	
	if (!checkValidStart(argc,argv,&pindex)) {
	     printf("Usage : %s [-s] simType firstLevelBits PhysicalMemorySizeBits TraceFileNames\n",argv[0]); exit(1);
	}

	//옵션 매개변수 받음
	simType =atoi(argv[pindex]);  //실행 타입
	firstLevelBits = atoi(argv[pindex + 1]);	// one-level size
	phyMemSizeBits = atoi(argv[pindex + 2]);	// physical memory size
	numProcess = argc - pindex - 3;				// process 개수
	phyFrameNum = 1 << (phyMemSizeBits - 12);	// 피지컬 프레임의 개수
	if ((simType == 1 || simType >= 3) && firstLevelBits == 0) { printf("two-level을 실행하기 위해서 firstbits는 0 이상\n"); exit(1); }
	
	//프로세스 테이블, 개수만큼 할당
	procTable = (struct procEntry *)malloc(sizeof(struct procEntry)*numProcess);

	//피지컬 메모리 프레임개수만큼 할당
	phyMemFrames = (struct framePage *)malloc(sizeof(struct framePage)*phyFrameNum);
	// 피지컬메모리 프레임 초기값 설정
	initPhyMem(phyMemFrames, phyFrameNum);
	

	if (phyMemSizeBits < PAGESIZEBITS) {
		printf("PhysicalMemorySizeBits %d should be larger than PageSizeBits %d\n",phyMemSizeBits,PAGESIZEBITS); exit(1);
	}
	if (VIRTUALADDRBITS - PAGESIZEBITS - firstLevelBits <= 0 ) {
		printf("firstLevelBits %d is too Big for the 2nd level page system\n",firstLevelBits); exit(1);
	}
	
	// initialize procTable for memory simulations
	for(i = 0; i < numProcess; i++) {
		int traceIndex = pindex + 3 + i;
		// opening a tracefile for the process
		printf("process %d opening %s\n",i,argv[traceIndex]);
		procTable[i].tracefp = fopen(argv[traceIndex], "r+");
		if (!procTable[i].tracefp) {
			printf("ERROR: can't open %s file; exiting...\n", argv[pindex + 3 + i]);
			exit(1);
		}
	}

	int nFrame = (1<<(phyMemSizeBits-PAGESIZEBITS)); assert(nFrame>0);

	printf("\nNum of Frames %d Physical Memory Size %ld bytes\n",nFrame, (1L<<phyMemSizeBits));
	
	// initialize procTable for the simulation
	for(i = 0; i < numProcess; i++) {
		// initialize procTable fields
		// rewind tracefilesa
		int traceIndex = pindex + 3+ i;
		procTable[i].traceName = argv[traceIndex];
		procTable[i].pid = i;
		procTable[i].ntraces=0;				// the number of memory traces
		procTable[i].num2ndLevelPageTable=0;	// The 2nd level page created(allocated);
		procTable[i].numIHTConflictAccess=0; 	// The number of Inverted Hash Table Conflict Accesses
		procTable[i].numIHTNULLAccess=0;		// The number of Empty Inverted Hash Table Accesses
		procTable[i].numIHTNonNULLAcess=0;		// The number of Non Empty Inverted Hash Table Accesses
		procTable[i].numPageFault=0;			// The number of page faults
		procTable[i].numPageHit=0;
	
		rewind(procTable[i].tracefp);
	}

	//one-level
	if (simType == 0 || simType >= 3) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		// call oneLevelVMSim() with FIFO
		oneLevelVMSim(procTable, phyMemFrames, 0);

		// initialize procTable for the simulation
		printf("=============================================================\n");
		printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		// call oneLevelVMSim() with LRU
		oneLevelVMSim(procTable, phyMemFrames, 1);
	}

	//two - level
	if (simType == 1 || simType >= 3) {
		// initialize procTable for the simulation
		printf("=============================================================\n");
		printf("The Two-Level Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		// call twoLevelVMSim()
		twoLevelVMSim(procTable, phyMemFrames);
	}

	//Inverted
	if (simType == 2 || simType >= 3) {
		// initialize procTable for the simulation
		printf("=============================================================\n");
		printf("The Inverted Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		// call invertedPageVMsim()
		invertedPageVMSim(procTable, phyMemFrames, nFrame);
	}
	return(0);
}

