#include "GPS_67J50.h"
#include "math.h"
#include "HaltiLCD.c"
#include "MyMMCFat32.h"
#include "MyMMCFat32.c"

HANDLE hFile=0xff;
char gfilename[32];

#define LCD_BKLT   	PIN_G0
#define SENSOR_ENX	PIN_F7
#define BT_RST		PIN_G4
#define MEM_CS		PIN_F6
#define HMC_RESET	PIN_F5
#define POWER_OFF	PIN_F2
#define CHARGE_IND	PIN_A5
#define GPS_EN   	PIN_A4
#define SD_DETECT   	PIN_D4
#define SD_PWR   	PIN_D7
#define KEY_POWER   	PIN_B0
#define KEY_DOWN   	PIN_B1
#define KEY_CENTER   	PIN_B2
#define KEY_LEFT   	PIN_B3
#define KEY_RIGHT   	PIN_B4
#define KEY_UP      	PIN_B5
#define SD_CS      	PIN_C2

#define ADCCH_BATT   0   //ADC Channel: Battery voltage
#define ADCCH_HMCA   1   //ADC Channel: HMC Out A
#define ADCCH_HMCB   2   //ADC Channel: HMC Out B
#define ADCCH_TEMP   3   //ADC Channel: Temperature

double fVLDO = 2.9833;

float fStandardPressure = 101.96;   //Sea level pressure 101.325(kPa) 

#include "ADS11xx.c"

#define ADS1112_ADDRESS 0x9A
#define ADS1112_CONFIG   (ADS11XX_CONTINUOUS_CONVERSION | ADS11XX_16_BITS | ADS11XX_PGA_8)

int g_nKeyNewState;
#define KB_POWER   0x01
#define KB_DOWN   0x02
#define KB_CENTER   0x04
#define KB_LEFT   0x08
#define KB_RIGHT   0x10
#define KB_UP      0x20

int g_bBKLight = 1;
int16 nLEDTimer = 500;
int nSampleTimer = 100;
int16 g_nTimerTick=0;
int16 g_nPwrBtnHoldTick=0;

#include "gpsParser.c"

void realtimeProcess();

#INT_EXT
void EXT_isr()
{
   g_nKeyNewState = input_b();
   output_high(LCD_BKLT);
   nLEDTimer = 500;
   clear_interrupt(INT_EXT);
}

#INT_EXT1
void EXT1_isr()
{
   g_nKeyNewState = input_b();
   output_high(LCD_BKLT);
   nLEDTimer = 500;
   clear_interrupt(INT_EXT1);
}

#INT_EXT2
void EXT2_isr()
{
   g_nKeyNewState = input_b();
   output_high(LCD_BKLT);
   nLEDTimer = 500;
   clear_interrupt(INT_EXT2);
}

#INT_EXT3
void EXT3_isr()
{
   g_nKeyNewState = input_b();
   output_high(LCD_BKLT);
   nLEDTimer = 500;
   clear_interrupt(INT_EXT3);
}

#int_RB
void RB_isr() 
{
   g_nKeyNewState = input_b();
   output_high(LCD_BKLT);
   nLEDTimer = 500;
   
  if(!(g_nKeyNewState&KB_POWER))
   {
      g_bBKLight = !g_bBKLight;
   }

}

const char sCmdBTLNM[]="AT+BTLNM=\"TOM\"\r\n";  //Set BT name
const char sCmdBTSRV[]="AT+BTSRV=1,\"GPS\"\r\n";   //Turn on SPP protocol (RF and discoverable)
const char sCmdBTAUT[]="AT+BTAUT=1,1\r\n";   //Automatically connect (not return to command mode)


int16 nReceived2=0;
char sBTBuffer[64]= "\x0";
int nBTBufferLen=0;

#int_RDA2
void RDA2_isr()
{
   int ch;

   ch = fgetc(BT_232);
   
   //putc(ch);
   
    nReceived2++;
    
 
      
   if(ch != '\r' && ch != '\n')
   {
      sBTBuffer[nBTBufferLen++] = ch;
   
      if(nBTBufferLen > 26)
         nBTBufferLen = 0;
      
      sBTBuffer[nBTBufferLen] = '*';
   }
   

}

