union
{
	int32 FAT32[128];
	int16 FAT16[256];
}FATTable;

int32 gFirstEmptyCluster;

FAT32Vars gFAT32Vars;
diskinforec DiskInfo;
FILE gFiles[MAXFILES];
int gFATErrCode;

#byte MMCAddressL = gFAT32Vars
#byte MMCAddressH = gFAT32Vars+1
#byte MMCAddressHL = gFAT32Vars+2
#byte MMCAddressHH = gFAT32Vars+3
#byte gStartSectorL = gFAT32Vars+8
#byte gStartSectorH = gFAT32Vars+9
#byte gStartSectorHL = gFAT32Vars+10
/*
#locate FATTable.FAT32 = 0x0600
#locate gFiles = 0x0800
#locate gFAT32Vars = 0x0C70
#locate DiskInfo = 0x0C90
*/
enum{
	FAT_UNKNOWN = 0,
	FAT12,
	FAT16,
	FAT32
};

char sFATName[4][6]={"FAT?", "FAT12", "FAT16", "FAT32"};
int gFATType;

#include "MMC.h"
#include "MMC.c"


char IsSelfDir(char *ch)
{
	if (ch[0] == '.' && ch[1] == '.') return 0xFF;
	else return 0;
}


int16 GetCurrentDOSDate()
{
	int16 retval;

	retval = myrec.tm_year - 1980;
	retval <<= 9;
	retval |= ((int16)myrec.tm_mon << 5);
	retval |= (int16)myrec.tm_mday;
	return retval;
}

int16 GetCurrentDOSTime()
{
	int16 retval;

	retval = myrec.tm_hour;
	retval <<= 11;
	retval |= ((int16)myrec.tm_min << 5);
	retval |= (int16)myrec.tm_sec >> 1;
	return retval;
}

int InitFAT()
{
	int32 actsector;
	int32 nTotalSec, nDataSec, nCountofClusters;
	char i;

	gFirstEmptyCluster = 0;
	gFAT32Vars.gStartSector = 0;
	gFAT32Vars.bFATModified = 0;
	gFATType = FAT_UNKNOWN;

	//Read start sector to temp buffer
	//fprintf(debug, "\r\nRead start sector ");
	gFATErrCode = ReadSector(gFAT32Vars.gStartSector,gFiles[MAXFILES-1].IObuffer);
	
	if(gFATErrCode != MMC_NO_ERR)
		return  gFATErrCode;

	if (gFiles[MAXFILES-1].IObuffer[0] != 0xEB) 
	{
		gStartSectorL = gFiles[MAXFILES-1].IObuffer[0x1C6];
		gStartSectorH = gFiles[MAXFILES-1].IObuffer[0x1C7];
		gStartSectorHL = gFiles[MAXFILES-1].IObuffer[0x1C8];

		//fprintf(debug, "\r\nRead start sector");
		gFATErrCode = ReadSector(gFAT32Vars.gStartSector,gFiles[MAXFILES-1].IObuffer);
		if(gFATErrCode != MMC_NO_ERR)
			return  gFATErrCode;
	}
	
	memcpy(&DiskInfo,gFiles[MAXFILES-1].IObuffer,sizeof(DiskInfo));

	//Check FAT type
	gFAT32Vars.nRootDirSectors = (DiskInfo.RootEntCnt*32 + (DiskInfo.BytsPerSec - 1))/DiskInfo.BytsPerSec;

	gFAT32Vars.nFatSize = DiskInfo.FATSz16;
	if(gFAT32Vars.nFatSize == 0)
		gFAT32Vars.nFatSize = DiskInfo.FATSz32;

	nTotalSec = DiskInfo.TotSec16;
	if(nTotalSec == 0)
		nTotalSec = DiskInfo.TotSec32;

	nDataSec = nTotalSec - (DiskInfo.RsvdSecCnt + DiskInfo.NumFATs*gFAT32Vars.nFatSize) + gFAT32Vars.nRootDirSectors;

	nCountofClusters = nDataSec/DiskInfo.SecPerClus;

	//fprintf(debug, "\r\nFatSize=%lu, TotalSec=%lu, CountofClusters=%lu",
	//	gFAT32Vars.nFatSize, nTotalSec, nCountofClusters);

	if(nCountofClusters < 4085)
		gFATType = FAT12;
	else if(nCountofClusters < 65525)
		gFATType = FAT16;
	else
		gFATType = FAT32;

	actsector = gFAT32Vars.gStartSector+DiskInfo.RsvdSecCnt;

	//Read FAT table
	//fprintf(debug, "\r\nRead FAT table ");
	gFATErrCode = ReadSector(actsector,FATTable.FAT32);

	gFAT32Vars.FATstartidx = 0;

	gFAT32Vars.gFirstDataSector = gFAT32Vars.gStartSector 
		+ DiskInfo.NumFATs*gFAT32Vars.nFatSize
		+ gFAT32Vars.nRootDirSEctors 
		+ DiskInfo.RsvdSecCnt - 2*DiskInfo.SecPerClus;
	
	for (i=0;i<MAXFILES;i++)
		gFiles[i].Free = TRUE;
		
	return  gFATErrCode;

}

