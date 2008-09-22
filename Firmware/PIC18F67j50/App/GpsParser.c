//#define _DEBUG
#include "string.h"
#include "stdlib.h"

#define GPS_BUFFER_LEN   512

char GPSBuffer[GPS_BUFFER_LEN];
int16 g_nGPSBufferWritePtr=0;   //Write pointer
int16 g_nGPSBufferReadPtr=0;   //Read pointer
int16 g_nGPSBufferEOLPtr=0;      //End of line pointer
int g_nGPSSentences=0;      //Number of sentences in buffer
int g_nGPSOverlapped = 0;

#define GPS_INC_READPTR(n)   (g_nGPSBufferReadPtr = (g_nGPSBufferReadPtr + (n))%GPS_BUFFER_LEN)
#define GPS_INC_WRITEPTR(n)   (g_nGPSBufferWritePtr = (g_nGPSBufferWritePtr + (n))%GPS_BUFFER_LEN)

#define LOG_FLAG_DISCARD_ZERO_KHM   0x01

/*
typedef struct
{
   TimeRecord tStartTime;
   TimeRecord tEndTime;
   int32   nDistance;
   float fMaxSpeed;
   float fMaxAltitude;
   float fMinAltitude;
}LOG_SESSION;
LOG_SESSION gLogSession;
*/
int nStillSecCount=0;   //Start counting when fSpeed=0

typedef struct
{
   int nSatID;
   int nSatSNR;
}SATINFO;

SATINFO satData;

typedef struct
{
   int bFixed;
   TimeRecord time;
   int32 nLatitude;
   float fLatitude;
   char northSouth;
   int32 nLongitude;
   float fLongitude;
   char eastWest;
   int32 nSpeed;
   float fSpeed;
   long direction;
   int numSat;   //Number of satellite
   signed int32 nAltitude;
   float fAltitude;
   int fixType;    //1:Not fixed, 2: 2D, 3:3D
   int satSNR[32]; //Satellite signal level : 0xFF:Not in sky, 0x80~0xFE: Fixed, 0x00~0x7F: Not fixed
}GPSDATA;

float fAccDistance=0;

GPSDATA recGPSData;

char GPSFlag = 1;

#bit g_bGPStoBT=GPSFlag.0   //Send GPS sentence to Bluetooth port
#bit g_bGPSNewSentence=GPSFlag.1
#bit g_bGPSFixed = GPSFlag.2
#bit g_bGPSDataReady = GPSFlag.3   //End of all one second sentences. normally end with $GPVTG
#bit g_bGPSOverlapped = GPSFlag.4

int16 g_nTickNewData = 0;

enum{
   GPS_STATE_IDLE = 0,
   GPS_STATE_NEWSENTENCE,
   GPS_STATE_GPGGA,
   GPS_STATE_GPRMC,
   GPS_STATE_GPGSA,
   GPS_STATE_GPGSV,
   GPS_STATE_GPVTG
};


int g_nGPSState = GPS_STATE_IDLE;
int g_nGPSReceived = 0;
int g_nGPSParamIndex = 0;
int g_bGPSNumSign = 0;

void gps_ResetSatelliteSignal();

