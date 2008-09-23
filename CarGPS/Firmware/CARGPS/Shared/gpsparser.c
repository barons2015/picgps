
#ifndef LOCAL_TIMEZONE
#define LOCAL_TIMEZONE	8
#endif

#define GPS_BUFFER_LEN	128

char GPSBuffer[GPS_BUFFER_LEN+1];
int nGPSReceived=0;

char GPSFlag=0;

#bit bGPRMC=GPSFlag.0
#bit bGPGGA=GPSFlag.1
#bit bGPCTS=GPSFlag.2
#bit bGPSDataIn = GPSFlag.3


//Buffer for each sentence

char GPRMCBuffer[GPS_BUFFER_LEN+1]="";
int nGPRMCLen=0;
char GPCTSBuffer[50];
int nGPCTSLen=0;
const char sGPRMC[]="RMC";
const char sGPCTS[]="CTS";	//Car Tracking System. "$GPCTS,1234,E*00" 1234->Car Module ID

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

typedef struct
{
	TimeRecord time;
	int bFixed;
	float latitude;
	char northSouth;
	float longitude;
	char eastWest;
	long nSpeed;
	float fSpeed;
	long direction;
	int numSat;	//Number of satellite
	float altitude;
	char sCTSID[6];	//device ID from CTS sentence
}GPSDATA;

GPSDATA recGPSData;

//Serial port interrupt, high priority
#int_RDA HIGH
void RDA_isr()
{
	char ch;
	char sCmp[4];

	
	ch = fgetc(GPS_STREAM);
	

#ifdef GPS_FORWARD
	if(g_bForwardGPS)
		fputc(ch, GPS_FORWARD);
#endif

	bGPSDataIn = 1;

	if(bGPRMC)
		return;
		

	if(ch == '$')	//Begining of GPS sentence '$'
	{
		nGPSReceived = 0;
		bGPSDataIn = 1;
	}	


	GPSBuffer[nGPSReceived++] = ch;
	
	if(ch == 0xA || nGPSReceived >= GPS_BUFFER_LEN)
	{
		GPSBuffer[nGPSReceived] = 0;

		if(nGPSReceived < 10  || GPSBuffer[0] != '$')
		{
			nGPSReceived = 0;
			GPSBuffer[0] = 0;
			return;
		}

		strcpy(sCmp, sGPRMC);
		if(strncmp(GPSBuffer+3, sCmp, 3) == 0)
		{
			strcpy(GPRMCBuffer, GPSBuffer);
			nGPRMCLen = nGPSReceived;
			GPRMCBuffer[nGPRMCLen] = 0;
			bGPRMC = 1;
			return;
		}

		strcpy(sCmp, sGPCTS);
		if(strncmp(GPSBuffer+3, sCmp, 3) == 0)
		{
			strcpy(GPCTSBuffer, GPSBuffer);
			nGPCTSLen = nGPSReceived;
			bGPCTS = 1;					
			return;
		}
		
		nGPSReceived = 0;
	}

}

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

int parseGPCTS(char *pCTS)
{
	int nComma, nLen;
	nLen = 0;
	nComma = 0;

	//Skip unused items
	while(*pCTS && nComma < 2)
	{
		if(*pCTS == ',')
			nComma ++;

		pCTS++;

		if(nComma == 1 && nLen < 4)
		{
			recGPSData.sCTSID[nLen++] = *pCTS;
		}
	}

	recGPSData.sCTSID[nLen] = 0;

	if(nComma < 1)
		return FALSE;

	return TRUE;

}

//$GPGGA,070102.000,4911.1179,N,12247.2601,W,1,09,0.9,81.8,M,-16.7,M,,0000*52
int parseGPGGA(char *pGGA)
{
	int nComma, nLen;
	float fTemp;

	nLen = 0;
	nComma = 0;

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
	{
		recGPSData.bFixed = 0;
		return FALSE;
	}	

	if(*pGGA == '1')
	{
		recGPSData.bFixed = 1;

		pGGA++;
		pGGA++;

		recGPSData.numSat = _strtoul(pGGA, nLen);
		pGGA += nLen+1;

		fTemp = _strtod(pGGA, nLen);
		pGGA += nLen+1;

		recGPSData.altitude = _strtod(pGGA, nLen);
	}
	else
	{
		recGPSData.bFixed = 0;
		recGPSData.numSat = 0;
	}	

	return recGPSData.bFixed;

}

//$GPRMC,052457.000,A,4911.1152,N,12247.2628,W,0.00,16.90,290406,,,A*4C
int parseGPRMC(char *pRMC, int nLen)
{
	int nIdx;
	char *pCurr;
	signed int nLocalHour;

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
	nLocalHour = recGPSData.time.tm_hour;
	nLocalHour += LOCAL_TIMEZONE;
	
	recGPSData.time.tm_hour = (nLocalHour + 24)%24;

	while(nIdx < nLen && pRMC[nIdx] != ',')
		nIdx++;

	nIdx++;
	if(nIdx >= nLen)
		return FALSE;


	if(pRMC[nIdx] != 'A')	//Fixed? 'A'= Active (Fixed), 'V' = Invalid(Not fixed)
	{
		recGPSData.bFixed = 0;
		//Reset speed
		recGPSData.latitude = 0;
		recGPSData.longitude = 0;
		recGPSData.fSpeed = 0.0;
		recGPSData.nSpeed = 0;
		return FALSE;
	}
	else	
		recGPSData.bFixed = 1;

	nIdx++;	//Skip A/V

	nIdx++;	//skip ','

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

	recGPSData.fSpeed = _strtod(pCurr, nIdx);
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

	recGPSData.fSpeed *= 1.852;
	recGPSData.nSpeed = (long)recGPSData.fSpeed;

	return TRUE;

}