void SaveFATTable()
{
	int32 actsector;
	actsector = gFAT32Vars.gStartSector+DiskInfo.RsvdSecCnt + gFAT32Vars.FATstartidx;
	gFATErrCode = WriteSector(actsector,FATTable.FAT32);
	actsector += gFAT32Vars.nFatSize;
	gFATErrCode = WriteSector(actsector,FATTable.FAT32);	

	gFAT32Vars.bFATModified = 0;
	
}

int32 GetNextCluster(int32 curcluster)
{
	int32 actsector;
	int32 clpage;
	char clpos;

#ifdef TRACE
	TRACE1("\r\nGetNextCluster(%lu)", curcluster);	
#endif
	if(gFATType == FAT32)
	{
		clpage = curcluster >> 7;
		clpos = curcluster & 0x7F;
		if (clpage != gFAT32Vars.FATstartidx) // read in the requested page
		{ 
			if(gFAT32Vars.bFATModified)
				SaveFATTable();			
			actsector = gFAT32Vars.gStartSector+DiskInfo.RsvdSecCnt + clpage;
			ReadSector(actsector,FATTable.FAT32);
			gFAT32Vars.FATstartidx = clpage;
		}
		return (FATTable.FAT32[clpos]);
	}
	else	//FAT16
	{
		clpage = curcluster >> 8;
		clpos = curcluster & 0xFF;		
		if (clpage != gFAT32Vars.FATstartidx) // read in the requested page
		{ 
			if(gFAT32Vars.bFATModified)
				SaveFATTable();
			actsector = gFAT32Vars.gStartSector+DiskInfo.RsvdSecCnt + clpage;
			ReadSector(actsector,FATTable.FAT16);
			gFAT32Vars.FATstartidx = clpage;
		}
		if(FATTable.FAT16[clpos] > 0xFFF4)
			return (FATTable.FAT16[clpos]|0x0FFF0000);
		else
			return (FATTable.FAT16[clpos]);
	}
	
}

void SetClusterEntry(int32 curcluster,int32 value)
{
	int32 actsector;
	int32 clpage;
	char clpos;

#ifdef TRACE
	//fprintf(debug, "\r\nSetClusterEntry(%lu, %lu)", curcluster, value);	
#endif
	if(gFATType == FAT32)
	{
		//FAT32, 4 bytes per entery. Total 128 entries per sector (512 bytes)
		clpage = curcluster >> 7;	//Calculate sector# in FAT table
		clpos = curcluster & 0x7F;	//Calculate FAT index in sector
		//Calculate FAT physical sector #
		actsector = gFAT32Vars.gStartSector+DiskInfo.RsvdSecCnt + clpage;
		
		if (clpage != gFAT32Vars.FATstartidx) 
		{
			if(gFAT32Vars.bFATModified)
				SaveFATTable();
			ReadSector(actsector,FATTable.FAT32);
			gFAT32Vars.FATstartidx = clpage;
		}
		FATTable.FAT32[clpos] = value;
		gFAT32Vars.bFATModified = 1;
	}
	else	//FAT16
	{
		//FAT16, 2 bytes per entry. Total 256 entries per sector (512 bytes)
		clpage = curcluster >> 8;
		clpos = curcluster & 0xFF;
		actsector = gFAT32Vars.gStartSector+DiskInfo.RsvdSecCnt + clpage;
		if (clpage != gFAT32Vars.FATstartidx) 
		{
			if(gFAT32Vars.bFATModified)
				SaveFATTable();
			ReadSector(actsector,FATTable.FAT16);
			gFAT32Vars.FATstartidx = clpage;
		}
		FATTable.FAT16[clpos] = value;
		gFAT32Vars.bFATModified = 1;
	}
//	gFATErrCode = WriteSector(actsector,FATTable.FAT32);
//	actsector += gFAT32Vars.nFatSize;
//	gFATErrCode = WriteSector(actsector,FATTable.FAT32);
}