#int_RDA HIGH
void GPS_isr()
{
   int ch;
   static char chKeyword[5];
   
   ch = fgetc(GPS);   //Get one char from GPS
   
//   if(g_bGPStoBT)
//      fputc(ch, BT_232);
   
   if(ch == 0x0D)
   {
      g_nGPSState = GPS_STATE_IDLE;
      return;
   }
   
   switch(g_nGPSState)
   {
      case GPS_STATE_IDLE:
         if(ch == '$')
         {
            g_nGPSState = GPS_STATE_NEWSENTENCE;
            g_nGPSReceived = 0;
         }
         break;
      case GPS_STATE_NEWSENTENCE:
         chKeyword[g_nGPSReceived++] = ch;
         if(g_nGPSReceived == 5)   //Received GPGGA/GPRMC/GPGSA/GPGSV/GPVTG
         {
            if(chKeyword[3] == 'G' && chKeyword[4] == 'A')   //GPGGA
               g_nGPSState = GPS_STATE_GPGGA;
            else if(chKeyword[3] == 'M' && chKeyword[4] == 'C')   //GPRMC
               g_nGPSState = GPS_STATE_GPRMC;
            else if(chKeyword[3] == 'S' && chKeyword[4] == 'A')   //GPGSA
               g_nGPSState = GPS_STATE_GPGSA;
            else if(chKeyword[3] == 'S' && chKeyword[4] == 'V')   //GPGSV
               g_nGPSState = GPS_STATE_GPGSV;
            else if(chKeyword[3] == 'T' && chKeyword[4] == 'G')   //GPVTG
               g_nGPSState = GPS_STATE_GPVTG;
            else
               g_nGPSState = GPS_STATE_IDLE;   //Unknown keyword
            
            //Initialize GPS sentence variables   
            g_nGPSReceived = 0;
            g_nGPSParamIndex = 0;
         }
         break;
      case GPS_STATE_GPGGA:
         if(ch == 0x0A || ch == '*')   //End of sentence
            g_nGPSState = GPS_STATE_IDLE;
         else if(ch == ',')   //Next param
         {
            g_nGPSParamIndex++;
            g_nGPSReceived = 0;
            g_bGPSNumSign = 0;
         }
        else
         {
            switch(g_nGPSParamIndex)   //Starts from 1
            {
               case 1:   //time
                  break;
               case 2:   //fLatitude
                  break;
               case 3:   //N/S
                  break;
               case 4:   //Longitude
                  break;
               case 5:   //W/E
                  break;
               case 6:   //1: Fixed, 0:Fix not avalable
                  //memset(&recGPSData, sizeof(GPSDATA), 0);
                  if(ch == '1')
                     recGPSData.bFixed = 1;
                  else
                     recGPSData.bFixed = 0;
                  g_bGPSFixed = recGPSData.bFixed;
                  break;
               case 7:   //Number of satellites
                     if(g_nGPSReceived == 0)
                     {
                        recGPSData.numSat = ch - '0';
                  }
                  else
                  {
                     recGPSData.numSat *= 10;
                     recGPSData.numSat += ch - '0';
                   }
                   
                   g_nGPSReceived++;
                  break;
               case 8:   
                  break;
               case 9:   //Altitude
                  if(ch == '-')
                        g_bGPSNumSign = 1;
                  else
                  {
                     //recGPSData.nAltitude = (recGPSData.nAltitude<<3) + (recGPSData.nAltitude<<1);      //*10
                     if(g_bGPSNumSign)
                        recGPSData.nAltitude -= ch - '0';
                     else
                        recGPSData.nAltitude += ch - '0';
                  }
                  break;
            } //switch(g_nGPSParamIndex)                                  
         }
        break;
      case GPS_STATE_GPRMC:
          if(ch == 0x0A || ch == '*')   //End of sentence
            g_nGPSState = GPS_STATE_IDLE;
         else if(ch == ',')   //Next param
         {
            g_nGPSParamIndex++;
            g_nGPSReceived = 0;
            g_bGPSNumSign = 0;
         }
         else
         {
            switch(g_nGPSParamIndex)   //Starts from 1
            {
               case 1:   //time
                    switch(g_nGPSReceived)
                    {
                          case 0:   //Hour
                             recGPSData.time.tm_hour = ch - '0';
                             recGPSData.time.tm_hour *= 10;
                             break;
                          case 1:
                             recGPSData.time.tm_hour += ch - '0';
                             break;
                          case 2:
                             recGPSData.time.tm_min =  ch - '0';
                             recGPSData.time.tm_min *= 10;
                             break;
                          case 3:
                             recGPSData.time.tm_min += ch - '0';
                             break;
                          case 4:
                             recGPSData.time.tm_sec =  ch - '0';
                              recGPSData.time.tm_sec *= 10;
                            break;
                          case 5:
                             recGPSData.time.tm_sec += ch - '0';
                             break;
                       }
                       
                       g_nGPSReceived++;
                             
                  break;
               case 2:   //A=data valid or V=data not valid
                     if(ch != 'A')
                        g_nGPSState = GPS_STATE_IDLE;
               recGPSData.nLatitude = 0;
                  recGPSData.nLongitude = 0;
                  recGPSData.nSpeed = 0;
                
                  break;
               case 3:   //Latitude
                     recGPSData.nLatitude = (recGPSData.nLatitude<<3) + (recGPSData.nLatitude<<1);    //*10
                     recGPSData.nLatitude += ch - '0';
                  break;
               case 4:   //N/S
                     recGPSData.northSouth = ch;
                     break;
               case 5:   //Longitude
                   recGPSData.nLongitude = (recGPSData.nLongitude<<3) + (recGPSData.nLongitude<<1);    //*10
                     recGPSData.nLongitude += ch - '0';
                 break;
               case 6:   //W/E
                     recGPSData.eastWest = ch;
                  break;
               case 7:   //Speed
                   recGPSData.nSpeed = (recGPSData.nSpeed<<3) + (recGPSData.nSpeed<<1);    //*10
                     recGPSData.nSpeed += ch - '0';
                     break;
               case 8:   //Direction
                     if(ch == '.')
                        g_nGPSReceived = 4;
                     else if(g_nGPSReceived < 3)
                     {
                       recGPSData.direction = (recGPSData.direction<<3) + (recGPSData.direction<<1);    //*10
                        recGPSData.direction += ch - '0';                   
                     }
                     g_nGPSReceived++;
                     break;
                  case 9:   //Date: DDMMYY
                     switch(g_nGPSReceived)
                     {
                        case 0:
                           recGPSData.time.tm_day = ch - '0';
                          recGPSData.time.tm_day *= 10;
                           break;
                       case 1:
                          recGPSData.time.tm_day += ch - '0';
                          break;
                        case 2:
                          recGPSData.time.tm_mon = ch - '0';
                          recGPSData.time.tm_mon *= 10;
                          break;
                        case 3:
                          recGPSData.time.tm_mon += ch - '0';
                          break;
                        case 4:
                          recGPSData.time.tm_year = ch - '0';
                          recGPSData.time.tm_year *= 10;
                          break;
                        case 5:
                          recGPSData.time.tm_year += ch - '0';
                          recGPSData.time.tm_year += 2000;
                          break;
                    }
                     break;
            } //switch(g_nGPSParamIndex)                                  
         }
         break;
      case GPS_STATE_GPGSA:
          if(ch == 0x0A || ch == '*')   //End of sentence
            g_nGPSState = GPS_STATE_IDLE;
         else if(ch == ',')   //Next param
         {
            g_nGPSParamIndex++;
            g_nGPSReceived = 0;
            g_bGPSNumSign = 0;
         }
         else
         {
            switch(g_nGPSParamIndex)   //Starts from 1
            {
               case 1:   //Mode 1
                  gps_ResetSatelliteSignal();
                  break;
               case 2:   //GPS Fix Type - 1:Fix not available, 2:2D, 3:3D
                     recGPSData.fixType = ch - '0';
                  break;
             case 3:   //Sat in used in channel 1
               case 4:   //Channel 2
               case 5:   //Channel 3
               case 6:   //ch 4
               case 7:   //ch 4
               case 8:   //ch 6
               case 9:   //ch7
               case 10:   //ch8
               case 11:   //ch 9
               case 12:   //ch10
               case 13:   //ch11
               case 14:   //ch12
               if(g_nGPSReceived == 0)
                  satData.nSatID = ch - '0';
               else if(g_nGPSReceived == 1)
               {
                  satData.nSatID *= 10;
                  satData.nSatID += ch - '0';
                  if(satData.nSatID <= 32)
                     recGPSData.satSNR[satData.nSatID-1] = 0x80;
               }
               
               g_nGPSReceived++;
                     break;
           
            } //switch(g_nGPSParamIndex)                                  
         }
         break;
      case GPS_STATE_GPGSV:
          if(ch == 0x0A || ch == '*')   //End of sentence
            g_nGPSState = GPS_STATE_IDLE;
         else if(ch == ',')   //Next param
         {
            g_nGPSParamIndex++;
            g_nGPSReceived = 0;
            g_bGPSNumSign = 0;
         }
         else
         {
            switch(g_nGPSParamIndex)   //Starts from 1
            {
               case 1:   //Number of GPGSV message
                  satData.nSatID = 0;
                  satData.nSatSNR = 0;   
                  break;
               case 2:   //Current GPGSV message number
                  break;
               case 3:   //Number of sat
                  break;
               case 4:   // Sat index
               case 8:
               case 12:
               case 16:
                     if(g_nGPSReceived == 0)
                     {
                        satData.nSatID = ch - '0';
                        satData.nSatSNR = 0;   
                     }
                     else if(g_nGPSReceived == 1)
                     {
                        satData.nSatID *= 10;
                        satData.nSatID += ch - '0';
                         if(satData.nSatID <= 32 && satData.nSatID > 0)
                           recGPSData.satSNR[satData.nSatID-1] = satData.nSatSNR;
                    }
                     g_nGPSReceived++;
                    break;
               case 5:   //
               case 9:
               case 13:
               case 17:
                 break;
               case 6:   //
               case 10:
               case 14:
               case 18:
                   break;
               case 7:   //SNR
               case 11:
               case 15:
               case 19:
                     if(g_nGPSReceived == 0)
                         satData.nSatSNR = ch - '0';
                    else if(g_nGPSReceived == 1)
                    {
                       satData.nSatSNR *= 10;
                         satData.nSatSNR += ch - '0';
                        if(satData.nSatID <= 32 && satData.nSatID > 0)
                           recGPSData.satSNR[satData.nSatID-1] = satData.nSatSNR;
               }
                     g_nGPSReceived++;
                     break;
            
            } //switch(g_nGPSParamIndex)                                  
         }
         break;
      case GPS_STATE_GPVTG:
            g_bGPSDataReady = 1;
            g_nGPSState = GPS_STATE_IDLE;
         break;
   }   //switch(g_nGPSState)

}

