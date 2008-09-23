#include "GpsLogger.h"

//#include "..\share\bootloader.h"

#include "string.h"
#include "stdlib.h"

#include "2402.c"


#ORG default

#define GPS_BUFFER_LEN	128

char GPSBuffer[GPS_BUFFER_LEN];
int nGPSReceived=0;

char GPSFlag;

#bit bGPRMC=GPSFlag.0
#bit bGPGGA=GPSFlag.1
#bit bGPSBusy = GPSFlag.7

//Buffer for each sentence

char GPRMCBuffer[GPS_BUFFER_LEN];
int nGPRMCLen=0;
char GPGGABuffer[GPS_BUFFER_LEN];
int nGPGGALen=0;
//char GPGSABuffer[GPS_BUFFER_LEN];
//int nGPGSALen=0;
//char GPVTGBuffer[GPS_BUFFER_LEN];
//int nGPVTGLen=0;
const char sGPRMC[]="RMC";
const char sGPGGA[]="GGA";
//const char sGPGSA[]="GSA";
//const char sGPVTG[]="VTG";

//GPRMC example
//$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62
//      1   220516     Time Stamp
//      2   A          validity - A-ok, V-invalid
//      3   5133.82    current Latitude
//      4   N          North/South
//      5   00042.24   current Longitude
//      6   W          East/West
//      7   173.8      Speed in knots
//      8   231.8      True course
//      9   130694     Date Stamp
//      10  004.2      Variation
//      11  W          East/West
//      12  *70        checksum

int n200msTick=0;

//Timers
int nBackLightTimeout=15;
int nGPSTimeout=0;
int nFlushTimeout=60;
int nMenuTimeout=1;

//Menu
int nMenuItem = 0;

#define MAX_MENU_PAGE 2


#define SW_MENU	0x10
#define SW_UP	0x80
#define SW_DOWN	0x40
#define SW_SELECT	0x20

#define VT_INFO 0	//ViewType : Information 
#define VT_MENU	1	//ViewType : Menu

char g_nViewType = VT_INFO;
char g_nKeyOldState, g_nKeyNewState;	//Hardkey status

float gBattVoltage;
#define BATT_LOW_WARN_VOLT	3.5	//Battery low warning voltage
#define BATT_LOW_SHUT_VOLT	3.4	//Shutdown voltage

void CheckButtonKeys();
void DisplayView();

#define LIGHT_LCDBACK_ON	0x01
#define LIGHT_WHITE_ON		0x02
#define LIGHT_GREEN_ON		0x04

#define LOG_FLAG_DISCARD_ZERO_KHM	0x01

#define MAX_INFO_VIEW_PAGE	3
typedef struct  {
	unsigned long    tm_year;
	char            tm_mon;
	char            tm_day;
	char            tm_mday;
	char            tm_hour;
	char            tm_min;
	char            tm_sec;
} TimeRecord;

typedef struct 
{
	char nFlag;
	signed char nTimeZone;
	char nBKLightLevel;
	char nInfoViewPage;
	char nLightFlag;
	long nFileSeqNo;
	int32	nOdometer;
	int  bFileLog;
	int nBKLightTimeout;

	long reserved1;
	long reserved2;
	long reserved3;
	long reserved4;
 
}LOGGER_SETTINGS;

typedef struct
{
	TimeRecord tStartTime;
	TimeRecord tEndTime;
	int32	nDistance;
	float fMaxSpeed;
	float fMaxAltitude;
	float fMinAltitude;
}LOG_SESSION;

TimeRecord myrec;

#define CardInserted PIN_B0
int gActCard, gPrevCard;

#include "MyMMCFat32.h"
#include "MyMMCFat32.c"

HANDLE hFile=0xff;
char gfilename[32];

int nStillSecCount=0;	//Start counting when speed=0

typedef struct
{
	TimeRecord time;
	float latitude;
	//long latitude2;
	char northSouth;
	float longitude;
	//long longitude2;
	char eastWest;
	float speed;
	long direction;
	int numSat;	//Number of satellite
	float altitude;
}GPSDATA;

float fAccDistance=0;

GPSDATA recGPSData;

LOGGER_SETTINGS gSettings;
LOG_SESSION gLogSession;

#int_RB
void RB_isr() 
{
	g_nKeyNewState = input_b();

	if(input(PIN_B4) == 0)
	{
		
		nBackLightTimeout = gSettings.nBKLightTimeout;
		
		nFlushTimeout = 0;	//Force flush file buffer
		
	}
}

//200ms interrupt
#int_TIMER3
void TIMER3_isr() 
{
	set_timer3(15535);	//200ms
	output_high(PIN_G3);

	bGPSBusy = 0;

#ifdef 4XCLOCK
	if(++n200msTick > 20)
#else
	if(++n200msTick > 5)	//1sec
#endif
	{
		n200msTick = 0;
		if(nBackLightTimeout > 0 && nBackLightTimeout != 0xFF)
			nBackLightTimeout--;

		if(nGPSTimeout > 0)
			nGPSTimeout--;
		if(nFlushTimeout > 0)
			nFlushTimeout--;
		if(nMenuTimeout > 0)
			nMenuTimeout--;
	}
}

