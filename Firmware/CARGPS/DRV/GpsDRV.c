#include "GpsDRV.h"
#BYTE OSCTUNE=0xF9B
#BIT PLLEN=OSCTUNE.6


//#define RESET_BASE			0x0000				//0x0000:No bootloader. 
#define RESET_BASE			0x2000				//0x2000: work with USB HID boot loader
//#define RESET_BASE			0x5000				//0x5000: Work with SD file bootloader
#define CONFIG_ADDRESS	RESET_BASE-0x400	//configuration block, won't be erased by boot loader (under 0x2000 is protected)
#define USER_CODE_BASE		RESET_BASE + 0x100	//Leave room for vector and bootup process
#define USB_CODE_BASE		0xD000				//USB Mass Storage Device code 
#define USER_VECTOR_BASE	USER_CODE_BASE+8	//App vector 


#build(reset=USER_CODE_BASE , interrupt=USER_VECTOR_BASE) // Move the reset and interrupt vectors 
#org RESET_BASE, RESET_BASE+7
void _init()
{
	goto_address(RESET_BASE+0x20);	//_select_application()
}

#org RESET_BASE+8,RESET_BASE+0x17
void _high_int()
{
	goto_address(USER_VECTOR_BASE);
}

#org RESET_BASE+0x18,RESET_BASE+0x1F
void _low_int()
{
	goto_address(USER_VECTOR_BASE+0x10);
}	

#org RESET_BASE+0x20,RESET_BASE+0xFF	
void _select_application() 
{
	PLLEN=1; //Enable 48MHz PLL
	
	delay_ms(100);
	if(input(PIN_C7))			//Connected to CARGPS?
		goto_address(USER_CODE_BASE);	//GPS logger application

	delay_ms(500);	
	if(input(PIN_C7))			//Connected to CARGPS?
		goto_address(USER_CODE_BASE);	//GPS logger application
	else
		goto_address(USB_CODE_BASE);	//USB application
}

//Protect the lower memory for HID bootloader
#if RESET_BASE
#ORG 0x0000,RESET_BASE-1 {}
#endif

#org default

#include "string.h"
#include "stdlib.h"

//IO pin definition
#define BATT_EN		PIN_E1	//output high to connect battery
#define LED_GRN		PIN_E0	//output low to turn of green LED
#define SD_DETECT	PIN_G3	//input low if SD is inserted
#define CardInserted SD_DETECT
#define NO_MMC_CARD	input(CardInserted)

#define LED_GREEN_ON	output_low(LED_GRN)
#define LED_GREEN_OFF	output_high(LED_GRN)
#define BATTERY_ON		output_high(BATT_EN)
#define BATTERY_OFF		output_low(BATT_EN)


#ORG default

long nBattVoltage;
float gBattVoltage;

#define BATT_LOW_WARN_VOLT	3.5	//Battery low warning voltage
#define BATT_LOW_SHUT_VOLT	3.4	//Shutdown voltage

#define LOG_FLAG_DISCARD_ZERO_KHM	0x01

typedef struct  {
	unsigned long    tm_year;
	char            tm_mon;
	char            tm_day;
	char            tm_mday;
	char            tm_hour;
	char            tm_min;
	char            tm_sec;
} TimeRecord;
TimeRecord myrec;

int nLogInterval=0;

typedef struct 
{
	char nFlag;
	long nFileSeqNo;
	signed int nTimeZone;
	char sDrvID[5];			//Driver module ID
	int bLogFixedOnly;		//1: start logging when GPS data is fixed.
	int nLogInterval;
	
	long reserved1;
	long reserved2;
	long reserved3;
	long reserved4;

}LOGGER_SETTINGS;
LOGGER_SETTINGS gSettings;


int gActCard, gPrevCard;

#include "..\shared\MyMMCFat32.h"
#include "..\shared\MyMMCFat32.c"

HANDLE hFile=0xff;
char gfilename[32];

int nStillSecCount=0;	//Start counting when speed=0


//Timers
int n10msTick=0;
int nHeartBeatTick=0;
int nGPSBusyTick=0;
int16 nNoGPSTick=0;

//10ms interrupt
#int_TIMER0
void TIMER0_isr() 
{
	set_timer0(5560);   //10ms

	//Send out CAR ID when nHeartBeatTick is 0
	if(nHeartBeatTick)
		nHeartBeatTick--;

	//Turn on/off LED when data comes in
	if(nGPSBusyTick)
		nGPSBusyTick--;
		
	nNoGPSTick++;

	if(++n10msTick > 100)	//Get 1sec timer
	{
		n10msTick = 0;
	}

}