void RDA_isr()
{
   int ch;
   
   if(g_nGPSBufferWritePtr ==0)   //new sentence sec
         g_nTickNewData = g_nTimerTick;
         
   ch = fgetc(GPS);
   
   if(g_bGPStoBT)
      fputc(ch, BT_232);
   
   if(ch == 0xD)   //Ignore LF
      return;
      
   GPSBuffer[g_nGPSBufferWritePtr] = ch;
   if(ch == 0xA)   //Received one whole sentence
   {
         g_bGPSNewSentence = true;
      g_nGPSSentences++;
      g_nGPSBufferEOLPtr = g_nGPSBufferReadPtr;
   }   

   GPS_INC_WRITEPTR(1);
   if(g_nGPSBufferWritePtr == g_nGPSBufferReadPtr)   //Overlapped?
   {
         GPS_INC_READPTR(1);
         g_bGPSOverlapped = 1;
         g_nGPSOverlapped ++;
   }
   


}


float gps_strtof()
{
   float fResult, fTemp, fDiv;
   int bMinus;

   bMinus = 0;
   fResult = 0;

   if(GPSBuffer[g_nGPSBufferReadPtr] == '-')
   {
      bMinus = 1;
      GPS_INC_READPTR(1);
   }
   else if(GPSBuffer[g_nGPSBufferReadPtr] == '+')
   {
      GPS_INC_READPTR(1);
   }

   while(GPSBuffer[g_nGPSBufferReadPtr] >= '0' && GPSBuffer[g_nGPSBufferReadPtr] <= '9')
   {
      fResult *= 10;
      fTemp = GPSBuffer[g_nGPSBufferReadPtr] - '0';
      fResult += fTemp;
      GPS_INC_READPTR(1);
   }

   if(GPSBuffer[g_nGPSBufferReadPtr] == '.')
   {
      GPS_INC_READPTR(1);
      fDiv = 10;
      while(GPSBuffer[g_nGPSBufferReadPtr] >= '0' && GPSBuffer[g_nGPSBufferReadPtr] <= '9')
      {
         fTemp = GPSBuffer[g_nGPSBufferReadPtr] - '0';
         fResult += fTemp/fDiv;
         fDiv *= 10;
         GPS_INC_READPTR(1);
      }
   }

   if(bMinus)
      fResult *= -1;

   return fResult;   
}