#int_RDA
void RDA_isr()
{
	int ch;
	char sCmp[4];
	
	bGPSBusy = 1;

	ch = getc();

	if(bGPRMC)
		return;

	if(ch == '$')	//Begining of GPS sentence
		nGPSReceived = 0;

	GPSBuffer[nGPSReceived++] = ch;


//	GPRMCBuffer[nGPRMCLen++] = ch;
//	GPRMCBuffer[nGPRMCLen] = 0;
	
	if(ch == 0xA || nGPSReceived >= GPS_BUFFER_LEN)
	{
		nGPRMCLen = 0;
		GPSBuffer[nGPSReceived] = 0;
		//fprintf(debug, "%5s\r\n", GPSBuffer);

		if(nGPSReceived < 10 || GPSBuffer[0] != '$')
		{
			nGPSReceived = 0;
			GPSBuffer[nGPSReceived] = 0;
			return;
		}
				

		strcpy(sCmp, sGPRMC);
		//if(GPSBuffer[3] == 'R') 
		if(strncmp(GPSBuffer+3, sCmp, 3) == 0)
		{
			strcpy(GPRMCBuffer, GPSBuffer);
			nGPRMCLen = nGPSReceived;
			bGPRMC = 1;
			return;
		}

		strcpy(sCmp, sGPGGA);
		if(strncmp(GPSBuffer+3, sCmp, 3) == 0)
		{
			strcpy(GPGGABuffer, GPSBuffer);
			nGPGGALen = nGPSReceived;
			bGPGGA = 1;					
			return;
		}

		//nGPSReceived = 0;
		GPSBuffer[nGPSReceived] = 0;
	
	}

}

#int_RDA2
void RDA2_isr()
{

}

#include "3310lcd.c"

float _strtod(char *str, int &nLen)
{
	float fResult, fTemp, fDiv;
	int bMinus;

	bMinus = 0;
	fResult = 0;
	nLen = 0;

	if(*str == '-')
	{
		bMinus = 1;
		nLen ++;
		str++;
	}
	else if(*str == '+')
	{
		nLen ++;
		str++;
	}

	while(*str >= '0' && *str <= '9')
	{
		fResult *= 10;
		fTemp = *str - '0';
		fResult += fTemp;
		str++;
		nLen++;
	}

	if(*str == '.')
	{
		nLen++;
		str++;
		fDiv = 10;
		while(*str >= '0' && *str <= '9')
		{
			fTemp = *str - '0';
			fResult += fTemp/fDiv;
			fDiv *= 10;
			str++;
			nLen++;
		}
	}

	if(bMinus)
		fResult *= -1;

	return fResult;	
}

long _strtoul(char *str, int &nLen)
{
	long lResult, lTemp;

	lResult = 0;
	nLen=0;

	while(*str >= '0' && *str <= '9')
	{
		lResult *= 10;
		lTemp = *str - '0';
		lResult += lTemp;
		str++;
		nLen++;
		if(nLen >= 5)	//Max 65535
			break;
	}
	
	return lResult;
}

signed long _strtol(char *str, int &nLen)
{
	signed long lResult, lTemp;
	char bMinus;

	lResult = 0;
	nLen=0;
	bMinus=0;
	
	if(*str == '-')
	{
		bMinus = 1;
		nLen++;
		str++;
	}
	else if(*str == '+')
	{
		nLen++;
		str++;
	}

	while(*str >= '0' && *str <= '9')
	{
		lResult *= 10;
		lTemp = *str - '0';
		lResult += lTemp;
		str++;
		nLen++;
		if(nLen >= 6)	//Max 65535
			break;
	}

	if(bMinus)
		lResult *= -1;
	return lResult;
}

//$GPGGA,070102.000,4911.1179,N,12247.2601,W,1,09,0.9,81.8,M,-16.7,M,,0000*52
int parseGPGGA(char *pGGA)
{
	int nComma, nLen;
	float fTemp;
	int bFixed;

	nLen = 0;
	nComma = 0;
	bFixed = 0;

	recGPSData.numSat = 0;
	recGPSData.altitude = 0;

	//Skip unused items
	while(*pGGA && nComma < 6)
	{
		if(*pGGA == ',')
			nComma ++;
		pGGA++;
	}

	if(nComma < 6)
		return FALSE;

	if(*pGGA == '1')
	{
		bFixed = 1;

		pGGA++;
		pGGA++;

		recGPSData.numSat = _strtoul(pGGA, nLen);
		pGGA += nLen+1;

		fTemp = _strtod(pGGA, nLen);
		pGGA += nLen+1;

		recGPSData.altitude = _strtod(pGGA, nLen);
	}

	return bFixed;
	
}

