
#use FAST_IO(C)
#define ChipSel pin_c2
#define ChipClk pin_c3
#define ChipDin pin_c5

//#define TRACE
//#define TRACE_READ_SECTOR
//#define TRACE_WRITE_SECTOR


#define NO_MMC_CARD	input(CardInserted)


int mmc_cmd(int8 cmd,int32 address,int8 tries,int8 valid,int8 invalid)
{
	int i,r1;
	for( i=0;i<16;i++) 
		SPI_READ(0xFF);// digest prior operation
	// commands
	// 7 6 5 4 3 2 1 0
	// 0 1 b b b b b b    bbbbbb=cmd
	// 16=0x50 set blocklength
	// 17=0x51 read block
	// 24=0x58 write block
#ifdef TRACE
//	TRACE2("\r\ncmd=%2X, address=%lu",cmd, address);
#endif

	SPI_READ(cmd);
	SPI_READ(MAKE8(address,3));
	SPI_READ(MAKE8(address,2));
	SPI_READ(MAKE8(address,1));
	SPI_READ(MAKE8(address,0));
	SPI_READ(0x95);
	// card comes up in MMC mode and requires a valid MMC cmd to switch to SPI mode
	// valid crc for MMC 0x40 cmd only
	// spi mode doesn't require the CRC to be correct just there

	for(i=0;i< tries;i++) 
	{
		r1=SPI_READ(0xFF);
		if (r1==valid)  break;
		if (r1==invalid)  break;
	}

	return(r1);
}

int init_MMC(int max_tries)
{
	int i,tries,c;
	tries=0;
		
	output_low(ChipSel);   /// reset chip hardware !!! required
	delay_ms(100);

	for(tries=0; tries < max_tries; tries++)
	{
		output_high(ChipSel);   /// reset chip hardware !!! required
		delay_ms(20);
		for(i=0;i<20;i++) 
			SPI_READ(0xFF); // min 80 clocks to get MMC ready

		output_low(ChipSel);   ///                      !!! required
	
		delay_ms(20);

		c=mmc_cmd(0x40,0x00000000,128,0x01,0x99);

		if (c==0x01) 
			break;
	}
	
	if(tries >= max_tries)
	{
		output_high(ChipSel);
		return MMC_INIT_RESET_ERR;
	}

	/// now try  to switch to idle mode
	/// Note: cmd1(idle) is the only command allowed after a cmd0(reset)

	for(tries=0; tries < max_tries; tries++)
	{
		c=mmc_cmd(0x41,0x00000000,128,0x00,0x99);
		if (c==0x00) 
			break;
	}

	output_high(ChipSel);
	
	if(tries >= max_tries)
		return MMC_INIT_IDLE_ERR;
		
	return MMC_NO_ERR;

}

int mmc_response(unsigned char response)
{
	unsigned long count = 0xFFFF;           // 16bit repeat, it may be possible to shrink this to 8 bit but there is not much point

	while(SPI_READ(0xFF) != response && --count > 0);

	if(count==0) 
		return 1;                  // loop was exited due to timeout
	else 
		return 0;                          // loop was exited before timeout
}


int ReadSector( int32 sector, char *buff)
{
	int r1;
	long i,iw; /// allows large gt 255 buff size addressing

	output_low(ChipSel);   
	delay_ms(1);
	
	TRACE1("\r\nRead sector# %lu.", sector);
	
	r1=mmc_cmd(0x51,sector<<9,16,0x00,0x40);

	if(r1 == 0x40)
	{
		output_high(ChipSel);
		return MMC_READ_INVALID_ERR;
	}
	else if(r1 != 0x00)
	{
		output_high(ChipSel);
		return MMC_READ_GEN_ERR;
	}
	
	//Get token
	for(iw=0;iw<1024;iw++)
	{
		r1=SPI_READ(0xFF);
		if (r1==0xFE) 
			break;
	}
	
	//Get token error. It may be caused by improper MMC reset
	if(r1 != 0xFE)
	{
		output_high(ChipSel);
		return MMC_READ_TOKEN_ERR;
	}

	//Read the whole sector (512 bytes)
	for (i=0;i<512;i++) 
		buff[i]=SPI_READ(0xFF);

	SPI_READ(0xFF); // read crc
	SPI_READ(0xFF);

#ifdef TRACE_READ_SECTOR
	fprintf(debug, "\r\nRead sector #%lu:", sector);

	for(i = 0; i<512; i++)
	{
		if(i%16 == 0)
			fprintf(debug, "\r\n%04LX - ", i);
		r1 = *buff;
		if(r1 < ' ' || r1 > 'z')
			r1 = '.';
		fprintf(debug, "%02X%c ", *buff++, r1);

	}
#endif

	output_high(ChipSel);
	return MMC_NO_ERR;
}

int WriteSector(int32 sector, char *buff)

{
	int r1;
	int16 i;


	TRACE1("\r\nWriteSector(%lu).", sector);

	if(sector == 0)	//never write sector 0
		return MMC_WRITE_SEC0_ERR;

	output_low(ChipSel);
	delay_ms(1);
	
	r1=mmc_cmd(0x58,sector<<9,16,0x00,0x40);


	if(r1 == 0x40)
	{
		output_high(ChipSel);	
		return MMC_WRITE_INVALID_ERR;
	}
	else if(r1 != 0x00)
	{
		output_high(ChipSel);
		return MMC_WRITE_GEN_ERR;
	}

	SPI_READ(0xFE);

	for (i=0;i < 512;i++) 
	{
		SPI_READ(buff[i]);  /// send payload
	}


	SPI_READ(0xFF); // send dummy chcksum
	SPI_READ(0xFF);
	r1=SPI_READ(0xFF);
	for( i=0;i<0x0fff;i++) 
	{
		r1=SPI_READ(0xFF);// digest prior operation
		if (r1!=0x00) 
			break;
	}

#ifdef TRACE_WRITE_SECTOR
	fprintf(debug, "\r\nWrite sector #%lu:", sector);

	for(i = 0; i<512; i++)
	{
		if(i%16 == 0)
			fprintf(debug, "\r\n%04LX - ", i);
		r1 = *buff;
		if(r1 < ' ' || r1 > 'z')
			r1 = '.';
		fprintf(debug, "%02X%c ", *buff++, r1);

	}

#endif

	output_high(ChipSel);
	return MMC_NO_ERR;
}