long gps_strtoul()
{
   long lResult, lTemp;
   int nLen;

   nLen = 0;

   lResult = 0;

   while(GPSBuffer[g_nGPSBufferReadPtr] >= '0' && GPSBuffer[g_nGPSBufferReadPtr] <= '9')
   {
      lResult *= 10;
      lTemp = GPSBuffer[g_nGPSBufferReadPtr] - '0';
      lResult += lTemp;
      GPS_INC_READPTR(1);
      
      if(++nLen > 5)   //Max 65535
         break;
   }
   
   return lResult;
}

signed long gps_strtol()
{
   signed long lResult, lTemp;
   char bMinus;
   int nLen;
   
   nLen = 0;
   lResult = 0;
   bMinus=0;
   
   if(GPSBuffer[g_nGPSBufferReadPtr] == '-')
   {
      bMinus = 1;
      
      GPS_INC_READPTR(1);
   }
   else if(GPSBuffer[g_nGPSBufferReadPtr] == '+')
   {
      
      GPS_INC_READPTR(1);
   }

   while(GPSBuffer[g_nGPSBufferReadPtr] >= '0' && GPSBuffer[g_nGPSBufferReadPtr] <= '9')
   {
      lResult *= 10;
      lTemp = GPSBuffer[g_nGPSBufferReadPtr] - '0';
      lResult += lTemp;
      GPS_INC_READPTR(1);
      
      if(++nLen > 6)   //Max 65535
         break;
   }

   if(bMinus)
      lResult *= -1;
   return lResult;
}