//$GPRMC,052457.000,A,4911.1152,N,12247.2628,W,0.00,16.90,290406,,,A*4C
int parseGPRMC(char *pRMC, int nLen)
{
	int nIdx;
	char *pCurr;

	if(nLen < 20)
		return FALSE;

	nIdx = 7;
	recGPSData.time.tm_hour = pRMC[nIdx++] - '0';
	recGPSData.time.tm_hour *= 10;
	recGPSData.time.tm_hour += pRMC[nIdx++] - '0';

	recGPSData.time.tm_min = pRMC[nIdx++] - '0';
	recGPSData.time.tm_min *= 10;
	recGPSData.time.tm_min += pRMC[nIdx++] - '0';

	recGPSData.time.tm_sec = pRMC[nIdx++] - '0';
	recGPSData.time.tm_sec *= 10;
	recGPSData.time.tm_sec += pRMC[nIdx++] - '0';	

	//Convert to local time
	recGPSData.time.tm_hour += 24;
	recGPSData.time.tm_hour += gSettings.nTimeZone;
	recGPSData.time.tm_hour %= 24;

	while(nIdx < nLen && pRMC[nIdx] != ',')
		nIdx++;

	nIdx++;
	if(nIdx >= nLen)
		return FALSE;


	if(pRMC[nIdx] != 'A')
		return FALSE;

	nIdx++;	//Skip A/V

	nIdx++;	//skip ','

	recGPSData.latitude = 0;
	//recGPSData.latitude2 = 0;
	recGPSData.longitude = 0;
	//recGPSData.longitude2 = 0;
	recGPSData.speed = 0.0;

	pCurr = pRMC + nIdx;

	recGPSData.latitude = _strtod(pCurr, nIdx);
	pCurr += nIdx + 1;

	if(*pCurr)
	{
		recGPSData.northSouth = *pCurr;	//North/South
		pCurr++;
		pCurr++;
	}
	else
		return FALSE;

	recGPSData.longitude = _strtod(pCurr, nIdx);
	pCurr += nIdx + 1;

	recGPSData.eastWest = *pCurr;	
	pCurr++;
	pCurr++;

	recGPSData.speed = _strtod(pCurr, nIdx);
	pCurr += nIdx + 1;

	recGPSData.direction = (int16)_strtod(pCurr, nIdx);
	pCurr += nIdx + 1;
	
	nIdx=0;
	recGPSData.time.tm_day = pCurr[nIdx++] - '0';
	recGPSData.time.tm_day *= 10;
	recGPSData.time.tm_day += pCurr[nIdx++] - '0';

	recGPSData.time.tm_mon = pCurr[nIdx++] - '0';
	recGPSData.time.tm_mon *= 10;
	recGPSData.time.tm_mon += pCurr[nIdx++] - '0';

	recGPSData.time.tm_year = pCurr[nIdx++] - '0';
	recGPSData.time.tm_year *= 10;
	recGPSData.time.tm_year += pCurr[nIdx++] - '0';	
	recGPSData.time.tm_year += 2000;	
	
	myrec.tm_year = recGPSData.time.tm_year;
	myrec.tm_mon = recGPSData.time.tm_mon;
	myrec.tm_day = recGPSData.time.tm_day;
	myrec.tm_mday = recGPSData.time.tm_day;
	myrec.tm_hour = recGPSData.time.tm_hour;
	myrec.tm_min = recGPSData.time.tm_min;
	myrec.tm_sec = recGPSData.time.tm_sec;

	recGPSData.speed *= 1.852;

	return TRUE;
	
}

void TestFat(int nIdx)
{
	HANDLE hFile;
	int16 i;
	int32 nfsize;
	char ch;

	sprintf(gfilename, "TEST%d.LOG", nIdx);
	hFile = fopen(gfilename, 'a');
	if(hFile!= 0)
	{
		printf("\r\nfailed to open file %s.", gfilename);
	}
	else
	{
		printf("\r\nFile opened.");
		for(i=0; i<200; i++)
		{
			sprintf(GPRMCBuffer, "FAT32 Test line %ld\r\n", i);
			fputstring(GPRMCBuffer, hFile);
		}
		
		fprintf(debug, "\r\nclose file");
		fclose(hFile);
	}
	fprintf(debug, "Get fize size\r\n");
	if(getfsize(gfilename, &nfsize))
	{
		fprintf(debug, "File not found.\r\n");
	}
	fprintf(debug, "Fize size %lu.\r\n", nfsize);

	fprintf(debug, "Open %s for read\r\n", gfilename);
	hFile = fopen(gfilename, 'r');
	if(hFile == 0)
	{
		while(nfsize-- > 0)
		{
			fgetch(&ch, hFile);
			fprintf(debug, "%c", ch);
		}
	
		fprintf(debug, "Close GPS.LOG\r\n");
		fclose(hFile);
	}
	fprintf(debug, "End of close GPS.LOG\r\n");
}