#define LOCAL_TIMEZONE gSettings.nTimeZone
#include "..\shared\gpsParser.c"

//Load settings from program flash memory
void LoadConfig()
{
	int nSize, *pBuf;
	nSize = sizeof(LOGGER_SETTINGS);
	pBuf = &gSettings;

	read_program_memory(CONFIG_ADDRESS, &gSettings, nSize);
}

//Save settings to program flash memory
void SaveConfig()
{
	int nSize, *pBuf;
	nSize = sizeof(LOGGER_SETTINGS);
	pBuf = &gSettings;

	write_program_memory(CONFIG_ADDRESS, &gSettings, nSize);
}

char sIniFile[] = "GPSDRV.INI";
char STR_INI_ID[] = "ID";
char STR_INI_TIMEZONE[] = "TIMEZONE";
char STR_INI_LOGINTERVAL[] = "LOGINTERVAL";
char STR_INI_LOGFIXEDONLY[] = "LOGFIXEDONLY";

int ReadIni(char *szIniFile)
{
#define INI_ENTRY_MAX_LEN	32
	char sName[INI_ENTRY_MAX_LEN+1], sValue[INI_ENTRY_MAX_LEN+1];
	int nNameLen, nValueLen;
	char ch;
#define INI_ENTRY_NAME	0
#define INI_ENTRY_VALUE	1
	int nType;
	int bEndOfFile;
	
	HANDLE hFile;
	
	hFile = fopen(szIniFile, 'r');
	if(hFile != 0)
	{
		return 0;
	}	
	
	nType = INI_ENTRY_NAME;
	nNameLen = 0;
	nValueLen = 0;
	bEndOfFile = 0;
	while(!bEndOfFile)
	{	
		bEndOfFile = !fgetch(&ch, hFile);
		if(ch == '=')
		{
			sName[nNameLen] = 0;
			nType = INI_ENTRY_VALUE;
			continue;
		}
		else if(ch == '\n' || bEndOfFile)	//End of line
		{
			sValue[nValueLen] = 0;
			if(strcmp(sName, STR_INI_ID) == 0)	//Device ID
			{
				strncpy(gSettings.sDrvID, sValue, 4);
			}	
			else if(strcmp(sName, STR_INI_TIMEZONE) == 0)	//Time zone, -12 ~ +12
			{
				gSettings.nTimeZone = atoi(sValue);
			}	
			else if(strcmp(sName, STR_INI_LOGINTERVAL) == 0)	//GPS Sentence logging interval, 1~255
			{
				gSettings.nLogInterval = atoi(sValue);
			}	
			else if(strcmp(sName, STR_INI_LOGFIXEDONLY) == 0)	//0 if want to log GPS sentence even it's not fixed yet
			{
				gSettings.bLogFixedOnly = atoi(sValue);
			}
			
			//Reset entry buffer
			nType = INI_ENTRY_NAME;
			nNameLen = 0;
			nValueLen = 0;	
			sName[0] = 0;
			sValue[0] = 0;
		}	
		else if(ch == '\r' || ch == ' ' || ch == '\t')	
		{
		}	
		else
		{
			if(nType == INI_ENTRY_NAME)
			{
				if(nNameLen < INI_ENTRY_MAX_LEN)
					sName[nNameLen++] = ch;
			}	
			else
			{
				if(nValueLen < INI_ENTRY_MAX_LEN)
					sValue[nValueLen++] = ch;
			}	
			
		}	
	}	
	fclose(hFile);
	hFile = 0xFF;
	
	return 1;
}	

void ReadVoltage()
{
	set_adc_channel(0);
	nBattVoltage = read_adc();
	//gBattVoltage = nBattVoltage;
	//gBattVoltage *= 0.00586;		//3.0v
}

void ClearUSARTError()
{
	int i;
	//OverRun error
	if(OERR1)
	{
		CREN1=0;
		CREN1=1;
	}	
	
	//Frame error
	if(FERR1)
	{
		i = RCREG1;
	}	
		
	//OverRun error
	if(OERR2)
	{
		CREN2=0;
		CREN2=1;
	}
	//Frame error
	if(FERR2)
	{
		i = RCREG2;
	}	
}