//$GPGGA,070102.000,4911.1179,N,12247.2601,W,1,09,0.9,81.8,M,-16.7,M,,0000*52
//Get GPS sentence from GPSBuffer[g_nGPSBufferReadPtr]
int parseGPGGA()
{
   int nComma, nLen;
   float fTemp;
   int bFixed;

   nLen = 0;
   nComma = 0;
   bFixed = 0;

   recGPSData.numSat = 0;
   recGPSData.fAltitude = 0;

   //Skip unused items
   while(g_nGPSBufferReadPtr != g_nGPSBufferWritePtr && nComma < 6)
   {
      if(GPSBuffer[g_nGPSBufferReadPtr] == ',')
         nComma ++;
      GPS_INC_READPTR(1);
   }

   if(nComma < 6)
      return FALSE;

   if(GPSBuffer[g_nGPSBufferReadPtr] == '1')
   {
      bFixed = 1;

      GPS_INC_READPTR(2);

      recGPSData.numSat = gps_strtoul();
      GPS_INC_READPTR(1);

      fTemp = gps_strtof();
      GPS_INC_READPTR(1);

      recGPSData.fAltitude = gps_strtof();
   }

   return bFixed;
   
}

//$GPRMC,052457.000,A,4911.1152,N,12247.2628,W,0.00,16.90,290406,,,A*4C
int parseGPRMC()
{
   GPS_INC_READPTR(1);   //Skip first comma

   recGPSData.time.tm_hour = GPSBuffer[g_nGPSBufferReadPtr] - '0';
   GPS_INC_READPTR(1);
   recGPSData.time.tm_hour *= 10;
   recGPSData.time.tm_hour += GPSBuffer[g_nGPSBufferReadPtr] - '0';
   GPS_INC_READPTR(1);

   recGPSData.time.tm_min = GPSBuffer[g_nGPSBufferReadPtr] - '0';
   GPS_INC_READPTR(1);
   recGPSData.time.tm_min *= 10;
   recGPSData.time.tm_min += GPSBuffer[g_nGPSBufferReadPtr] - '0';
   GPS_INC_READPTR(1);

   recGPSData.time.tm_sec = GPSBuffer[g_nGPSBufferReadPtr] - '0';
   GPS_INC_READPTR(1);
   recGPSData.time.tm_sec *= 10;
   recGPSData.time.tm_sec += GPSBuffer[g_nGPSBufferReadPtr] - '0';   
   GPS_INC_READPTR(1);

   //Convert to local time
   recGPSData.time.tm_hour += 24;
//   recGPSData.time.tm_hour += gSettings.nTimeZone;
   recGPSData.time.tm_hour %= 24;

   while(GPSBuffer[g_nGPSBufferReadPtr] != ',')
      GPS_INC_READPTR(1);   //move to next comma

   GPS_INC_READPTR(1);   //Skip comma

   if(GPSBuffer[g_nGPSBufferReadPtr] != 'A')
      return FALSE;

   GPS_INC_READPTR(1);   //Skip A/V

   GPS_INC_READPTR(1);   //skip ','

   recGPSData.fLatitude = 0;
   recGPSData.fLongitude = 0;
   recGPSData.fSpeed = 0.0;

   recGPSData.fLatitude = gps_strtof();
   GPS_INC_READPTR(1);

   if(GPSBuffer[g_nGPSBufferReadPtr])
   {
      recGPSData.northSouth = GPSBuffer[g_nGPSBufferReadPtr];   //North/South
      GPS_INC_READPTR(1);
      GPS_INC_READPTR(1);
   }
   else
      return FALSE;

   recGPSData.fLongitude = gps_strtof();
   GPS_INC_READPTR(1);

   recGPSData.eastWest = GPSBuffer[g_nGPSBufferReadPtr];   
   GPS_INC_READPTR(1);
   GPS_INC_READPTR(1);

   recGPSData.fSpeed = gps_strtof();
   GPS_INC_READPTR(1);

   recGPSData.direction = (int16)gps_strtof();
   GPS_INC_READPTR(1);
   

   recGPSData.time.tm_day = GPSBuffer[g_nGPSBufferReadPtr] - '0';
   GPS_INC_READPTR(1);
   recGPSData.time.tm_day *= 10;
   recGPSData.time.tm_day += GPSBuffer[g_nGPSBufferReadPtr] - '0';
   GPS_INC_READPTR(1);

   recGPSData.time.tm_mon = GPSBuffer[g_nGPSBufferReadPtr] - '0';
   GPS_INC_READPTR(1);
   recGPSData.time.tm_mon *= 10;
   recGPSData.time.tm_mon += GPSBuffer[g_nGPSBufferReadPtr] - '0';
   GPS_INC_READPTR(1);

   recGPSData.time.tm_year = GPSBuffer[g_nGPSBufferReadPtr] - '0';
   GPS_INC_READPTR(1);
   recGPSData.time.tm_year *= 10;
   recGPSData.time.tm_year += GPSBuffer[g_nGPSBufferReadPtr] - '0';   
   GPS_INC_READPTR(1);
   recGPSData.time.tm_year += 2000;   
   
   myrec.tm_year = recGPSData.time.tm_year;
   myrec.tm_mon = recGPSData.time.tm_mon;
   myrec.tm_day = recGPSData.time.tm_day;
   myrec.tm_mday = recGPSData.time.tm_day;
   myrec.tm_hour = recGPSData.time.tm_hour;
   myrec.tm_min = recGPSData.time.tm_min;
   myrec.tm_sec = recGPSData.time.tm_sec;

   recGPSData.fSpeed *= 1.852;

   return TRUE;
   
}