void LoadConfig()
{
	int i, nSize, *pBuf;
	nSize = sizeof(LOGGER_SETTINGS);
	pBuf = &gSettings;

	for(i=0; i<nSize; i++)
		*pBuf++ = read_ext_eeprom(i);
		
}

void SaveConfig()
{
	int i, nSize, *pBuf;
	nSize = sizeof(LOGGER_SETTINGS);
	pBuf = &gSettings;

	for(i=0; i<nSize; i++)
		write_ext_eeprom(i, *pBuf++);	
}

void ReadVoltage()
{
	long nBattVoltage;
	set_adc_channel(0);
	nBattVoltage = read_adc();
	gBattVoltage = nBattVoltage;
	//gBattVoltage*= 0.0045117;		//3.3v
	gBattVoltage *= 0.00408099;		//3.0v
}


void CheckButtonKeys()
{
	char nKeyChanged;
	nKeychanged = g_nKeyOldState^g_nKeyNewState;
	nKeyChanged&= 0xF0; 	//we have only four keys
	if(nKeyChanged)
	{
		nBackLightTimeout = gSettings.nBKLightTimeout;

		TRACE1("\r\nCheckButtonKeys:nKeyChanged = 0X%X.", nKeyChanged);
		if(SW_MENU & nKeyChanged)	//menu key
		{
			if(!(SW_MENU & g_nKeyNewState))	//Press down
			{
				if(g_nViewType == VT_MENU)
					g_nViewType = VT_INFO;
				else 
					g_nViewType = VT_MENU;
			}
		}
		if(SW_SELECT & nKeyChanged) //Select
		{
			if(!(SW_SELECT & g_nKeyNewState))	//Press down
			{
			if(g_nViewType == VT_MENU)
			{
				if(nMenuItem == 0)	//Enable/Disable logging
				{
					gSettings.bFileLog = !gSettings.bFileLog;
					if(!gSettings.bFileLog) //Turn off log
					{
						if(hFile != 0xFF)
						{
							fclose(hFile);
							hFile = 0xFF;
							sprintf(gfilename, "GPS%04lu.TXT", gSettings.nFileSeqNo-1);
							hFile = fopen(gfilename, 'w');
							if(hFile == 0)
							{
								printf(fputchar, "Distance=%luM\r\n", gLogSession.nDistance);
								printf(fputchar, "Maximum Speed=%5.1fkm/h\r\n", gLogSession.fMaxSpeed);
								printf(fputchar, "Minimum Altitude=%4.0fM\r\n", gLogSession.fMinAltitude);
								printf(fputchar, "Maximum Altitude=%5.0fM\r\n", gLogSession.fMaxAltitude);
								
								fclose(hFile);
								hFile = 0xFF;
							}
						}
					}
					else	//Turn on log
					{
						//gSettings.nFileSeqNo++;
					}
				}
				else if(nMenuItem == 1)	//set timezone
				{
					gSettings.nTimeZone++;
					if(gSettings.nTimeZone > 12)
						gSettings.nTimeZone = -12;
				}
				else if(nMenuItem == 2) //backlight
				{
					if(gSettings.nBKLightTimeout == 0xFF)
						gSettings.nBKLightTimeout = 0;
					else
						gSettings.nBKLightTimeout += 5;

					if(gSettings.nBKLightTimeout > 30)
						gSettings.nBKLightTimeout = 0xFF; //Always on
				}
			}
			}
		}

		if(SW_UP & nKeyChanged)	// up
		{
			if(!(SW_UP & g_nKeyNewState))	//press
			{
				if(g_nViewType == VT_INFO)	//Show infomation
				{
					if(gSettings.nInfoViewPage > 0)
						gSettings.nInfoViewPage--;
					else gSettings.nInfoViewPage = MAX_INFO_VIEW_PAGE -1;
				}
				else if(g_nViewType == VT_MENU)	//Show settings
				{
					if(nMenuItem > 0)
						nMenuItem--;
				}
			}
		}
		if(SW_DOWN & nKeyChanged) //Down
		{
			if(!(SW_DOWN & g_nKeyNewState))
			{
				if(g_nViewType == VT_INFO)	//Show infomation
				{
					gSettings.nInfoViewPage++;
					gSettings.nInfoViewPage %= MAX_INFO_VIEW_PAGE;
				}
				else if(g_nViewType == VT_MENU)	//Show settings
				{
					if(nMenuItem < 2)
						nMenuItem++;
				}
			}
		}
		g_nKeyOldState = g_nKeyNewState;

		DisplayView();

		SaveConfig();
	}
}