void ClearClusterEntry(int32 curcluster)
{
	int32 actsector;
	int32 clpage;
	char clpos;

#ifdef TRACE
	//fprintf(debug, "\r\nClearClusterEntry()");	
#endif
	if(gFATType == FAT32)
	{
		clpage = curcluster >> 7;
		clpos = curcluster & 0x7F;
	}
	else	//FAT16
	{
		clpage = curcluster >> 8;
		clpos = curcluster & 0xFF;
	}
	if (clpage != gFAT32Vars.FATstartidx) 
	{
		actsector = gFAT32Vars.gStartSector+DiskInfo.RsvdSecCnt + gFAT32Vars.FATstartidx;
		WriteSector(actsector,FATTable.FAT32);
		actsector += gFAT32Vars.nFatSize;
		WriteSector(actsector,FATTable.FAT32);
		actsector = gFAT32Vars.gStartSector+DiskInfo.RsvdSecCnt + clpage;
		ReadSector(actsector,FATTable.FAT32);
		gFAT32Vars.FATstartidx = clpage;
	}
	
	if(gFATType == FAT32)
	{
		FATTable.FAT32[clpos] = 0;
	}
	else	//FAT16
	{
		FATTable.FAT16[clpos] = 0;
	}
	gFAT32Vars.bFATModified = 1;
}

int32 FindFirstFreeCluster()
{
	int32 i,st,actsector,retval;
	int16 j;

#ifdef TRACE
	//fprintf(debug, "\r\nFindFirstFreeCluster()");	
#endif
	st = gFirstEmptyCluster;
	if(gFATType == FAT32)
	{
		for (i=st;i<DiskInfo.FATSz32;i++) 
		{
			if (i != gFAT32Vars.FATstartidx) 
			{
				actsector = gFAT32Vars.gStartSector+DiskInfo.RsvdSecCnt + i;
				//Save FAT
				if(gFAT32Vars.bFATModified)
					SaveFATTable();
				ReadSector(actsector,FATTable.FAT32);
				gFAT32Vars.FATstartidx = gFirstEmptyCluster = i;
			}
			for (j=0;j<128;j++)
				if (FATTable.FAT32[j] == 0) 
				{
					retval = i;
					retval <<= 7;
					retval |= j;
					return retval;
				}
		}
	}
	else	//FAT16
	{
		for (i=st;i<DiskInfo.FATSz16;i++) 
		{
			if (i != gFAT32Vars.FATstartidx) 
			{
				actsector = gFAT32Vars.gStartSector+DiskInfo.RsvdSecCnt + i;
				if(gFAT32Vars.bFATModified)
					SaveFATTable();
				ReadSector(actsector,FATTable.FAT16);
				gFAT32Vars.FATstartidx = gFirstEmptyCluster = i;
			}
			for (j=0;j<256;j++)
				if (FATTable.FAT16[j] == 0) 
				{
					retval = i;
					retval <<= 8;
					retval |= j;
					if(retval > 0xFFF4)
						return (retval|0x0FFF0000);
					else
						return retval;
				}
		}
	}
	return 0x0FFFFFFF;
}

void ConvertFilename(DIR *beDir,char *name)
{
	char i,j,c;

	j = 0;
	name[0] = 0;
	for (i=0;i<8;i++) {
		c = beDir->sName[i];
		if (c == ' ') break;
		name[j++] = c;
	}
	for (i=0;i<3;i++) {
		c = beDir->sExt[i];
		if (c == ' ' || c == 0) break;
		if (!i) name[j++] = '.';
		name[j++] = c;
	}
	name[j++] = 0;
}

void GetDOSName(DIR *pDir, char *fname)
{
	char i,j,leng,c,toext;

	toext = FALSE;
	j = 0;
	leng = strlen(fname);
	for (i=0;i<8;i++)
		pDir->sName[i] = ' ';
	for (i=0;i<3;i++)
		pDir->sExt[i] = ' ';
	for (i=0;i<leng;i++) {
		c = fname[i];
		c = toupper(c);
		if (c == '.') {
			toext = TRUE;
			continue;
		}
		if (toext) pDir->sExt[j++] = c;
		else pDir->sName[i] = c;
	}
}