//$GPGSV,3,1,12,20,00,000,,10,00,000,,25,00,000,,27,00,000,*79
//$GPGSV,3,2,12,22,00,000,,07,00,000,,21,00,000,,24,00,000,*79
//$GPGSV,3,3,12,16,00,000,,28,00,000,,26,00,000,,29,00,000,*78
//Get GPS sentence from GPSBuffer[g_nGPSBufferReadPtr]
int parseGPGSV()
{
   int nComma;
   int nNumMsg, nMsgID, nNumSat;
   int nSatID, nSatElevation, nSatSNR;
   long nSatAzimuth;

   nComma = 0;
   nNumMsg = 0;
   nMsgID = 0;
   nNumSat = 0;

   GPS_INC_READPTR(1);   //Skip first comma

   nNumMsg = (int)gps_strtoul();   //Get total number of message
   GPS_INC_READPTR(1);   //Skip comma
   
   nMsgID = (int)gps_strtoul();   //Get current message number
   GPS_INC_READPTR(1);   //Skip comma
   
   nNumSat = (int)gps_strtoul();   //Get total number of satellite in sky
   GPS_INC_READPTR(1);   //Skip comma

   while(GPSBuffer[g_nGPSBufferReadPtr] != '*')
   {
      nSatID = (int)gps_strtof() -1;
      GPS_INC_READPTR(1);   //Skip comma
      nSatElevation = (int)gps_strtoul();
      GPS_INC_READPTR(1);   //Skip comma
      nSatAzimuth = gps_strtoul();
      GPS_INC_READPTR(1);   //Skip comma
      if(GPSBuffer[g_nGPSBufferReadPtr] == '*')
         break;
      nSatSNR = (int)gps_strtof();
      GPS_INC_READPTR(1);   //Skip comma
      
      if(nSatID < 32)
      {
         if(recGPSData.satSNR[nSatID] == 0x80)   //Fixed
            recGPSData.satSNR[nSatID] += nSatSNR;
         else
            recGPSData.satSNR[nSatID] = nSatSNR;
            
      }
   }

   return 1;
   
}

