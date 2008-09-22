#include "GPS_CAR.h"

#include "math.h"
#include "string.h"
#include "stdlib.h"

#define BUZZER   	PIN_G0
#define GPS_EN   	PIN_D7
#define SD_DETECT   PIN_F6
#define SD_CS      	PIN_C2
#define LED_USB		PIN_B3
#define LED_RED		PIN_B4
#define LED_GRN		PIN_B5
#define LED_SP1		PIN_B2
#define LED_SP2		PIN_B1
#define LED_GPS		LED_GRN
#define CardInserted SD_DETECT
#define NO_MMC_CARD	input(CardInserted)

#define ALARM_ON	output_high(BUZZER)
#define ALARM_OFF	output_low(BUZZER)
#define LED_RED_ON	output_high(LED_RED)
#define LED_RED_OFF	output_low(LED_RED)
#define LED_GREEN_ON	output_high(LED_GRN)
#define LED_GREEN_OFF	output_low(LED_GRN)
#define LED_USB_ON		output_high(LED_USB)
#define LED_USB_OFF		output_low(LED_USB)

#define ADCCH_BATT   0   //ADC Channel: Battery voltage

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

typedef struct 
{
	char nFlag;
	long nFileSeqNo;
	signed int nTimeZone;
	char sDevID[5];	//Local Device ID

	long reserved1;
	long reserved2;
	long reserved3;
	long reserved4;

}LOGGER_SETTINGS;
LOGGER_SETTINGS gSettings;

#include "..\shared\MyMMCFat32.h"
#include "..\shared\MyMMCFat32.c"

HANDLE hFile=0xff;
char gfilename[32];



double fVLDO = 2.9833;

int g_bForwardGPS = 1;

int nSampleTimer = 100;
int16 g_nTimerTick=0;
int16 g_nGPSSignalTick=0;
int16 g_nLeaderSignalTick = 0;
int	g_nBuzzerTick=0;
int g_nLEDUSBTick=0;
int g_nDrvModuleTick=0;
int g_nLedRedTick=0;

int g_bLeaderPresents = FALSE;

void realtimeProcess();

int16 nReceived2=0;
char sDRVBuffer[64]= "\x0";
int nDRVBufferLen=0;
char sDRVModule[6] = "0000";	//4 digit driver module ID
int g_bDrvModuleActive = 0;

//#int_RDA is in gpsParser.c
#include "..\shared\gpsParser.c"

#int_RDA2
void RDA2_isr()
{
	int ch;

	ch = fgetc(MDLDRV);

	nReceived2++;

	g_nLEDUSBTick = 10;


	if(ch != '\r' && ch != '\n')
	{
		sDRVBuffer[nDRVBufferLen++] = ch;

		if(nDRVBufferLen > 26)
			nDRVBufferLen = 0;
	}
	else
	{
		//Driver module sends 'DRV1234\r\n' to car module
		if(nDrvBufferLen > 6 && sDRVBuffer[0] == 'D' && sDRVBuffer[1] == 'R' && sDRVBuffer[2] == 'V')
		{
			g_bDrvModuleActive = 1;
			LED_USB_ON;

			for(ch = 0; ch < 4; ch++)
				sDRVModule[ch] = sDRVBuffer[3+ch];
			sDRVModule[ch] = 0;

		} 

		nDRVBufferLen = 0;//Reset buffer write pointer
	} 

}


//10ms interrupt
#int_TIMER0
void TIMER0_isr() 
{
	set_timer0(5560);   //10ms

	if(nSampleTimer)
	{
		nSampleTimer--;
	}
	g_nTimerTick++;

	if(g_nGPSSignalTick)
		g_nGPSSignalTick--;

	if(g_nBuzzerTick)
		g_nBuzzerTick--;

	if(g_nDrvModuleTick)
		g_nDrvModuleTick--;
		
	if(g_nLedRedTick)
		g_nLedRedTick--;
		
	if(g_nLeaderSignalTick)
		g_nLeaderSignalTick--;
}

