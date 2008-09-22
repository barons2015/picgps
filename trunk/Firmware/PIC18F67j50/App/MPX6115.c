
#include <math.h>

//MPX6115 AD
#define ADS1100_ADDRESS   0x90
#define MPX_EN   SENSOR_ENX
#define ADS1100_CONFIG   (ADS11XX_CONTINUOUS_CONVERSION | ADS11XX_16_BITS | ADS11XX_PGA_1)

#define ADC_ADDR_MPX    ADS1100_ADDRESS
#define ADC_CFG_MPX     ADS1100_CONFIG

#define VOL_BATT_ADJ   0.000157600
#define VOL_DROP_FDN304PZ  0.0015   //1.5mv drop on mosfet

#define MPX_P_OFFSET 0.31

float fMPX_V;  //MPX sensor voltage,
float fMPX_Vadj;  //MPX sensor voltage, adjust to 5V

float fMPX_P;  //MPX sensor pressure
float fMPX_H;  //MPX sensor altitude
signed long lMPX_V;  //MPX sensor origianal adc value
float fStandardPressure = 101.96;   //Sea level pressure 101.325(kPa) 

float fRelativeAltBase;
float fMPX_H_old;
#define fVBatt fVLDO

void ReadMPXPressure();

void ReadMPXPressure()
{
   //Read MPX pressure sensor
   lMPX_V = ADS11XX_ReadData(ADC_ADDR_MPX);
   fMPX_V = (float)lMPX_V*fVBatt/32767;

   //(Vsen5v - VsenBatt)/(5V - Vbatt) = 0.83
   fMPX_Vadj = fMPX_V + (5-fVBatt-VOL_DROP_FDN304PZ)*0.83;
   //fMPX_Vadj = fMPX_V + 5*VOL_DROP_FDN304PZ*0.83 - fVBatt*0.83;

   //fMPX_VAdj += ((float)iMPX_T-40.0)*VOL_MPX_TEMP_ADJ;

   //Calculate MPX6115A pressure
   //fMPX_P = (fMPX_Vadj/5+0.095)/0.009;
   fMPX_P = (fMPX_Vadj- 0.4*0.009*5)/0.045 + (0.095/0.009);
   //fMPX_P = fMPX_P - MPX_P_OFFSET;

   //Calculate altitude
   //fMPX_H = -(log(fMPX_P/fStandardPressure)*273.2*287.1/9.80665);
   fMPX_H = 44330*(1-pow((fMPX_P/fStandardPressure),0.19));
/*
   if(abs(fMPX_H - fMPX_H_old) > 0.2)
   {
      iIntervalSensor = 1;
      iTickFastSense = 60;
   }
   else if(iTickFastSense == 0)
   {
      iIntervalSensor = 10;
   }
*/ 
   fMPX_H_old = fMPX_H;

}