//$GPGSA,A,3,07,02,26,27,09,04,15, , , , , ,1.8,1.0,1.5*33
void parse_GPGSA()
{
   int nSatID;
   GPS_INC_READPTR(1);   //Skip comma

   GPS_INC_READPTR(2);   //Skip mode 1

   recGPSData.fixType = (int)gps_strtoul();   //Get fix mode - 1:Fix not available, 2:2D, 3:3D
   GPS_INC_READPTR(1);   //Skip comma
   
   while(GPSBuffer[g_nGPSBufferReadPtr] != ',' && GPSBuffer[g_nGPSBufferReadPtr] != ' ')
   {
      nSatID = (int)gps_strtoul() -1;
      recGPSData.satSNR[nSatID] = 0x80;   
      GPS_INC_READPTR(1);   //Skip comma
   }

}

void gps_ResetSatelliteSignal()
{
   int i;
   for(i=0; i<32; i++)
   {
      recGPSData.satSNR[i] = 0xFF;
   }
}

int16 g_nTickStart, g_nTickEnd;

void GPSParser()
{
   char sName[3];

         LCD_Gotoxy(0,16);
           printf(LCD_PutChar, "%2d-%3ld-%3ld ", g_nGPSSentences, g_nGPSBufferReadPtr,g_nGPSBufferWritePtr);
             if(g_bGPSOverlapped)
           {
              printf(LCD_PutChar, "0 %c%c%c", 
                 GPSBuffer[g_nGPSBufferReadPtr+3], GPSBuffer[g_nGPSBufferReadPtr+4], GPSBuffer[g_nGPSBufferReadPtr+5]);
            g_bGPSOverlapped = 0;
           }
           else
              printf(LCD_PutChar, " %c%c%c %d ", 
                 GPSBuffer[g_nGPSBufferReadPtr+3], GPSBuffer[g_nGPSBufferReadPtr+4], GPSBuffer[g_nGPSBufferReadPtr+5], g_nGPSOverlapped);

   if(GPSBuffer[g_nGPSBufferReadPtr] == '$')   //Valid GPS sentece?
   {
      GPS_INC_READPTR(1);   //Skip '$'
      
      if(GPSBuffer[g_nGPSBufferReadPtr] == 'G')
      {
         GPS_INC_READPTR(1);   //Skip 'G'
         
         if(GPSBuffer[g_nGPSBufferReadPtr] == 'P')
         {
            GPS_INC_READPTR(1);   //Skip 'P'
   
            sName[0] = GPSBuffer[g_nGPSBufferReadPtr];
            GPS_INC_READPTR(1);
            sName[1] = GPSBuffer[g_nGPSBufferReadPtr];
            GPS_INC_READPTR(1);
            sName[2] = GPSBuffer[g_nGPSBufferReadPtr];
            GPS_INC_READPTR(1);
            
            if(sName[0] == 'G' && sName[1] == 'G' && sName[2] == 'A')   //Begin of sentece, every second
            {
               g_nTickStart = g_nTimerTick;
               g_bGPSDataReady = 0;
               g_bGPSFixed = parseGPGGA();
            }
            else if(sName[0] == 'R' && sName[1] == 'M' && sName[2] == 'C')   //every second
            {
               parseGPRMC();
            }
            else if(sName[0] == 'G' && sName[1] == 'S' && sName[2] == 'A')   //Accompany with GPGSV, very 3 seconds (GR86 default)
            {
               gps_ResetSatelliteSignal();
            }
            else if(sName[0] == 'G' && sName[1] == 'S' && sName[2] == 'V')   //Satellite infomation, every 3 seconds (GR86 default)
            {
               parseGPGSV();
            }
            else if(sName[0] == 'V' && sName[1] == 'T' && sName[2] == 'G')   //Last sentence received
            {
               g_bGPSDataReady = 1;
               g_nGPSBufferWritePtr = 0;
               g_nGPSBufferReadPtr = g_nGPSBufferWritePtr;
               g_nTickEnd = g_nTimerTick;
                 printf(LCD_PutChar, "%ld %ld ", g_nTickEnd -g_nTickStart, g_nTickEnd - g_nTickNewData);
            }
         }
      }
   }
      
      
   //Set read pointer to next sentence
   while(g_nGPSBufferReadPtr != g_nGPSBufferWritePtr && GPSBuffer[g_nGPSBufferReadPtr] != 0x0A)
   {
      GPS_INC_READPTR(1);   //Skip CR
   }

   if(g_nGPSBufferReadPtr != g_nGPSBufferWritePtr && GPSBuffer[g_nGPSBufferReadPtr] == 0x0A)
      GPS_INC_READPTR(1);   //Skip CR
   
}

