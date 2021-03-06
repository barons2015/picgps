#ifndef SCP1000_H
#define SCP1000_H

//Address
#define SCP1000_ADDRESS      0x22

//Registers
#define SCPREG_REVID   0x00
#define SCPREG_DATAWR   0x01   //Indirect register access data
#define SCPREG_ADDPTR   0x02   //Indirect register access pointer
#define SCPREG_OPERATION   0x03   //Operation register
#define SCPREG_OPSTATUS   0x04   //Operation status
#define SCPREG_RSTR      0x06   //ASIC software reset
#define SCPREG_STATUS   0x07   //ASIC top-level status
#define SCPREG_DATARD8   0x7F   //Pressure output data (MSB)
#define SCPREG_DATARD16   0x80   //Pressure output data (LSB)
#define SCPREG_TEMPOUT   0x81   //14bit temperature output data

//Operation Code
#define SCPOP_NOP       0x00  //No operation/cancel current operation/stop continuous sampling
#define SCPOP_READREG   0x01  //Read indirect access register pointed by ADDPTR. Register contents is available in DATARD16 in bits[7:0]
#define SCPOP_WRITEREG  0x02  //Write DATAWR contents in to the indirect access register pointed by ADDPTR
#define SCPOP_READEEPROM   0x05  //Read EEPROM register pointed by ADDPTR. Register contents is available in DATARD8 in bits [7:0]
#define SCPOP_WRITEEEPROM  0x06  //Write DATAWR contents into the EEPROM register pointed by ADDPTR.
#define SCPOP_INIT      0x07     //Perform INIT sequence
#define SCPOP_HSPMODE   0x09     //High speed continuous mode , continuous
#define SCPOP_HRESMODE  0x0A     //High resolution mode, continuous
#define SCPOP_UPWRMODE  0x0B     //Utra low power acquisition mode, continuous
#define SCPOP_TRIG      0x0C     //Low Power acquisition mode. single sample.
#define SCPOP_SELFTEST  0x0F     //ASIC Self Test

int SCP1000_ReadRegister8(int nRegAddr)
{
   int nResult;
   
   i2c_start();

   if(i2c_write(SCP1000_ADDRESS))           // Send the address of the device
      return 0xAA;   //NO ACK

   if(i2c_write(nRegAddr))
      return 0xAB;
   
   i2c_start();
   if(i2c_write(SCP1000_ADDRESS| 0x01))           // Send the address of the device
      return 0xAC;
   
   nResult = i2c_read(0);      

   i2c_stop();

   return nResult;   
}

int SCP1000_WriteRegister8(int nRegAddr, int nData)
{
   i2c_start();

   if(i2c_write(SCP1000_ADDRESS))           // Send the address of the device
      return 0xAA;   //NO ACK

   if(i2c_write(nRegAddr))
      return 0xAB;
    
   i2c_write(nData);      

   i2c_stop();
   
   return 0;
}

int16 SCP1000_ReadRegister16(int nRegAddr)
{
   int16 nTemp;
   
   i2c_start();

   if(i2c_write(SCP1000_ADDRESS))           // Send the address of the device
      return 0xAA;   //No ACK

   i2c_write(nRegAddr);
   
   i2c_start();
   i2c_write(SCP1000_ADDRESS| 0x01);           // Send the address of the device
   
   nTemp = i2c_read(1);     
   nTemp <<= 8;
   nTemp += i2c_read(0);     

   i2c_stop();

   return nTemp;  
}

int   SCP1000_ReadID()
{
   return SCP1000_ReadRegister8(SCPREG_REVID);  
}

int   SCP1000_ReadStatus()
{
   return SCP1000_ReadRegister8(SCPREG_STATUS);  
 
}

int16 SCP1000_ReadTemperature()
{
   return SCP1000_ReadRegister16(SCPREG_TEMPOUT);
}

int16 SCP1000_ReadPressure()
{
   return SCP1000_ReadRegister16(SCPREG_DATARD16);

}

void SCP1000_Reset()
{
   SCP1000_WriteRegister8(SCPREG_RSTR, 0x01);   //Reset software
}

int SCP1000_SelfTest()
{
   SCP1000_WriteRegister8(SCPREG_OPERATION, SCPOP_SELFTEST); //Self test
   delay_ms(100);
   return SCP1000_ReadRegister8(SCPREG_DATARD8);
}
#endif