//10ms interrupt
#int_TIMER0
void TIMER0_isr() 
{
      set_timer0(5560);   //10ms
//   set_timer3(Config.nTimerBase);    //1ms interrupt

   if(nLEDTimer)
      nLEDTimer--;

   if(nSampleTimer)
   {
      nSampleTimer--;
   }
   g_nTimerTick++;
   
   if(input(KEY_POWER) == 0)   //Hold Power key
         g_nPwrBtnHoldTick++;
   else
      g_nPwrBtnHoldTick = 0;

}

#BYTE CCP1CON=0xFBD
#BYTE UCFG=0xF6F
#BIT UPUEN=UCFG.4
#BIT FSEN=UCFG.2

#BYTE OSCTUNE=0xF9B
#BIT PLLEN=OSCTUNE.6

int16 nHMCA, nHMCB;

void senseBattery();
void ReadSensors()
{
   //Turn on sensors power
   output_low(SENSOR_ENX);
   delay_ms(10);
   
   SenseBattery();
   
   output_high(HMC_RESET); //Set HMC1052
   delay_ms(3);

   set_adc_channel(ADCCH_HMCA);
   delay_ms(10);
   nHMCA = read_adc();
   nHMCA >>= 6;

   set_adc_channel(ADCCH_HMCB);
   delay_ms(1);
   nHMCB = read_adc();
   nHMCB >>= 6;
      
   output_low(HMC_RESET);  //Reset HMC1052
   
   //Turn off sensor power
   //output_float(SENSOR_ENX);
}


#define CardInserted PIN_D4
int gActCard, gPrevCard;



#include "at26_dataflash.c"
#include "scp1000.c"

int nSCPStatus;
int16 nSCPTemperature, nSCPPressure;
double fSCPTemperature, fSCPPressure;
double fSCPAltitude;
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

void displayGPSSNRBar(int x, int y, int nSat)
{
   int nSNR, nOrgSNR;
   int16 nSNRColor;
   nSNRColor = 0xF800;   //Blue
   
   nOrgSNR = recGPSData.satSNR[nSat];
   
   if(nOrgSNR < 0x80)
         nSNRColor = 0xC618;
         
         
   nOrgSNR = nOrgSNR&0x7F;
   nSNR = nOrgSNR;
   
   if(nSNR > 49)
      nSNR = 50;
   nSNR/=2;
   
  
   LCD_Gotoxy(x+6,y-14);
   printf(LCD_PutChar, "%02d", nSat+1);
   
    gps_FloodFill(x, y-32- 25, x+23, y-16-nSNR-1, bkgColor);
  
    gps_FloodFill(x, y-16-nSNR-1, x+23, y-16, nSNRColor);

    LCD_Gotoxy(x+6,y-32-nSNR);
   printf(LCD_PutChar, "%02d", nOrgSNR);
  
}

void displayGPSSNR()
{
   int i, nTotalValidSat;
   int x, y;
   nTotalValidSat = 0;
   
   //Display GPS time
   LCD_Gotoxy(0,0);
   printf(LCD_PutChar, "%02d:%02d:%02d", recGPSData.time.tm_hour,recGPSData.time.tm_min, recGPSData.time.tm_sec);


   printf(LCD_PutChar, " GPS%02d", recGPSData.numSat);

   //Display battery level
    LCD_Gotoxy(208-32, 0);
    printf(LCD_PutChar, "%3.1fV", fBatteryVoltage);
    
    for(i=0; i<32; i++)
    {
       if(recGPSData.satSNR[i] != 0xFF)
       {
          x = (nTotalValidSat%6)*30 + 18;
          y= nTotalValidSat>5?174:94;

          displayGPSSNRBar(x, y, i);
          
          if(++nTotalValidSat > 12)   //Show maximum 12 satellites
             break;        
       }
       
      //realtimeProcess();
      
    }
   
    for(i=nTotalValidsat; i<12; i++)
    {
        x = (i%6)*30 + 18;
        y= i>5?174:94;
            gps_FloodFill(x, y-32-25, x+23, y, bkgColor);

    }
  
    
}