/*
$GPGGA,000327.049,0000.0000,N,00000.0000,E,0,00,,0.0,M,0.0,M,,0000*48
$GPRMC,000327.049,V,0000.0000,N,00000.0000,E,,,160406,,,N*78
$GPVTG,,T,,M,,N,,K,N*2C
$GPGGA,000328.051,0000.0000,N,00000.0000,E,0,00,,0.0,M,0.0,M,,0000*4E
$GPRMC,000328.051,V,0000.0000,N,00000.0000,E,,,160406,,,N*7E
$GPVTG,,T,,M,,N,,K,N*2C
$GPGGA,000329.056,0000.0000,N,00000.0000,E,0,00,,0.0,M,0.0,M,,0000*48
$GPGSA,A,1,,,,,,,,,,,,,,,*1E
$GPGSV,3,1,12,20,00,000,,10,00,000,,25,00,000,,27,00,000,*79
$GPGSV,3,2,12,22,00,000,,07,00,000,,21,00,000,,24,00,000,*79
$GPGSV,3,3,12,16,00,000,,28,00,000,,26,00,000,,29,00,000,*78
$GPRMC,000329.056,V,0000.0000,N,00000.0000,E,,,160406,,,N*78
$GPVTG,,T,,M,,N,,K,N*2C
$GPGGA,000330.049,0000.0000,N,00000.0000,E,0,00,,0.0,M,0.0,M,,0000*4E
$GPRMC,000330.049,V,0000.0000,N,00000.0000,E,,,160406,,,N*7E
$GPVTG,,T,,M,,N,,K,N*2C
*/