//Read the first dir sector
void ReadRootDirectory(HANDLE hFile)
{
	int32 actsector;

	TRACE0("\r\nReadRootDirectory()");	

	if (hFile > (MAXFILES-1)) 
		return;
		
	actsector = gFAT32Vars.gStartSector + DiskInfo.NumFATs*gFAT32Vars.nFatSize+DiskInfo.RsvdSecCnt;
	
	ReadSector(actsector,DIR_BUFFER);
	
	gFAT32Vars.gDirEntrySector = actsector;
	gFiles[hFile].dirSector = actsector;
	gFiles[hFile].dirIdx = 0;
	memcpy(&(gFiles[hFile].DirEntry),DIR_BUFFER,32);
	gFiles[hFile].CurrentCluster = DiskInfo.RootClus;
}

//Always call TryFile before calling FindDirEntry to allow search from root dir sector
char FindDirEntry(char *fname,HANDLE hFile)
{
	DIR *pDir;
	int16 i;
	char filename[16];
	int32 nextcluster,actsector;
	int nSecInClus;
	int bDone;


	TRACE2("\r\nFindDirEntry(%s,%d)", fname, hFile);	

	if (hFile > (MAXFILES-1)) 
		return FALSE;

	bDone = false;
	nSecInClus = 0;
	gFAT32Vars.gFirstEmptyDirEntry = 0xFFFF;
	gFAT32Vars.gFirstDirEntryCluster = 0x0FFFFFFF;
	nextcluster = gFiles[hFile].CurrentCluster;
	do {
		pDir = (DIR*)(DIR_BUFFER);
		for (i=0;i<16;i++) 
		{
			if ((pDir->sName[0] == 0xE5 || pDir->sName[0] == 0) && gFAT32Vars.gFirstEmptyDirEntry == 0xFFFF)  // store first free
			{
				gFAT32Vars.gFirstEmptyDirEntry = i;
				gFAT32Vars.gFirstDirEntryCluster = gFiles[hFile].CurrentCluster;
			}

			if (pDir->sName[0] == 0) 	//Searched all exist dir entries
				return FALSE;

			ConvertFilename(pDir,filename);

			if (!strcmp(filename,fname)) 	//Found matching dir entry
			{
				memcpy(&(gFiles[hFile].DirEntry),pDir,32);
				gFiles[hFile].dirIdx = i;
				gFAT32Vars.gDirEntryIdx = i;
				return TRUE;
			}
			pDir++;
		}
		
		if(gFATType == FAT32)	//FAT32 DIR sector are chained
		{
			//Searched all sector in cluster?
			if(++nSecInClus < DiskInfo.SecPerClus)
			{
				actsector = gFiles[hFile].CurrentCluster*DiskInfo.SecPerClus + gFAT32Vars.gFirstDataSector + nSecInClus;
				ReadSector(actsector,DIR_BUFFER);
				gFAT32Vars.gDirEntrySector = actsector;
				gFiles[hFile].dirSector = actsector;
			}
			else	//Get next cluster
			{
				nSecInClus = 0;
				nextcluster = GetNextCluster(gFiles[hFile].CurrentCluster);
				gFAT32Vars.gFirstDirEntryCluster = nextcluster;
				if (nextcluster != 0x0FFFFFFF && nextcluster != 0) 
				{
					actsector = nextcluster*DiskInfo.SecPerClus + gFAT32Vars.gFirstDataSector;
					ReadSector(actsector,DIR_BUFFER);
					gFAT32Vars.gDirEntrySector = actsector;
					gFiles[hFile].dirSector = actsector;
					gFiles[hFile].CurrentCluster = nextcluster;
				}
				else 
					bDone = true;
			}
			//bDone = !(nSecInClus < DiskInfo.SecPerClus || (nextcluster != 0x0FFFFFFF && nextcluster != 0));
		}
		else	//FAT16
		{
			
			if(++nSecInClus < gFAT32Vars.nRootDirSectors)
			{
				actsector = gFAT32Vars.gStartSector + DiskInfo.NumFATs*gFAT32Vars.nFatSize+DiskInfo.RsvdSecCnt + nSecInClus;
				ReadSector(actsector,DIR_BUFFER);
				gFAT32Vars.gDirEntrySector = actsector;
				gFiles[hFile].dirSector = actsector;
			}
			else
			{
				bDone = true;
			}
			
		}
	} while (!bDone);

	return FALSE;
}