void realtimeProcess()
{
//     while(g_nGPSSentences)   //New sentence received, 
//    {
//         GPSParser();
//         g_nGPSSentences--;
//      }
}

void main()
{
   int32 i=0;
   int16 nTemp;
   int16 nPressure;
   float fTemp;
   int16 nTick=0;
   char bSDReady=0;
   int error;
   char lcdId[4];
   
   

   //delay_ms(2000);
   //output_high(LCD_BKLT);
   //while(input(KEY_POWER) == 0);
   
   output_high(POWER_OFF);

   PLLEN=1; //Enable 48MHz PLL
   RBPU = 0;
   
   setup_adc_ports(sAN0|sAN1|sAN2|sAN3|VSS_VDD);
   setup_adc(ADC_CLOCK_DIV_32);
   setup_psp(PSP_DISABLED);
   set_tris_c(0b10010011); //c7=rx I, c6=tx O, c5 SDO O,c4 SDI I
   SPEN1=1;
   SPEN2=1;
   SETUP_SPI(SPI_MASTER | SPI_CLK_DIV_4 | SPI_H_TO_L |SPI_XMIT_L_TO_H );

   setup_wdt(WDT_OFF);
   setup_timer_0(RTCC_INTERNAL);
   setup_timer_2(T2_DIV_BY_1,255,1);
   //setup_timer_2(T2_DISABLED,0,1);
   setup_timer_3(T3_INTERNAL|T3_DIV_BY_2);
   setup_timer_4(T4_DISABLED,0,1);
   setup_comparator(NC_NC_NC_NC);
   setup_vref(FALSE);

   set_timer0(5560);    //10ms interrupt
   //set_timer3(59543);    //1ms interrupt

   enable_interrupts(INT_RDA);
   enable_interrupts(INT_RDA2);
   enable_interrupts(INT_EXT);
   enable_interrupts(INT_EXT1);
   enable_interrupts(INT_EXT2);
   enable_interrupts(INT_EXT3);
   enable_interrupts(INT_RB);
   enable_interrupts(INT_TIMER0);
   enable_interrupts(GLOBAL);

   output_high(PIN_F3); //Debug TX
   
   output_low(SD_PWR);  //Turn on SD card power
   gPrevCard = 1;
   

   //Reset Bluetooth BGB203
   output_low(BT_RST);
   delay_ms(2);
   output_float(BT_RST);
   delay_ms(50);
   
   //Initialize LCD
   LCD_Init();
   LCD_ClearScreen();
   

 
    
    delay_ms(100);
/*    SCP1000_WriteRegister8(SCPREG_RSTR, 0x01);   //Reset software
   delay_ms(100);
   SCP1000_WriteRegister8(0x03, 0x0F); //Self test
   delay_ms(100);
   if((SCP1000_ReadRegister8(SCPREG_DATARD8) == 0x01))
   {
      LCD_gotoxy(0, 10*14);
      printf(LCD_PutChar, "SCP: SelfTest failed.");
      delay_ms(1000);
   }
   else
   {
      LCD_gotoxy(0, 10*14);
      printf(LCD_PutChar, "SCP: SelfTest passed .");
   }
   SCP1000_WriteRegister8(0x03, 0x07);
*/
   

   //SCP1000_WriteRegister8(0x03, 0x0A);   //09: High speed mode, 0A: High resolution mode
   SCP1000_WriteRegister8(SCPREG_OPERATION, SCPOP_TRIG);  
   
   nLEDTimer = 1500;
   
   ReadSensors();

   //Turn on GPS 
   output_high(GPS_EN);
   
   while(1)
   {
   /*  
         if(g_nPwrBtnHoldTick > 300)   //Hold Power key for more than 3 seconds
         {
            output_low(GPS_EN);   //Turn off GPS
            output_high(SD_PWR);   //Turn off SD Card
          output_low(BT_RST);      //Turn off BT
           LCD_Shutdown();
            delay_ms(200);
            output_low(LCD_BKLT);
            output_low(POWER_OFF);
         }
*/
      //if(nLEDTimer == 0)
       //  output_low(LCD_BKLT);

      //realtimeProcess();
          
        if(g_bGPSDataReady)   //All sentence parsed, we can display or save GPS data now
        {
           //LCD_ClearScreen();
           displayGPSSNR();
           g_bGPSDataReady = 0;
          
        }
        
        if(nSampleTimer)
         continue;
      else
      {
         nSampleTimer = 100;
         ReadSensors();
      }
      
    displayGPSSNR();
	LCD_Gotoxy(0, 160);
    printf(LCD_PutChar, "%04ld", i++);
    
    //Enable LCD
   output_low(LCD_CSX);
   LCD_WriteCommand(LCDCMD_READ_DISPLAY_ID);
    delay_us(1);
    LCD_ReadData(lcdId, 4);
    LCD_Gotoxy(0,12);
    printf(LCD_PutChar, "ID%02X%02X%02X", lcdId[1], lcdId[2], lcdId[3]);
   output_high(LCD_CSX);
	}
}
/*
       
       if(input(CHARGE_IND) == 0)
         printf(LCD_PutChar, "-->>");
       else
         printf(LCD_PutChar, "    ");
       
         
       LCD_Gotoxy(0, 3*14);
       printf(LCD_PutChar, "HMC1052: X=%lu, Y=%lu ", nHMCA, nHMCB);
       
       LCD_Gotoxy(0, 4*14);
       printf(LCD_PutChar, "GPS:");
       printf(LCD_PutChar, "%lu bytes received", g_nGPSBufferWritePtr);
 
        LCD_Gotoxy(0, 5*14);
       printf(LCD_PutChar, "BGB203:");
       printf(LCD_PutChar, "%lu bytes received", nReceived2);
       LCD_Gotoxy(0, 6*14);
          if(nTick == 2)
            fprintf(BT_232, "+++\r\n");
         else if(nTick == 3)
            fprintf(BT_232, sCmdBTLNM);
         else if(nTick == 4)
            fprintf(BT_232, sCmdBTAUT);
         else if(nTick == 5)
           fprintf(BT_232, sCmdBTSRV);
         else if(nTick == 6)
            g_bGPStoBT = 1;
        
       if(nReceived2 > 0)
       {
         printf(LCD_PutChar, "%s", sBTBuffer);
       }
       
       LCD_Gotoxy(0, 7*14);
       printf(LCD_PutChar, "Key:");
       
       //g_nKeyNewState = input_b();
      LCD_Gotoxy(4*8, 7*14);
      printf(LCD_PutChar, "0x%02x", g_nKeyNewState);
      
      //SD test
      gActCard = input(CardInserted); 

      if(gActCard)   
      {
         if(gPrevCard == 0 && hFile != 0xFF)   //Card will be pulled out while a file is still open
         {
            fclose(hFile);
            hFile = 0xFF;
         }
         bSDReady = 0;
          LCD_Gotoxy(0, 8*14);
         printf(LCD_PutChar, "SD: No SD.     ");
     }


      if (gActCard == 0 && gPrevCard != 0)  // card was pulled out then pushed back now 
      {
         bSDReady = 0;
         error  = 1;

         //SaveConfig();

         set_tris_c(0b10010011); //c7=rx I, c6=tx O, c5 SDO O,c4 SDI I
         SETUP_SPI(SPI_MASTER | SPI_CLK_DIV_4 | SPI_H_TO_L |SPI_XMIT_L_TO_H );


         LCD_Gotoxy(0, 8*14);
         printf(LCD_PutChar, "SD: Inserted.");

         delay_ms(50); 
         for(i=0; i<10&&error; i++)
         {
           LCD_Gotoxy(0, 8*14);
           printf(LCD_PutChar, "SD: Init... ");
            error = init_mmc(10);
            delay_ms(50); 
         }  

         LCD_Gotoxy(0, 8*14);
         if(!error)
         {
            printf(LCD_PutChar, "SD: Init OK.");
         }
         else
             printf(LCD_PutChar, "SD: Init Failed.");

         if(InitFAT() != MMC_NO_ERR)
         {
             LCD_Gotoxy(0, 8*14);
            printf(LCD_PutChar, "SD: FAT faile: 0x%02X.", gFATErrCode);
         }


         if(!error)
         {   
            hFile = 0xFF;
            bSDReady = 1;
            LCD_Gotoxy(0, 8*14);
            printf(LCD_PutChar, "SD: "); 
            printf(LCD_PutChar, "%s ", sFATName[gFATType]);
            printf(LCD_PutChar, "%luMB", DiskInfo.TotSec16!=0?DiskInfo.TotSec16/2000:DiskInfo.TotSec32/2000);
        }               

      } //Card inserted
      
      gPrevCard = gActCard;
       
      LCD_Gotoxy(0, 9*14);
     //SETUP_SPI(SPI_MASTER | SPI_CLK_DIV_4 | SPI_H_TO_L |SPI_XMIT_L_TO_H );
     setup_spi(SPI_MASTER|SPI_H_TO_L|SPI_CLK_DIV_4|SPI_SAMPLE_AT_END);
     i = AT26_ReadDeviceID();
     //error = AT26_ReadSectorProtection(0x30000);
     AT26_WriteEnable();
     
     if(nTick == 1)
         AT26_UnprotectSector(0);
     if(nTick == 2)
     {
         AT26_WriteStatusRegister(0x00); //Global unprotect
         AT26_BlockErase4K(0);
     }
     if(nTick == 5)
         AT26_WriteByte(0, 'W');
     else if(nTick == 9)
         AT26_WriteByte(1, 'F');
         
      nTemp = AT26_ReadByte(0);
    
      error = AT26_ReadStatusRegister();
     printf(LCD_PutChar, "FLASH:0x%02X,0x%02X,%08lX", error, (int)nTemp, i); 
      
      //SCP1000
      LCD_Gotoxy(0, 1*14);
     nTemp = SCP1000_ReadID();
      nSCPStatus = SCP1000_ReadStatus();
      if(nSCPStatus & 0x20)   //data ready
      {
         nSCPTemperature = SCP1000_ReadTemperature();
          nPressure = SCP1000_ReadRegister8(SCPREG_DATARD8);
          nPressure &= 0x07;
        nSCPPressure = SCP1000_ReadPressure();
        fSCPTemperature = nSCPTemperature;
        fSCPTemperature = fSCPTemperature/20;
        fSCPPressure = nPressure;
        fSCPPressure *= 0x10000;
        fSCPPressure += nSCPPressure;
        fSCPPressure *= 0.25;
        fSCPPressure /= 1000;
        //fSCPAltitude = -(log(fSCPPressure/fStandardPressure)*(273.2+fSCPTemperature)*287.1/9.80665);
        fSCPAltitude = 44330*(1-pow((fSCPPressure/fStandardPressure),0.19));
        
         printf(LCD_PutChar, "SCP:%02X,%4.1fC, %8.4fkpa", nSCPStatus, fSCPTemperature, fSCPPressure);
         LCD_Gotoxy(0, 2*14);
         printf(LCD_PutChar, "SCP:%5.1fM ", fSCPAltitude);
      
       
         SCP1000_WriteRegister8(SCPREG_OPERATION, SCPOP_TRIG);  
      }
      else
      {
         printf(LCD_PutChar, "SCP:%02X          ", nSCPStatus);
      }

         LCD_Gotoxy(0, 160);
         printf(LCD_PutChar, "%04ld %d %ld %ld ", nTick++, nSampleTimer, get_timer3(), get_timer0());
    
//      delay_ms(1000);
   }
   Sleep();
}


*/
