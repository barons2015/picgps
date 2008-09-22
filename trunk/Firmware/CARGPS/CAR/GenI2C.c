//Generic I2C functions
#ifndef GENI2C_H
#define GENI2C_H

#SEPARATE
void GenI2C_WriteSingleByte(int nI2CAddr, int nData)
{
	i2c_start();

	i2c_write(nI2CAddr);           // Send the address of the device

	i2c_write(nData);

	i2c_stop();
}

#SEPARATE
void GenI2C_WriteByte(int nI2CAddr, int nRegAddr, int nData)
{
	i2c_start();

	i2c_write(nI2CAddr);           // Send the address of the device

	i2c_write(nRegAddr);

	i2c_write(nData);

	i2c_stop();

}

#SEPARATE
int GenI2C_ReadByte(int nI2CAddr, int nRegAddr)
{
	int nData;

	//Write command byte
	i2c_start();
	i2c_write(nI2CAddr);           // Send the address of the device
	i2c_write(nRegAddr);
	//i2c_stop();

	//Read
	i2c_start();
	i2c_write(nI2CAddr | 0x01);           // Send the address of the device
	nData = i2c_read(0);
	i2c_stop();

	return nData;

}

#define EXT_EEPROM_WRITE_CMD  0xA2
#define EXT_EEPROM_READ_CMD   0xA3

#SEPARATE
byte read_ext_eeprom(long address)
{
  byte data;

   i2c_start();
   i2c_write(EXT_EEPROM_WRITE_CMD);
   i2c_write(hi(address));
   i2c_write(address);
   i2c_start();
   i2c_write(EXT_EEPROM_READ_CMD);
   data=i2c_read(0);
   i2c_stop();

   return(data);
}

#SEPARATE
void write_ext_eeprom(long address, byte data)
{
   i2c_start();

   i2c_write(EXT_EEPROM_WRITE_CMD);

   i2c_write(hi(address));
   i2c_write(address);
   i2c_write(data);
   i2c_stop();
   delay_ms(5);

}

#endif