// file I/O routines
char* TryFile(char *fname, HANDLE *hFile)
{
	char i,leng;
	char *filename;

	TRACE1("\r\nTryFile(%s)", fname);
	*hFile = 0xFF;
	for (i=0;i<MAXFILES;i++) 
	{
		if (gFiles[i].Free) 
		{
			*hFile = i;
			break;
		}
	}
	if (*hFile == 0xFF) 
		return 0;

	ReadRootDirectory(*hFile);

	filename = fname;
	leng = strlen(fname);
	for (i=0;i<leng;i++) 
	{
		if (fname[i] == '/') 
		{
			fname[i] = 0;
			if (!cwd(filename,*hFile)) 
			{
				gFiles[*hFile].Free = TRUE;
				return 0;
			}
			filename = fname+i+1;
		}
	}
	return filename;
}

char fcreate(HANDLE hFile,char *fname)
{
	DIR *pDir;
	int32 actsector,actcl;
	int16 i;

#ifdef TRACE
	TRACE2("\r\nfcreate(%d, %s)", hFile, fname);	
#endif

	if (hFile > (MAXFILES-1))
		return FALSE;

	if(gFATType == FAT32)
	{
		if (gFAT32Vars.gFirstDirEntryCluster == 0x0FFFFFFF) 
		{
#ifdef TRACE
			TRACE0("\r\nfcreate() - gFirstDirEntryCluster == 0xFFFFFFF");	
#endif			// extend the directory file !!!
			gFAT32Vars.gFirstDirEntryCluster = FindFirstFreeCluster();
			gFAT32Vars.gFirstEmptyDirEntry = 0;
			SetClusterEntry(gFiles[hFile].CurrentCluster,gFAT32Vars.gFirstDirEntryCluster);
			SetClusterEntry(gFAT32Vars.gFirstDirEntryCluster,0x0FFFFFFF);
			actsector = gFAT32Vars.gFirstDirEntryCluster*DiskInfo.SecPerClus + gFAT32Vars.gFirstDataSector;
			
			for (i=0;i<512;i++)
				DIR_BUFFER[i] = 0;

			//Clean up  all sectors in the dir cluster
			for(i=0; i<DiskInfo.SecPerClus; i++)
				gFATErrCode = WriteSector(actsector + i,DIR_BUFFER);
				
			if(gFATErrCode != MMC_NO_ERR)
			{
#ifdef TRACE
				TRACE0( "\r\nfcreate() - WriteSector failed");	
#endif				return false;
			}
		}
		else
			actsector = gFiles[hFile].dirSector;

		//actsector = gFAT32Vars.gFirstDirEntryCluster*DiskInfo.SecPerClus + gFAT32Vars.gFirstDataSector;
	}
	else	//FAT16
	{
		//FAT16 has fixed size DIR sectors
		actsector = gFiles[hFile].dirSector;
	}
	
	ReadSector(actsector,DIR_BUFFER);

	pDir = (DIR*)(&(DIR_BUFFER[32*gFAT32Vars.gFirstEmptyDirEntry]));
	gFiles[hFile].dirSector = actsector;
	gFiles[hFile].dirIdx = gFAT32Vars.gFirstEmptyDirEntry;
	
	GetDOSName(pDir,fname);
	pDir->bAttr = 0;
	actcl = FindFirstFreeCluster();

#ifdef TRACE
	TRACE1("\r\nFindFirstFreeCluster returns %lu", actcl);	
#endif	

	pDir->hCluster = actcl & 0xFFFF;
	pDir->hClusterH = actcl >> 16;
	SetClusterEntry(actcl,0x0FFFFFFF);
	pDir->wSize = 0;
	gFiles[hFile].wFileSize = 0;
	pDir->hDate = GetCurrentDOSDate();
	pDir->hTime = GetCurrentDOSTime();

	gFATErrCode = WriteSector(actsector,DIR_BUFFER);

	memcpy(&(gFiles[hFile].DirEntry),pDir,32);
	return TRUE;
}