#BYTE CCP1CON=0xFBD
#BYTE UCFG=0xF6F
#BIT UPUEN=UCFG.4
#BIT FSEN=UCFG.2

#BYTE OSCTUNE=0xF9B
#BIT PLLEN=OSCTUNE.6

void senseBattery();
void ReadSensors()
{
	SenseBattery(); 
}


int gActCard, gPrevCard;

double fBatteryVoltage;

void senseBattery()
{
	long nTemp;

	//Read battery voltage
	set_adc_channel(ADCCH_BATT);
	nTemp = 0;
	delay_us(10);
	nTemp = read_adc();
	fBatteryVoltage = nTemp;
	fBatteryVoltage = fBatteryVoltage*fVLDO*2/65535.0;
}



void realtimeProcess()
{

}

//Generate alarm 
//speed < 55km/h no alarm
//55km/h < speed < 75km/h, beep once
//75km/h < speed < 115km/h, beep twice, red led on
//115km/h < speed, keep beeping, read led brinking
enum
{
	eAlarmState_no_alarm = 0,
	eAlarmState_alarm60_trig,
	eAlarmState_alarm60_keep,
	eAlarmState_alarm80_trig,
	eAlarmState_alarm80_trig_end,
	eAlarmState_alarm80_trig2,
	eAlarmState_alarm80_trig2_end,
	eAlarmState_alarm80_keep,
	eAlarmState_alarm120_trig,
	eAlarmState_alarm120_trig_end,
	eAlarmState_alarm120_keep
};

//Speed Alarm state-machine
void speed_alarm_proc(long nSpeed)
{
	static int nAlarmState = eAlarmState_no_alarm;

	switch(nAlarmState)
	{
	case eAlarmState_no_alarm:
		{
			if(nSpeed > 55)
				nAlarmState = eAlarmState_alarm60_trig;
		}
		break;
	case eAlarmState_alarm60_trig:
		{
			g_nBuzzerTick = 50;
			ALARM_ON;
			nAlarmState = eAlarmState_alarm60_keep;
		}
		break;
	case eAlarmState_alarm60_keep:
		{
			LED_RED_OFF;
			if(nSpeed < 55)
				nAlarmState = eAlarmState_no_alarm;
			else if(nSpeed > 75)
				nAlarmState = eAlarmState_alarm80_trig;
			else
				nAlarmState = eAlarmState_alarm60_keep;
		}
		break;
	case eAlarmState_alarm80_trig:
		{
			g_nBuzzerTick = 100;
			g_nLedRedTick = 200;
			ALARM_ON;
			nAlarmState = eAlarmState_alarm80_trig_end;
		}
		break;
	case eAlarmState_alarm80_trig_end:
		{
			if(g_nBuzzerTick == 0)
				nAlarmState = eAlarmState_alarm80_trig2;
		}
		break;
	case eAlarmState_alarm80_trig2:
		{
			g_nBuzzerTick = 100;
			g_nLedRedTick = 200;
			ALARM_ON;
			nAlarmState = eAlarmState_alarm80_trig2_end;
		}
		break;
	case eAlarmState_alarm80_trig2_end:
		{
			if(g_nBuzzerTick == 0)
				nAlarmState = eAlarmState_alarm80_keep;
		}
		break;
	case eAlarmState_alarm80_keep:
		{
			g_nLedRedTick = 200;
			if(nSpeed < 75)
				nAlarmState = eAlarmState_alarm60_keep;
			else if(nSpeed > 115)
				nAlarmState = eAlarmState_alarm120_trig;
		}
		break;
	case eAlarmState_alarm120_trig:
		{
			g_nBuzzerTick = 50;
			g_nLedRedTick = 50;
			ALARM_ON;
			nAlarmState = eAlarmState_alarm120_trig_end;
		}
		break;
	case eAlarmState_alarm120_trig_end:
		{
			if(g_nBuzzerTick == 0)
				nAlarmState = eAlarmState_alarm120_keep;
		}
		break;
	case eAlarmState_alarm120_keep:
		{
			if(nSpeed < 115)
			{
				ALARM_OFF;
				nAlarmState = eAlarmState_alarm80_keep;
			}	
			else
			{
				nAlarmState = eAlarmState_alarm120_trig;
			}	
		}
		break;
	default:
		nAlarmState = eAlarmState_no_alarm;
		break;
	}	
}

