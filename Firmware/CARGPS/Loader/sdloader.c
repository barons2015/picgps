/*
	Program memory orgnization
	0x000000~0x004BFF	Reserved for MMC bootloader
	0x004C00~0x004FFF	Reserved for user settings (1K block)
	0x005000~		User application
	0x005000 ~ 0x007FFF	Mass Storage Device
	0x008000 ~ 
*/

#include "gpsLOADER.h"

#BYTE OSCTUNE=0xF9B
#BIT PLLEN=OSCTUNE.6

#define _BOOTLOADER
#include "..\shared\bootloader.h"
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


#include "..\shared\MyMMCFat32.h"
#include "..\shared\MyMMCFat32.c"

HANDLE hFile=0xff;
char gfilename[32];

#define BUFFER_BIN_BLOCK	1024	//minimum 64
//#define BUFFER_HEX_LINE		64
//BYTE gBuffer[BUFFER_HEX_LINE];
BYTE gBinBlock[BUFFER_BIN_BLOCK];
int32 gBlockAddr;
int16 gBlockWritePtr;

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
		
		WR = 1;	
		
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
	int bLEDOn=0;

	gBlockAddr = 0;
	gBlockWritePtr = 0;
	memset(gBinBlock, 0xFF, BUFFER_BIN_BLOCK);

	//Read BIN file indentifier
	bMoreChar = fgetch(&nChar, hFile);
	if(!bMoreChar || nChar != 'W')	//Invalide file
	{
		//while(1);
		return;
	}

	bMoreChar = fgetch(&nChar, hFile);
	if(!bMoreChar || nChar != 'F')	//invalide file
	{
		//while(1);
		return;
	}

	while(!bDone)
	{
		if(bLEDOn)
			LED_GREEN_ON;
		else
			LED_GREEN_OFF;
			
		gBlockAddr = 0xFFFFFFFF;
		fread(&gBlockAddr, 4, hFile);

		if(gBLockAddr == 0xFFFFFFFF)
			break;
		
		if(fread(gBinBLock, BUFFER_BIN_BLOCK, hFile) != BUFFER_BIN_BLOCK)
		{
			//File corrupted
			//while(1);
			return;
		}

		//Decode program code
		for(i=0; i<BUFFER_BIN_BLOCK; i++)
			gBinBlock[i] ^= 'W';

		if(gBlockAddr > LOADER_END)	//Do not overwrite boot loader memory block
		{
				//Write block to flash
				WriteFlashBlock(gBlockAddr, gBinBlock, BUFFER_BIN_BLOCK);
		}
	}
	
}



#ORG default
void main()
{
	long i;
	int error;
	char bSDReady;
	
	PLLEN=1; //Enable 48MHz PLL

	BootloaderActive = 1;
	
	memset(gFiles, 0, sizeof(FILE));
	gFiles[0].wFileSize = 0;
	gFilename[0] = 0;

	bSDReady = 0;

	i=0;

	hFile = 0xff;
	bSDReady = 0;
	error = 0;

	if (!NO_MMC_CARD)  // card inserted
	{
		bSDReady = 0;
		error  = 1;

		//setup SPI port for SD interface
		set_tris_c(0b11010011); //c7=rx I, c6=tx O, c5 SDO O,c4 SDI I
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

		if(bSDReady)
		{
			sprintf(gfilename, "GPSDRVSW.BIN");
			hFile = fopen(gfilename, 'r');
			if(hFile == 0)
			{
				LoadProgram(hFile);
				fclose(hFile);
				remove(gfilename);
			}
		}	

		set_tris_c(0xFF); //Set to default
		setup_spi(FALSE);
	}

	//Jump to user application
	BootloaderActive = 0;
	goto_address(APPLICATION_START);
}
