#ifndef ADS11XX_H
#define ADS11XX_H


// Conversion Mode
#define ADS11XX_SINGLE_CONVERSION	0x10
#define ADS11XX_CONTINUOUS_CONVERSION	0x00

// Effective Bits
#define ADS11XX_12_BITS		0x00	//128 SPS
#define ADS11XX_14_BITS		0x04	//32 SPS
#define ADS11XX_15_BITS		0x08	//16 SPS
#define ADS11XX_16_BITS		0x0C	// 8 SPS

// Onchip OP gain
#define ADS11XX_PGA_1		0x00
#define ADS11XX_PGA_2		0x01
#define ADS11XX_PGA_4		0x02
#define ADS11XX_PGA_8		0x03

//for ADS1112 input selection
#define ADS11XX_INP_0		0x00	//AIN0, AIN1
#define ADS11XX_INP_1		0x20	//AIN2, AIN3
#define ADS11XX_INP_2		0x40	//AIN0, AIN3
#define ADS11XX_INP_3		0x60	//AIN1, AIN3

#SEPARATE
void ADS11XX_Init( int nAddress, int configByte)
{
	i2c_start();

	i2c_write(nAddress);           // Send the address of the device
	i2c_write(configByte);

	i2c_stop();
}
#SEPARATE
int ADS11XX_ReadConfig(int nAddress)
{
	int tmp;

	i2c_start();

	tmp = i2c_write(nAddress| 0x01);           // Send the address of the device

	tmp = i2c_read(1);      // Ignore the output data
	tmp = i2c_read(1);      // Ignore the output data

	tmp = i2c_read(1);

	i2c_stop();

	return tmp;
}
#SEPARATE
long ADS11XX_ReadData(int nAddress)
{
	long ad_val;
	int tmp;

	i2c_start();

	i2c_write(nAddress | 0x01);           // Send the address of the device

	ad_val = i2c_read(1);    // High byte of data
	tmp = i2c_read(1);      // Low byte of data
	i2c_read(1);		//skip config data

	i2c_stop();

	ad_val <<= 8;
	ad_val += tmp;


	return ad_val;
}
#endif