void DisplayInfoPage0()
{
	int nDegree, nMinute;
	float fSecond;
	
		nokia_clean_ddram();
		nokia_gotoxy(0,5);
		if (!gActCard) 
		{
			nokia_gotoxy(0,5);
			printf(nokia_printchar, "SDOK"); 
			nokia_gotoxy(54, 5);
			printf(nokia_printchar, "%s", sFATName[gFATType]);
			nokia_gotoxy(24,5);
			printf(nokia_printchar, "%luMB", DiskInfo.TotSec16!=0?DiskInfo.TotSec16/2000:DiskInfo.TotSec32/2000);
		}
		else
		{
			nokia_gotoxy(0,5);
			printf(nokia_printchar, "NOSD          "); 
		}
		
		nokia_gotoxy(0,0);
		printf(nokia_printchar, "%02d:%02d:%02d", recGPSData.time.tm_hour,recGPSData.time.tm_min, recGPSData.time.tm_sec);
		nokia_gotoxy(54, 0);
		printf(nokia_printchar, "GPS");
		nokia_gotoxy(0, 1);
		if(recGPSData.speed > 999.0)
			printf(nokia_printchar, "%5ldKMH, %3lu%c", (long)recGPSData.speed, recGPSData.direction, CHAR_DEGREE);
		else
			printf(nokia_printchar, "%5.1fKMH, %3lu%c", recGPSData.speed, recGPSData.direction, CHAR_DEGREE);
		nokia_gotoxy(0, 2);
		//printf(nokia_printchar, "%c%4lu.%lu", recGPSData.northSouth, recGPSData.latitude, recGPSData.latitude2);
		//printf(nokia_printchar, "%c %9.4f ", recGPSData.northSouth, recGPSData.latitude);
		nDegree = (int)(recGPSData.latitude/100);
		nMinute = (int)((long)recGPSData.latitude%100);
		fSecond = (recGPSData.latitude - (long)recGPSData.latitude)*60;
		
		printf(nokia_printchar, "%3d%c%02d'%05.2f\"%c", nDegree,CHAR_DEGREE, nMinute, fSecond, recGPSData.northSouth);
		
		nokia_gotoxy(0, 3);
		//printf(nokia_printchar, "%c%5lu.%lu", recGPSData.eastWest, recGPSData.longitude, recGPSData.longitude2);
		//printf(nokia_printchar, "%c%10.4f ", recGPSData.eastWest, recGPSData.longitude);
		nDegree = (int)(recGPSData.longitude/100);
		nMinute = (int)((long)recGPSData.longitude%100);
		fSecond = (recGPSData.longitude - (long)recGPSData.longitude)*60;
		
		printf(nokia_printchar, "%3d%c%02d'%05.2f\"%c", nDegree, CHAR_DEGREE, nMinute, fSecond, recGPSData.eastWest);
		
		if(hFile < MAXFILES)
		{
			nokia_gotoxy(60,4);
			printf(nokia_printchar, "%3luB", gFiles[hFile].posinsector);
		}

		nokia_gotoxy(0, 4);
		printf(nokia_printchar, "%5ldM", (signed long)recGPSData.altitude);	

		nokia_gotoxy(54, 0);
		printf(nokia_printchar, "GPS%02d", recGPSData.numSat);
	
		nokia_refresh();
}

