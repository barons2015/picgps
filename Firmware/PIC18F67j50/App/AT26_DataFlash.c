
#use FAST_IO(C)
#define AT26ChipSel PIN_F6
#define AT26ChipClk PIN_C3
#define AT26ChipDin PIN_C4
#define AT26ChipDout PIN_C5

//Read Commands
#define AT26CMD_READ_ARRAY      0x0B
#define AT26CMD_READ_ARRAY_SLOW   0x03

//Program and Erase Commands
#define AT26CMD_ERASE_4KB      0x20
#define AT26CMD_ERASE_32KB      0x52
#define AT26CMD_ERASE_64KB      0xD8
#define AT26CMD_ERASE_CHIP      0x60
#define AT26CMD_PROGRAM         0x02

//Protection Commands
#define AT26CMD_WRITE_ENABLE   0x06
#define AT26CMD_WRITE_DISABLE   0x04
#define AT26CMD_PROTECT_SECTOR   0x36
#define AT26CMD_UNPROTECT_SECTOR   0x39
#define AT26CMD_READ_SECTOR_PROTECTION_REGISTERS   0x3C

//Status Register Commands
#define AT26CMD_READ_STATUS_REGISTER   0x05
#define AT26CMD_WRITE_STATUS_REGISTER   0x01

//Miscellaneous Command
#define AT26CMD_READ_MANUFACTURER_DEVICE_ID   0x9F
#define AT26CMD_DEEP_POWER_DOWN      0xB9
#define AT26CMD_RESUME_FROM_DEEP_POWER_DOWN   0xAB

int32 AT26_ReadDeviceID()
{
   byte byRead;
   int32 nResp = 0;
   
   output_low(AT26ChipSel);
   
   SPI_WRITE(AT26CMD_READ_MANUFACTURER_DEVICE_ID);
   
   //Read Manufacturer ID
   byRead = SPI_READ(0);
   nResp = byRead;
   nResp <<= 8;
   //Read Device ID Part I
   byRead = SPI_READ(0);
   nResp += byRead;
   nResp <<= 8;
   //Read Device ID Part II
   byRead = SPI_READ(0);
   nResp += byRead;
   nResp <<= 8;
   
   byRead = SPI_READ(0);
   nResp += byRead;
   
   output_high(AT26ChipSel);
   
   return nResp;   
}

byte AT26_ReadStatusRegister()
{
   byte byRead;
   
   output_low(AT26ChipSel);
   
   SPI_WRITE(AT26CMD_READ_STATUS_REGISTER);
    byRead = SPI_READ(0);
    //Note: Doesn't work with : byRead = SPI_READ(AT26CMD_READ_STATUS_REGISTER);
  
   output_high(AT26ChipSel);
 
   return byRead;
}


void AT26_WriteStatusRegister(int nData)
{
  
   output_low(AT26ChipSel);
   
   SPI_WRITE(AT26CMD_WRITE_STATUS_REGISTER);
   SPI_WRITE(nData);
   
   output_high(AT26ChipSel);
 
}


void AT26_WriteEnable()
{
  
   output_low(AT26ChipSel);
   
   SPI_WRITE(AT26CMD_WRITE_ENABLE);
   
   output_high(AT26ChipSel);
 
}

void AT26_WriteDiable()
{
  
   output_low(AT26ChipSel);
   
   SPI_WRITE(AT26CMD_WRITE_DISABLE);
   
   output_high(AT26ChipSel);
 
}

byte AT26_ReadSectorProtection(int32 nAddress)
{
      byte byRead;
   
   output_low(AT26ChipSel);
   
   SPI_WRITE(AT26CMD_READ_SECTOR_PROTECTION_REGISTERS);

   SPI_WRITE(make8(nAddress, 2));
   SPI_WRITE(make8(nAddress, 1));
   SPI_WRITE(make8(nAddress, 0));
   
   byRead = SPI_READ(0);
    //Note: Doesn't work with : byRead = SPI_READ(AT26CMD_READ_STATUS_REGISTER);
  
   output_high(AT26ChipSel);
 
   return byRead;
}

void AT26_UnprotectSector(int32 nAddress)
{  
   output_low(AT26ChipSel);
   
   SPI_WRITE(AT26CMD_UNPROTECT_SECTOR);

   SPI_WRITE(make8(nAddress, 2));
   SPI_WRITE(make8(nAddress, 1));
   SPI_WRITE(make8(nAddress, 0));
   
   output_high(AT26ChipSel);
}

void AT26_WriteByte(int32 nAddress, byte nData)
{
   output_low(AT26ChipSel);
   
   SPI_WRITE(AT26CMD_PROGRAM);

   SPI_WRITE(make8(nAddress, 2));
   SPI_WRITE(make8(nAddress, 1));
   SPI_WRITE(make8(nAddress, 0));
 
   SPI_WRITE(nData);
   
   output_high(AT26ChipSel);
}

byte AT26_ReadByte(int32 nAddress)
{
   byte byRead;
   
   output_low(AT26ChipSel);
   
   SPI_WRITE(AT26CMD_READ_ARRAY);

   SPI_WRITE(make8(nAddress, 2));
   SPI_WRITE(make8(nAddress, 1));
   SPI_WRITE(make8(nAddress, 0));
   SPI_WRITE(0);  //Dummy byte
 
   byRead = SPI_READ(0);
   
   output_high(AT26ChipSel);
 
   return byRead;
}

void AT26_BlockErase4K(int32 nAddress)
{
   output_low(AT26ChipSel);
   
   SPI_WRITE(AT26CMD_ERASE_4KB);

   SPI_WRITE(make8(nAddress, 2));
   SPI_WRITE(make8(nAddress, 1));
   SPI_WRITE(make8(nAddress, 0));
   
   output_high(AT26ChipSel);
}