int32 ComposeCluster(HANDLE hFile)
{
	int32 retval;

	retval = gFiles[hFile].DirEntry.hClusterH;
	retval <<= 16;
	retval |= gFiles[hFile].DirEntry.hCluster;
	return retval;
}

char fopen(char *fname, char mode)
{
	char found;
	HANDLE hFile;
	int32 actsector,actcluster,nextcluster;
	char *filename;

#ifdef TRACE
	TRACE2("\r\nfopen(%s, %c)", fname, mode);	
#endif
	if (NO_MMC_CARD) 
		return 0xFF;

	filename = TryFile(fname,&hFile);
	
	if (filename == 0) 	//Invalid filename?
		return 0xFF;
	
	found = FALSE;
	found = FindDirEntry(filename,hFile);
	
	if (!found) 	//File not exist
	{
		if (mode == 'r') 
		{
			gFiles[hFile].Free = TRUE;
			return 0xFF;
		} 
		else 
		{
			if (!fcreate(hFile,filename)) 
				return 0xFF;
			found = TRUE;
		}
	}
	if (found) 
	{
		gFiles[hFile].Free = FALSE;
		gFiles[hFile].mode = mode;
		if  (mode == 'a') 	//Append
		{
			gFiles[hFile].wFileSize = gFiles[hFile].DirEntry.wSize;
			actcluster = ComposeCluster(hFile);
			nextcluster = actcluster;
			TRACE2("\r\nCluster starts at %lu(0x%04X).", actcluster, actcluster);
			while (actcluster != 0x0FFFFFFF && nextcluster != 0) 
			{
				nextcluster = GetNextCluster(actcluster);
				if (nextcluster == 0x0FFFFFFF || nextcluster == 0) 
					break;
				actcluster = nextcluster;
			}
			TRACE2("\r\nFound end cluster %lu(0x%04X).", actcluster, actcluster);
			
			actsector = actcluster*DiskInfo.SecPerClus + gFAT32Vars.gFirstDataSector;
			gFiles[hFile].SecInCluster = (gFiles[hFile].wFileSize>>9)%DiskInfo.SecPerClus;
			actsector += gFiles[hFile].SecInCluster;
			
			ReadSector(actsector,gFiles[hFile].IObuffer);
			
			gFiles[hFile].CurrentCluster = actcluster;
			gFiles[hFile].posinsector = gFiles[hFile].wFileSize & 0x01FF;
			
			if (gFiles[hFile].posinsector == 0 && gFiles[hFile].wFileSize != 0) 
				gFiles[hFile].posinsector = 512;
		} 
		else 	//Write or read only
		{
			gFiles[hFile].wFileSize = 0;
			actsector = ComposeCluster(hFile)*DiskInfo.SecPerClus;
			actsector += gFAT32Vars.gFirstDataSector;
			ReadSector(actsector,gFiles[hFile].IObuffer);
			gFiles[hFile].CurrentCluster = ComposeCluster(hFile);
			gFiles[hFile].posinsector = 0;
			gFiles[hFile].SecInCluster = 0;
		}
	}
	return hFile;
}

void fclose(HANDLE hFile)
{
#ifdef TRACE
	//fprintf(debug, "\r\nfclose()");	
#endif
	//	output_low(YELLOWLED);
	if (hFile > (MAXFILES-1)) return;
	if ((gFiles[hFile].mode == 'a') || (gFiles[hFile].mode == 'w')) 
		fflush(hFile);
	gFiles[hFile].Free = TRUE;
}


void fflush(HANDLE hFile)
{
	int32 actsector;
	DIR *pDir;

	if (hFile > (MAXFILES-1)) 
		return;
	
	//Write data sector
	actsector = gFiles[hFile].CurrentCluster*DiskInfo.SecPerClus + gFAT32Vars.gFirstDataSector;
	actsector += gFiles[hFile].SecInCluster;
	WriteSector(actsector,gFiles[hFile].IObuffer);
	
	if(gFAT32Vars.bFATModified)
		SaveFATTable();
	
	//Read dir entry
#ifdef DIR_SHARE_IOBUFFER	
	if(ReadSector(gFiles[hFile].dirSector,gFiles[hFile].IObuffer) == MMC_NO_ERR)
	{
#endif
		pDir = (DIR*)(&(DIR_BUFFER[32*gFiles[hFile].dirIdx]));
		
		//Update file size
		if (gFiles[hFile].DirEntry.bAttr & 0x10) 
			pDir->wSize = 0; // if it is a directory
		else 
			pDir->wSize = gFiles[hFile].wFileSize;

		//Update file date/time
		pDir->hDate = GetCurrentDOSDate();
		pDir->hTime = GetCurrentDOSTime();

		//Write dir entry
		WriteSector(gFiles[hFile].dirSector,DIR_BUFFER);
#ifdef DIR_SHARE_IOBUFFER
	}
	//Read back data sector
	ReadSector(actsector,gFiles[hFile].IObuffer);
#else

}