void DisplayInfoPage1()
{
	signed long x1, x2, x3, x4, direction;
	nokia_clean_ddram();
	//Horizontal lines
	nokia_line(2, 0, 81, 0, 1);
	nokia_line(2, 23, 81, 23, 1);
	
	//Vertical lines
	nokia_line(2, 0, 2, 23, 1);	//Left border
	nokia_line(81, 0, 81, 23, 1);	//Right border
	nokia_line(42, 0, 42, 7, 1);	//Center
	nokia_line(42, 17, 42, 23, 1);	//Center
	nokia_line(22, 0, 22, 7, 1);	//1/4 divider
	nokia_line(22, 17, 22, 23, 1);	//1/4 divider
	nokia_line(62, 0, 62, 7, 1);	//3/4
	nokia_line(62, 17, 62, 23, 1);	//3/4

	direction = recGPSData.direction;

	x1 = -1; x2= -1; x3=-1; x4=-1;
	if(direction < 90)
		x1 = 40 - direction*78/180;
	else if(direction > 270)
		x1 = 40 + (360 - direction)*78/180;

	if(direction <= 180)
		x2 = 40 - (direction - 90)*78/180;


	if(direction >= 90 && direction <= 270)
		x3 = 40 - (direction - 180)*78/180;

	if(direction >= 180)
		x4 = 40 - (direction - 270)*78/180;
	else if(direction == 0)
		x4 = 0;

	
	
	if(x1 >= 0 && x1 < 84)
	{
		nokia_gotoxy(x1, 1);
		printf(nokia_printchar, "N");
	}

	if(x2 >= 0 && x2 < 84)
	{
		nokia_gotoxy(x2, 1);
		printf(nokia_printchar, "E");
	}

	if(x3 >= 0 && x3 < 84)
	{
		nokia_gotoxy(x3, 1);
		printf(nokia_printchar, "S");
	}

	if(x4 >= 0 && x4 < 84)
	{
		nokia_gotoxy(x4, 1);
		printf(nokia_printchar, "W");
	}
	
	//nokia_gotoxy(60, 3);
	//printf(nokia_printchar, "%ld", x2);
	//nokia_gotoxy(60, 4);
	//printf(nokia_printchar, "%ld", x3);
	//nokia_gotoxy(60, 5);
	//printf(nokia_printchar, "%ld", x4);

	if(recGPSData.direction >= 100)
	{
		nokia_gotoxy(33,3);
		printf(nokia_printchar, "%3lu", recGPSData.direction);
	}
	else if(recGPSData.direction >= 10)
	{
		nokia_gotoxy(36,3);
		printf(nokia_printchar, "%lu", recGPSData.direction);		
	}
	else
	{
		nokia_gotoxy(40,3);
		printf(nokia_printchar, "%lu", recGPSData.direction);		
	}

	nokia_gotoxy(0, 4);
	printf(nokia_printchar, "%5.1fKMH %5.1fKM", recGPSData.speed, (float)gLogSession.nDistance/1000.0);

	nokia_gotoxy(0, 5);
	printf(nokia_printchar, "%5ldM %5luKM", (signed long)recGPSData.altitude, gSettings.nOdometer/1000);	



//	nokia_circle(24,24,23,1);
//	nokia_line(24,24,24,1,1);

	nokia_refresh();
}

void DisplayInfoPage2()
{
		nokia_clean_ddram();
		nokia_gotoxy(0,0);
		printf(nokia_printchar, "%4.2fV", gBattVoltage);
		nokia_gotoxy(0,1);
		printf(nokia_printchar, "%s", gfilename);
		nokia_gotoxy(0,2);
		printf(nokia_printchar, "F Size:%lu", gFiles[0].wFileSize);
		nokia_gotoxy(0, 3);
		printf(nokia_printchar, "Max km/h:%ld", (long)gLogSession.fMaxSpeed);
		nokia_gotoxy(0, 4);
		printf(nokia_printchar, "Min Alt.:%ldm", (signed long)gLogSession.fMinAltitude); 
		nokia_gotoxy(0, 5);
		printf(nokia_printchar, "Max Alt.:%ldm", (signed long)gLogSession.fMaxAltitude); 
		nokia_refresh();
}

void DisplayMenuPage0()
{
	int y;
	y=0;

	nokia_clean_ddram();
	nokia_gotoxy(0,y);

	if(nMenuItem == y)
		nokia_char_invert(1);

	printf(nokia_printchar, "Log Status ");
	if(gSettings.bFileLog)
		printf(nokia_printchar, " On");
	else
		printf(nokia_printchar, "Off");

	nokia_char_invert(0);

//Time zone
	y++;
	if(nMenuItem == y)
		nokia_char_invert(1);
	nokia_gotoxy(0, y);
	printf(nokia_printchar, "Timezone   %3d",gSettings.nTimeZone);
	nokia_char_invert(0);

//Backlight
	y++;
	if(nMenuItem == y)
		nokia_char_invert(1);
	nokia_gotoxy(0, y);
	printf(nokia_printchar, "Backlight  ");
	
	if(gSettings.nBKLightTimeout == 0)
		printf(nokia_printchar, "Off"); //Always off
	else if(gSettings.nBKLightTimeout == 0xff)
		printf(nokia_printchar, " On");	//Always on
	else
		printf(nokia_printchar, "%2us", gSettings.nBkLightTimeout);

	nokia_char_invert(0);

	nokia_refresh();
}

void DisplayView()
{
	if(g_nViewType == VT_INFO)	
	{
		switch(gSettings.nInfoViewPage)
		{
			case 0:
				DisplayInfoPage0();
				break;
			case 1:
				DisplayInfoPage1();
				break;
			case 2:
				DisplayInfoPage2();
				break;
		}
	}
	else //display menu
	{
		if(nMenuItem < 6)
		{
			DisplayMenuPage0();
		}
		else if(nMenuItem < 12)
		{
		}
	}
}