void blinkLED(int nTimes, int nOnms, int nOffms)
{
	int i;
	for(i=0; i<nTimes; i++)
	{
			LED_GREEN_ON;
			delay_ms(nOnms);
			LED_GREEN_OFF;
			delay_ms(nOffms);
	}	
}	
	
#BYTE CCP1CON=0xFBD
#BYTE UCFG=0xF6F
#BIT UPUEN=UCFG.4
#BIT FSEN=UCFG.2

void main()
{
	long i;
	int error;
	char bSDReady;
	int bLogToFile;	//Set to 1 to enable loggin

	PLLEN=1; //Enable 48MHz PLL
	
	bLogToFile = 1;

	gBattVoltage = 0;

	memset(gFiles, 0, sizeof(FILE));
	gFiles[0].wFileSize = 0;
	gFilename[0] = 0;

	memset(&recGPSData, 0, sizeof(GPSDATA));
	recGPSData.northSouth = 'N';
	recGPSData.eastWest = 'E';

	LED_GREEN_OFF;

	//Load settings from program flash
	LoadConfig();

	if(gSettings.nFlag != 0x56)
	{
		gSettings.nFlag = 0x56;
		strcpy(gSettings.sDrvID, "0001");
		gSettings.nFileSeqNo = 0;
		gSettings.nTimeZone = 8;
		gSettings.bLogFixedOnly = 0;
		gSettings.nLogInterval = 3;

		gSettings.reserved1 = 0;
		gSettings.reserved2 = 0;
		gSettings.reserved3 = 0;
		gSettings.reserved4 = 0;
		SaveConfig();
	}


	setup_adc_ports(sAN0|VSS_VDD);
	setup_adc(ADC_CLOCK_INTERNAL|ADC_TAD_MUL_0);
	setup_psp(PSP_DISABLED);


	//Setup SPI for SD card
	SPEN1=1;
	SPEN2=1;
	set_tris_c(0b10010011); //c7=rx I, c6=tx O, c5 SDO O,c4 SDI I
	SETUP_SPI(SPI_MASTER | SPI_CLK_DIV_4 | SPI_H_TO_L |SPI_XMIT_L_TO_H );

	setup_spi2(FALSE);
	setup_wdt(WDT_OFF);
	setup_timer_0(RTCC_INTERNAL);
	setup_timer_1(T1_DISABLED);
	setup_timer_2(T2_DIV_BY_1,255,1);
	setup_timer_3(T3_INTERNAL|T3_DIV_BY_8);
	set_timer3(15535);	//200ms

	setup_timer_4(T4_DISABLED,0,1);
	setup_comparator(NC_NC_NC_NC);
	setup_vref(FALSE);


	//Enable serial port interrupt
	enable_interrupts(INT_RDA);

	set_timer0(5560);    //10ms interrupt
	enable_interrupts(INT_TIMER0);

	//	enable_interrupts(INT_RDA2);
	enable_interrupts(GLOBAL);

	setup_comparator(NC_NC_NC_NC);
	setup_vref(FALSE);

	i=0;
	gPrevCard = 1;

	hFile = 0xff;

	nGPSBusyTick = 0;

	for(i=0; i<5 && !bGPCTS; i++)
	{
		//Startup notification
		LED_GREEN_ON;
		delay_ms(200);

		LED_GREEN_OFF;
		delay_ms(200);
	}

	
	//Initiate uSD card
	gActCard = input(CardInserted); 
	bSDReady = 0;
	hFile = 0xFF;
	error  = 1;
	{
		if (gActCard == 0)  //SD card is inserted
		{

			set_tris_c(0b10010011); //c7=rx I, c6=tx O, c5 SDO O,c4 SDI I
			SETUP_SPI(SPI_MASTER | SPI_CLK_DIV_4 | SPI_H_TO_L |SPI_XMIT_L_TO_H );

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
				if(InitFAT() != MMC_NO_ERR)
				{
					TRACE1("\r\n Failed to initFat, error code = 0x%02X.", gFATErrCode);
					error = 1;
				}
			}
			else
				TRACE0("\r\nSD Init Failed."); 


			if(!error)
			{	
				bSDReady = 1;
			}					

		} //Card inserted
		
	}
	
	if(bSDReady)	//SD init ok. Read ini file if there is one.
	{
		//Make backup battery online
		BATTERY_ON;	
		
		if(ReadIni(sIniFile))
		{
			SaveConfig();
			remove(sIniFile);
		}	
	}
	else	//Failed to init SD card. Notify driver and stop app.
	{
		//Blinks 3 times every sec
		for(i=0; i<30; i++)
		{

			//Blinks LED 3 times, on 100ms, off 100ms
			blinkLED(3, 100, 100);
			delay_ms(400);			
			
		}	
		
		sleep();
	}		

		for(i=0; i<5; i++)
		{

			//Blinks LED 2 times, on 100ms, off 100ms
			blinkLED(2, 100, 100);
			delay_ms(600);			
			fprintf(MDLCAR, "DRV%s,%d\r\n", gSettings.sDrvID,bSDReady);
			
		}	

	
	//Main loop
	while(1)
	{
	
		//clear any USART error
		ClearUSARTError();

		
		if(bGPSDataIn)	//GPS data has been received.Flag set in INT_RDA()
		{
			nGPSBusyTick = 10;
			nNoGPSTick = 0;
			bGPSDataIn = 0;	//clear flag
		}	
	
		if(nGPSBusyTick)
		{
			if(bSDReady)
				LED_GREEN_OFF;
			else
				LED_GREEN_ON;
		}
		else
		{
			if(bSDReady)
				LED_GREEN_ON;
			else
				LED_GREEN_OFF;
		}

		//Communicate with Car module every 1s
		if(nHeartBeatTick == 0)
		{
			fprintf(MDLCAR, "DRV%s,%d\r\n", gSettings.sDrvID,bSDReady);
			//fprintf(MDLCAR, "$GPDRV,%s,%d,E*00\r\n", gSettings.sDrvID,bSDReady);
			nHeartBeatTick = 100;	//100*10ms
		}

		//ReadVoltage();
		//if(nBattVoltage < 700)	//Unplug from Car module?
		if(nNoGPSTick > 300)	//More than 2sec there's no GPS signal received
		{
			//Close file
			if(hFile != 0xFF)	
			{
				fclose(hFile);
			}	

			//Save settins to EEPROM
			SaveConfig();

			//notify user we are going to shut down
			for(i=0; i<10; i++)	
			{
				LED_GREEN_ON;
				delay_ms(100);
				LED_GREEN_OFF;
				delay_ms(100);
			}	

			//Disconnect from backup battery
			BATTERY_OFF;	
			sleep();
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

		if(bGPCTS)	//Get command from Car module
		{
			parseGPCTS(GPCTSBuffer);
			bGPCTS = 0;
			if(hFile != 0xFF)	//Close file since leader module is around
			{
				fclose(hFile);
				hFile = 0xFF;
			}		
		}

		if(bGPRMC)	//GPGGA & GPRMC sentence received?
		{
			//Parse GPS sentences
			parseGPRMC(GPRMCBuffer, nGPRMCLen);


			if(recGPSData.bFixed || !gSettings.bLogFixedOnly)	//Log sentence when position is fixed, or log all sentence for debugging
			{	
				//Write log if SD card is ready and loggin is enabled
				if(bSDReady && bLogToFile)
				{
					//Create new log if file is not openned yet
					if(hFile!= 0)
					{
						sprintf(gfilename, "%s_%03lu.LOG",gSettings.sDrvID, gSettings.nFileSeqNo++);
						
						//Range of nFileSeqNo: 0~999
						gSettings.nFileSeqNo %= 1000;
						
						hFile = fopen(gfilename, 'a');
						if(hFile == 0)
						{
							printf(fputchar, "$GPCTS,%s,%s,E*00\r\n", recGPSData.sCTSID,gSettings.sDrvID);
						}
						else	//Failed to create file, notify driver and stop app.
						{
							BATTERY_OFF;
							//Blinks 4 times per second
							for(i=0; i<30; i++)
							{
								blinkLED(4, 80, 70);
								delay_ms(1000-4*(80+70));
							}	
							sleep();
						}	
					}

					if(hFile == 0)	//File opened
					{
						//Log GPS sentence every 1 minute if speed is 0kmh
						if(recGPSData.nSpeed < 1 && gSettings.bLogFixedOnly)
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
							nLogInterval = nLogInterval + 1;
							if(nLogInterval >= gSettings.nLogInterval)
							{
								nLogInterval = 0;
								//Write GPS sentence to log file
								fputstring(GPRMCBuffer, hFile);
							}	
						}
					}
				}//if(bSDReady && bLogToFile)

			}	//if(bGPSFixed)


			bGPRMC = 0;	//current GPRMC sentence parsing finished

			nGPSReceived = 0;

		}

		gPrevCard = gActCard; 

	}

}

#ORG default