char cwd(char *fname, HANDLE hFile)
{
	int32 actsector;

	if (hFile > (MAXFILES-1)) 
		return FALSE; // just in case of overaddressing
	
	if (IsSelfDir(fname)) 
		return TRUE; // already in Root dir
	
	if (!FindDirEntry(fname,hFile)) 
		return FALSE; // not found
	
	actsector = ComposeCluster(hFile)*DiskInfo.SecPerClus;
	actsector += gFAT32Vars.gFirstDataSector; // read current dir
	ReadSector(actsector,DIR_BUFFER);
	gFAT32Vars.gDirEntrySector = actsector;
	gFiles[hFile].dirSector = actsector;
	gFiles[hFile].CurrentCluster = ComposeCluster(hFile);
	return TRUE;
}

void fputch(char ch, HANDLE hFile)
{
	int32 nextcluster,actsector;

	if (hFile > (MAXFILES-1)) 
		return;
		
	//Sector buffer full? Write to disk
	if (gFiles[hFile].posinsector >= 512) 
	{
#ifdef DIR_SHARE_IOBUFFER		
		//Calculate physical sector#
		actsector = gFiles[hFile].CurrentCluster*DiskInfo.SecPerClus + gFAT32Vars.gFirstDataSector;
		actsector += gFiles[hFile].SecInCluster;
		
		//Write to disk
		WriteSector(actsector,gFiles[hFile].IObuffer);
#else
		fflush(hFile);
#endif
		//Increase sector counter
		gFiles[hFile].SecInCluster++;	

		//Finished a cluster? Find a free cluster and append it to FAT chain.
		if(gFiles[hFile].SecInCluster >= DiskInfo.SecPerClus)
		{
			//Get next free cluster
			nextcluster = FindFirstFreeCluster();

			//Is it a valid cluster?
			if (nextcluster != 0x0FFFFFFF && nextcluster != 0) 
			{
				//Append new found cluster to FAT chain
				SetClusterEntry(gFiles[hFile].CurrentCluster,nextcluster);
				SetClusterEntry(nextcluster,0x0FFFFFFF);
				
				//Calculate phsical sector#
				actsector = nextcluster*DiskInfo.SecPerClus + gFAT32Vars.gFirstDataSector;
				
				//Read sector into buffer			
				ReadSector(actsector,gFiles[hFile].IObuffer);
				
				//Clear buffer
				memset(gFiles[hFile].IObuffer, 0, 512);
			
				//Remember cluster#
				gFiles[hFile].CurrentCluster = nextcluster;
			}
			
			//Reset sector (in cluster) counter
			gFiles[hFile].SecInCluster = 0;
		}

		//Reset byte (in sector) counter
		gFiles[hFile].posinsector = 0;
	}
	
	gFiles[hFile].IObuffer[gFiles[hFile].posinsector] = ch;
	gFiles[hFile].posinsector++;
	gFiles[hFile].wFileSize++;

}

void fputstring(char *str, HANDLE hFile)
{

#ifdef TRACE
//	fprintf(debug, "\r\nfputstring(%s, %d)", str, hFile);	
#endif

	if (hFile > (MAXFILES-1)) 
		return;

	while(*str)
		fputch(*str++,hFile);
}

int16 fread(char *buffer, int16 leng, HANDLE hFile)
{
	int16 i,retv;
	char c,v;

	TRACE1("\r\nfread(length=%ld)", leng);	

	if (hFile > (MAXFILES-1)) return 0;
	retv = 0;
	for (i=0;i<leng;i++) 
	{
		v = fgetch(&c,hFile);
		if (v) {
			buffer[i] = c;
			retv++;
		}
		else break;
	}
	return retv;
}