#define CONFIG_ADDRESS	0xFC00
void LoadConfig()
{
	int nSize;
	nSize = sizeof(LOGGER_SETTINGS);

	read_program_memory(CONFIG_ADDRESS, &gSettings, nSize);

}

void SaveConfig()
{
	int nSize;
	nSize = sizeof(LOGGER_SETTINGS);

	write_program_memory(CONFIG_ADDRESS, &gSettings, nSize);
}

//clear any USART error
void clearUSARTError()
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

#define NRF_CSN		PIN_F5	//NRF chip select
#define NRF_CE		PIN_F2
#define NRF_SCK		PIN_C0
#define NRF_DI		PIN_A4
#define NRF_DO		PIN_C1
#define NRF_IRQ		PIN_A5

#include "../shared/nrf24l01.c"


void main()
{
	#define NRF_PACKET_SIZE 7
	char gNRFPacket[NRF_PACKET_SIZE+1];
	int nCount=0;
	int32 i=0;
	char bSDReady=0;
	int error;
	int bLogToFile = 1;
	int nStillSecCount = 0;
	int nLogDate = 0;
	char sLogDRVModule[6]= "";

	PLLEN=1; //Enable 48MHz PLL

	//Reset GPS data

	//Reset output Pin
	output_low(GPS_EN);
	output_low(LED_RED);
	output_low(LED_GRN);
	output_low(LED_USB);
	
	//Reset gps data
	memset(&recGPSData, 0, sizeof(GPSDATA));
	recGPSData.northSouth = 'N';
	recGPSData.eastWest = 'E';
	
	//load settings
	gSettings.nFileSeqNo = 0;
	LoadConfig();
	strcpy(gSettings.sDevID, "9999");	//remove this line when ID post change ready
	if(gSettings.nFlag != 0x55)
	{
		gSettings.nFlag = 0x55;
		gSettings.nFileSeqNo = 0;
		gSettings.nTimeZone = 8;

		gSettings.reserved1 = 0;
		gSettings.reserved2 = 0;
		gSettings.reserved3 = 0;
		gSettings.reserved4 = 0;
		SaveConfig();
	}
	
	//Startup notification
	output_high(BUZZER);
	output_high(LED_RED);
	delay_ms(200);
	delay_ms(200);

	output_low(BUZZER);
	output_low(LED_RED);
	output_high(LED_GRN);

	delay_ms(200);
	delay_ms(200);
	output_high(BUZZER);
	output_low(LED_GRN);
	output_high(LED_USB);

	delay_ms(200);
	delay_ms(200);
	output_low(BUZZER);
	output_low(LED_USB);


	setup_adc_ports(sAN0|VSS_VDD);
	setup_adc(ADC_CLOCK_DIV_32);
	setup_psp(PSP_DISABLED);
	set_tris_c(0b10010000); //c7=rx I, c6=tx O, c5 SDO O,c4 SDI I
	SETUP_SPI(SPI_MASTER | SPI_CLK_DIV_4 | SPI_H_TO_L |SPI_XMIT_L_TO_H );

	//SPI2 for Car Driver module
	//SETUP_SPI2(SPI_MASTER | SPI_CLK_DIV_4 | SPI_H_TO_L |SPI_XMIT_L_TO_H );

	setup_wdt(WDT_OFF);
	setup_timer_0(RTCC_INTERNAL);
	setup_timer_2(T2_DIV_BY_1,255,1);
	//setup_timer_2(T2_DISABLED,0,1);
	setup_timer_3(T3_INTERNAL|T3_DIV_BY_2);
	setup_timer_4(T4_DISABLED,0,1);
	setup_comparator(NC_NC_NC_NC);
	setup_vref(FALSE);

	set_timer0(5560);    //10ms interrupt
	
	
	SPEN1=1;
	SPEN2=1;
	enable_interrupts(INT_RDA);
	enable_interrupts(INT_RDA2);

	enable_interrupts(INT_TIMER0);
	enable_interrupts(GLOBAL);


	gPrevCard = 1;

	delay_ms(100);


	ReadSensors();

	//Turn on GPS 
	output_high(GPS_EN);

	g_nGPSSignalTick = 100;

	g_nDrvModuleTick = 50;	//Check Drver module every 500ms

	TRACE0("GPSCAR started.\r\n");
	
	NRF_config_rx();
	
	NRF_DumpRegister();
	
	while(1)
	{
		
		//clear any USART error
		clearUSARTError();	

		//Check if we received command from Leader module
		if(input(NRF_IRQ) == 0 ) //IRQ active low
		{
			//Yes, Leader is around here
			if(g_bLeaderPresents == FALSE)
			{
				//Notify leader
				LED_RED_ON;
				g_nLedRedTick = 50;
				
				ALARM_ON;
				g_nBuzzerTick = 20;
			}
			
			LED_GREEN_ON;
			g_nGPSSignalTick = 100;
				
			g_bLeaderPresents = TRUE;
			g_bForwardGPS = FALSE;	//stop forwarding GPS signal to Car Module
			
			//Next leader command shall arrive in 30 sec
			g_nLeaderSignalTick = 3000;	
			
			//Receive data and clear rx state for next command
			NRF_reset_rx();
			
			//display tracing msg
			TRACE2("\r\n%03u Recv: %s", i++, gNRFPacket);
			//delay_ms(10);
		}	
			
		if(g_nLeaderSignalTick == 0 && g_bLeaderPresents)
		{
			g_bLeaderPresents = FALSE;
			g_bForwardGPS = TRUE;	
			TRACE0("\r\nLeader's gone.");
				
			//Notify leader
			LED_GREEN_ON;
			LED_RED_ON;
			g_nGPSSignalTick = 500;
			g_nLedRedTick = 300;		
		}	
		

		//GPS data has been received. Flag set in INT_RDA()
		if(bGPSDataIn && bGPRMC)	
		{
			g_nGPSSignalTick = 10;	//100ms on
			bGPSDataIn = 0;	//clear flag
		}	
	
		if(g_nGPSSignalTick || g_bLeaderPresents)
		{
			LED_GREEN_ON;
		}
		else
		{
			LED_GREEN_OFF;
		}
			
		//Turn off buzzer
		if(g_nBuzzerTick == 0)
		{
			output_low(BUZZER);
		}	
		
		
		if(g_nLedRedTick == 0)
			LED_RED_OFF;
		else
			LED_RED_ON;
			
		//check Driver module
		if(g_nDrvModuleTick == 0)
		{
			if(!g_bDrvModuleActive)	//Driver module is not there
			{
				//Stop forwarding GPS sentence to Driver module
				g_bForwardGPS = 0;	

				//Send customized Car Tracking System sentence $GPCTS, every 0.5s
				fprintf(MDLDRV, "$GPCTS,%s,E*00\r\n", gSettings.sDevID);
				g_nDrvModuleTick = 50;	//50*10ms = 500ms

				//Car is moving but without driver module, beep to notify driver
				if(recGPSData.nSpeed > 10)	//speed > 10kmh
				{
					g_nBuzzerTick = 10;	//Beep 0.1ms
				}
			}
			else
			{
				//Enabled forwarding GPS sentence to Driver module
				g_bForwardGPS = 1;	

				//Driver module is currently active. Schedule next check point in 2s 
				//Driver module shall send active sentence 'DRV' at least every 1s
				g_nDrvModuleTick = 200;		//200*10ms = 2s

				//Reset driver module flag to false. RDA2_isr will set it if receive sentence from driver module.
				g_bDrvModuleActive = 0;
				LED_USB_OFF;
			}
		}

		if(bGPRMC)	//Process GPRMC sentence
		{
			parseGPRMC(GPRMCBuffer, nGPRMCLen);
			bGPRMC = 0;	//current GPRMC sentence parsing finished
 	 		speed_alarm_proc(recGPSData.nSpeed);
 	 		
 	 		//If Leader is around
			if(g_bLeaderPresents)
			{
				//Send customized Car Tracking System sentence $GPCTS, every 0.5s
				fprintf(MDLDRV, "$GPCTS,%s,E*00\r\n", gSettings.sDevID);		
			}
		}
			
 		if(g_bLeaderPresents)
 		{
	 		//Skip file logging
	 		continue;
	 	}	
    
		//SD card 
		gActCard = input(CardInserted); 

		if(gActCard)	
		{
			if(gPrevCard == 0 && hFile != 0xFF)	//Card will be pulled out while a file is still open
			{
				g_nLedRedTick = 100;
				LED_RED_ON;
				fclose(hFile);
				hFile = 0xFF;
			}
			bSDReady = 0;
		}

		if (gActCard == 0 && gPrevCard != 0)  // card was pulled out then pushed back now 
		{
			bSDReady = 0;
			error  = 1;

			set_tris_c(0b10010000); //c7=rx I, c6=tx O, c5 SDO O,c4 SDI I
			SETUP_SPI(SPI_MASTER | SPI_CLK_DIV_4 | SPI_H_TO_L |SPI_XMIT_L_TO_H );

			TRACE0("\r\nSD card Inserted.");

			delay_ms(50); 
			for(i=0; i<10&&error; i++)
			{
				TRACE1("\r\nInit SD (%ld)...", i); 
				LED_RED_ON;
				error = init_mmc(10);
				LED_RED_OFF;
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


			hFile = 0xFF;
			if(!error)
			{	
				bSDReady = 1;
			}	
			else
			{
				bSDReady = 0;
				g_nLedRedTick = 500;
				LED_RED_ON;
			}					

		} //Card inserted

		if(bGPCTS)	//Get CTS command 
		{
			parseGPCTS(GPCTSBuffer);
			bGPCTS = 0;
		}

		{

#define LOGTEST
#ifndef LOGTEST
			if(recGPSData.bFixed)	//Only log sentence when position is fixed
#endif
			{	
				//Write log if SD card is ready and loggin is enabled
				if(bSDReady && bLogToFile)
				{
					//Create one log file for each day
					if(nLogDate != myrec.tm_day)
					{
						if(hFile == 0)	//Close current file
						{
							fclose(hFile);
							hFile = 0xFF;
						}	
						nLogDate = myrec.tm_day;
					}	
					//Create new log if file is not openned yet
					if(hFile!= 0)
					{
						sprintf(gfilename, "%s%02d%02d.LOG",
							gSettings.sDevID, 
							myrec.tm_mon,
							myrec.tm_day);
							
						hFile = fopen(gfilename, 'a');
						if(hFile == 0xFF)
						{
							g_nLedRedTick = 80;
						}	
					}

					if(hFile == 0)	//File opened
					{
						if(strcmp(sLogDRVModule, sDRVModule) != 0)
						{
							printf(fputchar, "$GPCTS,%s,%s,E*00\r\n",gSettings.sDevID, sDRVModule);
							strcpy(sLogDRVModule,sDRVModule);
						}
						//Log GPS sentence every 1 minute if speed is 0kmh
						if(recGPSData.nSpeed < 1)
						{
							nStillSecCount++;
							nStillSecCount %= 60;
							
							if(nStillSecCount == 59)
							{
								fclose(hFile);
								hFile = 0xFF;
							}	
						}
						else
						{
							nStillSecCount = 1;
						}

#ifndef LOGTEST	
						if(nStillSecCount == 1)
#endif
						{
							//Write GPS sentence to log file
							g_nGPSSignalTick = 60;
							fputstring(GPRMCBuffer, hFile);
						}
					}
				}//end of if(bSDReady && bLogToFile)

			}	//end of if(bGPSFixed)
		}
		gPrevCard = gActCard; 		
		
	}
}		



