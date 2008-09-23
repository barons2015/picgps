#define MAXFILES 1 
typedef struct _diskinforec 
{ 
	char   	JumpCode[3]; 	//0xEB??90 or 0xE9????
	char  	OEMName[8]; 
	int16   BytsPerSec; 	//512, 1024 or 4096. Only 512 supported
	char   	SecPerClus; 	//1~128
	int16 	RsvdSecCnt; 	//Number of reserved sectors
	char   	NumFATs; 	//Number of FAT data structure
	int16 	RootEntCnt; 	//FAT12, FAT16: Number of root dir. FAT32: 0
	int16 	TotSec16; 	//Total sectors. If 0 check TotalSectors32
	char   	Media; 		//Media
	int16 	FATSz16; 	//For FAT12/FAT16, Sectors per FAT. FAT32: 0
	int16 	SecPerTrk; 	//
	int16 	NumHeads; 	//Number of heads
	int32   HiddSec; 	//Hidden sectors

	int32   TotSec32; 	//If nSectors is 0, this is the total number of sectors

	int32 	FATSz32; 
	int16 	ExtFlags; 
	int16 	FSVer; 
	int32 	RootClus; 
} diskinforec; 

typedef struct _direntry 
{ 
	char   sName[8]; 
	char   sExt[3]; 
	char   bAttr; 
	char   bReserved[8]; 
	int16 	hClusterH; 
	int16   hTime; 
	int16   hDate; 
	int16   hCluster; 
	int32   wSize; 
} DIR; 

typedef struct 
{ 
	char	IOpuffer[512]; 		//Data buffer
	DIR	DirEntry; 		//DIR entry
	int32	CurrentCluster; 	//Current data cluster#
	int	SecInCluster;		//Current sector# in cluster (0~DiskInfo.SecPerClus-1)
	int16	posinsector; 		//Byte index in current sector (0~511)
	int32   wFileSize; 		//Byte index in whole file
	int32	dirSector; 		//DIR entry sector#
	int16	dirIdx; 		//DIR entry index in sector
	char	mode; 			//'a' append, 'w' over write, 'r' read only
	char	Free; 
}FILE; 

typedef struct 
{ 
	int32 MMCAddress; 
	int32 FATstartidx; 
	int32 gStartSector; 
	int32 gFirstDataSector; 
	int16 gDirEntryIdx; 
	int32 gDirEntrySector; 
	int16 gFirstEmptyDirEntry; 
	int32 gFirstDirEntryCluster; 
	int32 nRootDirSectors;
	int32 nFatSize;
	int   bFATModified;
} FAT32Vars; 

#define HANDLE	char

int InitFAT(); 
char FindDirEntry(char *fname, HANDLE hFile); 

char fopen(char *fname, char mode); 
void fclose(HANDLE hFile); 
void fflush(HANDLE hFile); 
char cwd(char *fname, HANDLE hFile); 
void fputch(char ch, HANDLE hFile); 
void fputchar(char ch){fputch(ch, 0);}
char fgetch(char *ch, HANDLE hFile); 
void fputstring(char *str, HANDLE hFile); // fputs is reserved in CCS C 
int16 fread(char *buffer, int16 leng, HANDLE hFile); 
void fwrite(char *buffer, int16 leng, HANDLE hFile); 
char remove(char *fname); 
char getfsize(char *fname, int32 *fsiz); 
