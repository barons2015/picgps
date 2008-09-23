/*
	Program memory orgnization
	0x000000~0x0003FF		Reserved for interrupt vector only. (minimum erase block is 1024 byte)
	0x000400~0x004FFF	Reserved for MMC bootloader
	0x005000~		User application
*/

#include "gpsLOADER.h"

#define _BOOTLOADER
#include "..\share\bootloader.h"
#org HIGH_INT_VECTOR, INTERRUPT_REMAP_END 
void isr_relocate(void) 
{ 
#asm 
  // Address 0x0008 is the hardware High priority interrupt 
  TSTFSZ BootloaderActive       // if bootloader active 
  goto  LOADER_HIGH_INT         // then jump to bootloader ISR 
  GOTO  APPLICATION_HIGH_INT    // else jump to application ISR 
  NOP
  NOP
  NOP                           // Just filling memory 

  // Address 0x0018 is the hardware Low priority interrupt 
  TSTFSZ BootloaderActive       // if bootloader active 
  goto   LOADER_NORMAL_INT      // then jump to bootloader ISR 
  GOTO   APPLICATION_NORMAL_INT // else jump to application ISR 
#endasm 
}

#include "string.h"
#include "stdlib.h"
#include "2402.c"

#ORG default
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

#define CardInserted PIN_B0
int gActCard, gPrevCard;

#include "MyMMCFat32.h"
#include "MyMMCFat32.c"

HANDLE hFile=0xff;
char gfilename[32];

#define BUFFER_BIN_BLOCK	1024	//minimum 64
//#define BUFFER_HEX_LINE		64
//BYTE gBuffer[BUFFER_HEX_LINE];
BYTE gBinBlock[BUFFER_BIN_BLOCK];
int32 gBlockAddr;
int16 gBlockWritePtr;

#include "3310lcd.c"

unsigned int atoi_b16(char *s) {  // Convert two hex characters to a int8
   unsigned int result = 0;
   int i;

   for (i=0; i<2; i++,s++)  {
      if (*s >= 'A')
         result = 16*result + (*s) - 'A' + 10;
      else
         result = 16*result + (*s) - '0';
   }

   return(result);
}

#byte TBLPTRU=0xFF8
#byte TBLPTRH=0xFF7
#byte TBLPTRL=0xFF6
#byte TABLAT=0xFF5

#byte EECON1=0xFA6
#byte EECON2=0xFA7
#byte INTCON=0xFF2

#bit WR=EECON1.1
#bit WREN=EECON1.2
#bit FREE=EECON1.4
#bit CFGS=EECON1.5
#bit EEPGD=EECON1.6

void WriteFlashBlock(int32 addr, BYTE * pBuff, long nLen)
{
	int i,j;

	addr = gBlockAddr;

	//Erase flash first
	TBLPTRU = (byte)(addr>>16);
	TBLPTRH = (byte)((addr>>8)&0xFF);
	TBLPTRL = (byte)(addr&0xFF);


	//EEPGD = 1;
	//CFGS = 0;
	WREN = 1;
	FREE = 1;
	
	disable_interrupts(GLOBAL); 
	
	EECON2 = 0x55;
	EECON2 = 0xAA;
	
	WR = 1;	//Start erase
	
	for(i=0; i<16; i++)
	{
		for(j=0; j<64; j++)
		{
			TABLAT = *pBuff++;
#asm
			TBLWT*+
#endasm		
		}

		TBLPTRU = (byte)(addr>>16);
		TBLPTRH = (byte)((addr>>8)&0xFF);
		TBLPTRL = (byte)(addr&0xFF);
		addr += 64;

		disable_interrupts(GLOBAL);
		WREN = 1;
		EECON2 = 0x55;
		EECON2 = 0xAA;
		
		WR = 1;	//Start program
		
		enable_interrupts(GLOBAL);
		WREN = 0;
	}
	
	
	enable_interrupts(GLOBAL);
	
}