void fwrite(char *buffer, int16 leng, HANDLE hFile)
{
	int16 i;

	TRACE1("\r\nfwrite(length=%ld)", leng);	
	
	if (hFile > (MAXFILES-1)) 
		return;
		
	for (i=0;i<leng;i++)
		fputch(buffer[i],hFile);

}

char fgetch(char *ch,HANDLE hFile)
{
	int32 nextcluster,actsector;

	if (hFile > (MAXFILES-1)) 
		return FALSE;
	
	if (gFiles[hFile].wFileSize >= gFiles[hFile].DirEntry.wSize) //Invalid read pointer
		return FALSE;
	
	*ch = gFiles[hFile].IObuffer[gFiles[hFile].posinsector];
	
	gFiles[hFile].posinsector++;
	gFiles[hFile].wFileSize++;
	
	if (gFiles[hFile].posinsector >= 512) 
	{
		//More sector in cluster?
		gFiles[hFile].SecInCluster++; 
		if(gFiles[hFile].SecInCluster < DiskInfo.SecPerClus)	//Get next sector incluster
		{
			actsector = gFiles[hFile].CurrentCluster*DiskInfo.SecPerClus + gFAT32Vars.gFirstDataSector;
			actsector += gFiles[hFile].SecInCluster;
			ReadSector(actsector,gFiles[hFile].IObuffer);
			gFiles[hFile].posinsector = 0;
		}
		else	//Get next cluster
		{
			gFiles[hFile].SecInCluster = 0;
			nextcluster = GetNextCluster(gFiles[hFile].CurrentCluster);
			if (nextcluster != 0x0FFFFFFF && nextcluster != 0) 
			{
				actsector = nextcluster*DiskInfo.SecPerClus + gFAT32Vars.gFirstDataSector;
				ReadSector(actsector,gFiles[hFile].IObuffer);
				gFiles[hFile].CurrentCluster = nextcluster;
				gFiles[hFile].posinsector = 0;
			}
		}
	}
	return TRUE;
}

char remove(char *fname)
{
	char i,found;
	HANDLE hFile;
	DIR *pDir;
	int32 nextcluster,currentcluster;
	char *filename;

	filename = TryFile(fname,&hFile);
	if (filename == 0) return FALSE;
	found = FindDirEntry(filename,hFile);
	if (!found) {
		gFiles[hFile].Free = TRUE;
		return FALSE;
	}
	pDir = (DIR*)(&(DIR_BUFFER[32*gFAT32Vars.gDirEntryIdx]));
	pDir->sName[0] = 0xE5;
	for (i=1;i<8;i++)
		pDir->sName[i] = ' ';
	for (i=0;i<3;i++)
		pDir->sExt[i] = ' ';
	WriteSector(gFAT32Vars.gDirEntrySector,DIR_BUFFER);
	currentcluster = ComposeCluster(hFile);
	while (currentcluster != 0x0FFFFFFF && nextcluster != 0) 
	{
		nextcluster = GetNextCluster(currentcluster);
		ClearClusterEntry(currentcluster);
		currentcluster = nextcluster;
	}
	ClearClusterEntry(currentcluster);
	SetClusterEntry(currentcluster,0);
	SaveFATTable();
//	currentcluster = gFAT32Vars.gStartSector+DiskInfo.RsvdSecCnt + gFAT32Vars.FATstartidx;
//	WriteSector(currentcluster,FATTable.FAT32);
//	currentcluster += DiskInfo.FATSz32;
//	WriteSector(currentcluster,FATTable.FAT32);
	gFiles[hFile].Free = TRUE;
	return TRUE;
}

char getfsize(char *fname, int32 *fsiz)
{
	char found;
	HANDLE hFile;
	DIR *pDir;
	char *filename;

	*fsiz = 0;
	filename = TryFile(fname,&hFile);
	if (filename == 0) 
		return 1;

	found = FindDirEntry(filename,hFile);
	if (!found) 
	{
		gFiles[hFile].Free = TRUE;
		return 2;
	}
	//fprintf(debug, "getfsize - found DirEntry %lu.\r\n", gFAT32Vars.gDirEntryIdx);
	
	pDir = (DIR*)(&(DIR_BUFFER[32*gFAT32Vars.gDirEntryIdx]));
	gFiles[hFile].Free = TRUE;
	*fsiz = pDir->wSize;
	return 0;
}