//#ORG 0x6000//, 0x62FF
void main()
{
	long i;
	int error;
	char bSDReady;
	int nCount;
	int bGPSFixed;
	
	nCount = 11;

	GPSFlag = 0;
	GPRMCBuffer[0] = 0;
	GPGGABuffer[0] = 0;
	gBattVoltage = 0;

	memset(gFiles, 0, sizeof(FILE));
	gFiles[0].wFileSize = 0;
	gFilename[0] = 0;

	memset(&recGPSData, 0, sizeof(GPSDATA));
	recGPSData.northSouth = 'N';
	recGPSData.eastWest = 'E';

	memset(&gLogSession, 0, sizeof(LOG_SESSION));
	gLogSession.fMinAltitude = 10000;


#ifdef 4XCLOCK
	OSCTUNE=0x40;
#endif

	bSDReady = 0;

	g_nKeyOldState = 0xFF;
	g_nKeyNewState = 0xFF;

	LoadConfig();
	if(gSettings.nFlag != 0x55)
	{
		gSettings.nFlag = 0x55;
		gSettings.nTimeZone = -7;
		gSettings.nBKLightLevel = 50;
		gSettings.nInfoViewPage = 0;
		gSettings.nLightFlag = 0x07;
		gSettings.nFileSeqNo = 0;
		gSettings.nOdometer = 0;
		gSettings.bFileLog = 0;
		gSettings.nBKLightTimeout = 15; 

		gSettings.reserved1 = 0;
		gSettings.reserved2 = 0;
		gSettings.reserved3 = 0;
		gSettings.reserved4 = 0;
		SaveConfig();
	}


	setup_adc_ports(AN0|VSS_VDD);
	setup_adc(ADC_CLOCK_INTERNAL|ADC_TAD_MUL_0);
	setup_psp(PSP_DISABLED);
	//setup_spi(SPI_MASTER|SPI_L_TO_H|SPI_CLK_DIV_4);
	set_tris_c(0b10010011); //c7=rx I, c6=tx O, c5 SDO O,c4 SDI I
	SETUP_SPI(SPI_MASTER | SPI_CLK_DIV_4 | SPI_H_TO_L |SPI_XMIT_L_TO_H );

	setup_spi2(FALSE);
	setup_wdt(WDT_OFF);
	setup_timer_0(RTCC_INTERNAL);
	setup_timer_1(T1_DISABLED);
	setup_timer_2(T2_DIV_BY_1,255,1);
	//setup_timer_3(T3_DISABLED|T3_DIV_BY_1);

	setup_timer_3(T3_INTERNAL|T3_DIV_BY_8);
	set_timer3(15535);	//200ms

	setup_timer_4(T4_DISABLED,0,1);
	setup_comparator(NC_NC_NC_NC);
	setup_vref(FALSE);
	enable_interrupts(INT_RDA);
	RBPU = 0;
	enable_interrupts(INT_RB);
   	enable_interrupts(INT_TIMER3);
//	enable_interrupts(INT_RDA2);
	enable_interrupts(GLOBAL);
	setup_oscillator(False);


	setup_ccp2(CCP_PWM);
	setup_ccp3(CCP_PWM);
//	setup_ccp4(CCP_PWM);

	setup_comparator(NC_NC_NC_NC);
	setup_vref(FALSE);

	ReadVoltage();

	printf("GPSLogger\r\n");


	output_high(PIN_E7);
	output_high(PIN_G0);
	output_high(PIN_G3);

	set_pwm2_duty(100);
	set_pwm3_duty(80);
//	set_pwm4_duty(80);

	//Init Nokia 3310 LCD
	nokia_init();

	nokia_clean_ddram();

	nokia_gotoxy(12, 2);
	printf(nokia_printchar, "GPS Logger");
	nokia_gotoxy(12, 3);
	printf(nokia_printchar, "%s", __DATE__);
	nokia_refresh();

	delay_ms(2000);

	nokia_clean_ddram();
	nokia_gotoxy(0,0);
	printf(nokia_printchar, "00:00:00");
	nokia_gotoxy(0,1);
	printf(nokia_printchar, "0KMH");
	nokia_gotoxy(0, 2);
	printf(nokia_printchar, "N000.00");
	nokia_gotoxy(0, 3);
	printf(nokia_printchar, "E000.00");

	nokia_gotoxy(0,5);
	printf(nokia_printchar, "NOSD"); 
	nokia_refresh();
	

	i=0;
	gPrevCard = 1;

	hFile = 0xff;

	while(1)
	{
		if(nBackLightTimeout > 0)
		{
			set_pwm2_duty(500);
			//set_pwm3_duty(100);
			//set_pwm4_duty(100);
		}
		else
		{
			set_pwm2_duty(0);
//			set_pwm3_duty(0);
//			set_pwm4_duty(0);
//			output_high(PIN_G0);
//			output_high(PIN_G3);
		}

		gActCard = input(CardInserted); 

		if(gActCard)	
		{
			if(gPrevCard == 0 && hFile != 0xFF)	//Card will be pulled out while a file is still open
			{
				fclose(hFile);
				hFile = 0xFF;
			}
			bSDReady = 0;
		}

		if (gActCard == 0 && gPrevCard != 0)  // card was pulled out then pushed back now 
		{
			bSDReady = 0;
			error  = 1;

			SaveConfig();

			set_tris_c(0b10010011); //c7=rx I, c6=tx O, c5 SDO O,c4 SDI I
			SETUP_SPI(SPI_MASTER | SPI_CLK_DIV_4 | SPI_H_TO_L |SPI_XMIT_L_TO_H );

			TRACE0("\r\nSD card Inserted.");

			delay_ms(50); 
			for(i=0; i<10&&error; i++)
			{
				TRACE1("\r\nInit SD (%ld)...", i); 
				error = init_mmc(10);
				delay_ms(50); 
			}  

			if(!error)
			{
				TRACE0("\r\nSD Init Ok"); 
			}
			else
				TRACE0("\r\nSD Init Failed."); 

			if(InitFAT() != MMC_NO_ERR)
			{
				TRACE1("\r\n Failed to initFat, error code = 0x%02X.", gFATErrCode);
			}


			if(!error)
			{	
				hFile = 0xFF;
				bSDReady = 1;
			}					

		} //Card inserted

		if(bGPGGA && bGPRMC)	//Process GPRMC sentence
		{
			nMenuTimeout = 0;

			//fprintf(debug, "Receive %s", GPRMCBuffer);
			bGPSFixed = parseGPGGA(GPGGABuffer);

			if(parseGPRMC(GPRMCBuffer, nGPRMCLen))
			{	
				//Write log
				if(bSDReady && gSettings.bFileLog)
				{
					//Create new log if file is not openned yet
					if(hFile!= 0)
					{
						sprintf(gfilename, "GPS%04lu.LOG",gSettings.nFileSeqNo++);
						hFile = fopen(gfilename, 'a');
						if(hFile == 0)
						{
							nFlushTimeout = 60;
							gLogSession.tStartTime = recGPSData.time;
							gLogSession.nDistance = 0;
							gLogSession.fMinAltitude = 10000.0;
							gLogSession.fMaxSpeed = 0;
							gLogSession.fMaxAltitude = 0;
							//fprintf(debug, "File opened, datasec=%lu\r\n", gFiles[hFile].CurrentCluster*DiskInfo.SecPerClus + gFAT32Vars.gFirstDataSector);
						}
					}

					if(hFile == 0)	//File opened
					{
						//Log GPS sentence every 1 minute if speed is 0kmh
						if(recGPSData.speed < 0.01)
						{
							nStillSecCount++;
							nStillSecCount %= 60;
						}
						else
						{
							nStillSecCount = 1;
						}
						
						if(nStillSecCount == 1)
						{
							output_low(PIN_G3);
							
							fputstring(GPRMCBuffer, hFile);
							fputstring(GPGGABuffer, hFile);
						}
					}
				}
				
			}
			else
			{
				//nokia_gotoxy(54, 0);
				//printf(nokia_printchar, "GPSNA");
			}

			bGPGGA = 0;
			nGPSReceived = 0;

			bGPRMC = 0;
			nGPSReceived = 0;
			nGPSTimeout = 10;	

			if(bGPSFixed)	//Update maximum data when position fixed (valid)
			{
				if(gLogSession.fMinAltitude > recGPSData.altitude)
					gLogSession.fMinAltitude = recGPSData.altitude;
				if(gLogSession.fMaxAltitude < recGPSData.altitude)
					gLogSession.fMaxAltitude = recGPSData.altitude;
				if(gLogSession.fMaxSpeed < recGPSData.speed)
					gLogSession.fMaxSpeed = recGPSData.speed;
			}
					
			fAccDistance += recGPSData.speed*10/36;
			if(fAccDistance > 1.0)
			{
				i = (long)fAccDistance;
				gLogSession.nDistance += i;
				gSettings.nOdometer += i;
				fAccDistance -= i;
			}
		}

		if(nGPSTimeout == 0)
		{
			nokia_gotoxy(54, 0);
			printf(nokia_printchar, "NOGPS");
			nGPSTimeout = 10;
		}

		if(nFlushTimeout == 0 &&hFile == 0)
		{
			TRACE0("\r\nFlush file buffer.");
			fflush(hFile);
			nFlushTimeout = 60;	//Flush buffer every 1 minute, or when function button pressed
		}

		gPrevCard = gActCard; 
		
		//Check buttons (menu, up, down, select)
		CheckButtonKeys();


		if(nMenuTimeout == 0)
		{
			ReadVoltage();

/*
			if(gBattVoltage < BATT_LOW_SHUT_VOLT)
			{
				nokia_clean_ddram();
				nokia_gotoxy(0, 2);
				printf(nokia_printchar, "Battery low");
				printf(nokia_printchar, "GPS Log shutdown");
				nokia_refresh();
				if(hFile != 0xFF)
				{
					fclose(hFile);
					sleep();
				}
			}
*/
			DisplayView();
			nMenuTimeout = 1;
		}
	}

}