void LoadProgram(HANDLE hFile)
{
	char nChar;
	int bMoreChar;
	int bDone = 0;
	int16 i;

	gBlockAddr = 0;
	gBlockWritePtr = 0;
	memset(gBinBlock, 0xFF, BUFFER_BIN_BLOCK);

	nokia_clean_ddram();
	nokia_gotoxy(0,0);
	printf(nokia_printchar, "SW Update");
	nokia_refresh();

	//Read BIN file indentifier
	bMoreChar = fgetch(&nChar, hFile);
	if(!bMoreChar || nChar != 'W')
	{
		nokia_gotoxy(0,1);
		printf(nokia_printchar, "Invalid File");
		nokia_refresh();
		while(1);
	}

	bMoreChar = fgetch(&nChar, hFile);
	if(!bMoreChar || nChar != 'F')
	{
		nokia_gotoxy(0,1);
		printf(nokia_printchar, "Invalid File");
		nokia_refresh();
		while(1);
	}

	while(!bDone)
	{
		gBlockAddr = 0xFFFFFFFF;
		fread(&gBlockAddr, 4, hFile);

		if(gBLockAddr == 0xFFFFFFFF)
			break;
		
		if(fread(gBinBLock, BUFFER_BIN_BLOCK, hFile) != BUFFER_BIN_BLOCK)
		{
			nokia_gotoxy(0,1);
			printf(nokia_printchar, "File corrupted");
			nokia_refresh();
			while(1);
		}

		//Decode program code
		for(i=0; i<BUFFER_BIN_BLOCK; i++)
			gBinBlock[i] ^= 'W';

		if(gBlockAddr > LOADER_END)	//There is something in Block
		{
				//Write block to flash
			
				nokia_gotoxy(0,3);
				printf(nokia_printchar, "Loading...");
				nokia_gotoxy(0,4);
				printf(nokia_printchar, "%lu%%", gFiles[hFile].wFileSize*100/gFiles[hFile].DirEntry.wSize);
				nokia_refresh();

				WriteFlashBlock(gBlockAddr, gBinBlock, BUFFER_BIN_BLOCK);
		}
	}
	
}

#ORG APPLICATION_START, APPLICATION_START + 0xff
void UserProgram()
{
	nokia_clean_ddram();
	nokia_gotoxy(3, 6);
	printf(nokia_printchar, "BOOT LOADER");
	nokia_refresh();
	delay_ms(2000);
	while(1);
}

#ORG default
void main()
{
	long i;
	int error;
	char bSDReady;

	BootloaderActive = 1;
	
	memset(gFiles, 0, sizeof(FILE));
	gFiles[0].wFileSize = 0;
	gFilename[0] = 0;

#ifdef 4XCLOCK
	OSCTUNE=0x40;
#endif

	bSDReady = 0;

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

	setup_timer_3(T3_DISABLED|T3_DIV_BY_1);
	//setup_timer_3(T3_INTERNAL|T3_DIV_BY_8);
	//set_timer3(15535);	//200ms

	setup_timer_4(T4_DISABLED,0,1);
	setup_comparator(NC_NC_NC_NC);
	setup_vref(FALSE);
//	enable_interrupts(INT_RDA);
	RBPU = 0;
//	enable_interrupts(INT_RB);
//   	enable_interrupts(INT_TIMER3);
//	enable_interrupts(GLOBAL);
	setup_oscillator(False);


	setup_ccp2(CCP_PWM);
	setup_ccp3(CCP_PWM);
//	setup_ccp4(CCP_PWM);

	setup_comparator(NC_NC_NC_NC);
	setup_vref(FALSE);


	output_high(PIN_E7);
	output_high(PIN_G0);
	output_high(PIN_G3);

	set_pwm2_duty(100);
	set_pwm3_duty(80);
//	set_pwm4_duty(80);

	//Init Nokia 3310 LCD
	nokia_init();

	nokia_clean_ddram();
	nokia_refresh();

	i=0;
	gPrevCard = 1;

	hFile = 0xff;
	set_pwm2_duty(500);


	gActCard = input(CardInserted); 

	if (gActCard == 0 )  // card inserted
	{
		bSDReady = 0;
		error  = 1;

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

		if(InitFAT() != MMC_NO_ERR)
		{
			TRACE1("\r\n Failed to initFat, error code = 0x%02X.", gFATErrCode);
			error = 1;
		}


		if(!error)
		{	
			hFile = 0xFF;
			bSDReady = 1;
		}				

	}


	if(bSDReady)
	{
		sprintf(gfilename, "GPSLOGSW.BIN");
		hFile = fopen(gfilename, 'r');
		if(hFile == 0)
		{
			nokia_clean_ddram();
			nokia_gotoxy(0, 2);
			printf(nokia_printchar, "Found GPSLOGSW.BIN");
			nokia_refresh();
			delay_ms(2000);

			LoadProgram(hFile);

			fclose(hFile);
			remove(gfilename);

			nokia_clean_ddram();
			nokia_gotoxy(0,0);
			printf(nokia_printchar, "SW update completed.");
			nokia_gotoxy(0,2);
			printf(nokia_printchar, "Please restart device.");
			nokia_refresh();
			delay_ms(5000);
			while(1);
		}

	}

	//Call user application
	//UserProgram();
	BootloaderActive = 0;
	goto_address(APPLICATION_START);
}
