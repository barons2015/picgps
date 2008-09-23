
/* Below pins need to be defined 

#define NRF_CSN		PIN_C2	//NRF chip select
#define NRF_CE		PIN_F5
#define NRF_SCK		PIN_C3
#define NRF_DI		PIN_C4
#define NRF_DO		PIN_C5
#define NRF_IRQ		PIN_B1

*/

#define NRF_PACKET_SIZE	7
BYTE gNRFPacket[NRF_PACKET_SIZE + 1];

#define NRF_Enable(a)	a?output_high(NRF_CE):output_low(NRF_CE)
#define NRF_Select(a)	a?output_low(NRF_CSN):output_high(NRF_CSN)


byte _spi_write(byte nData)
{
	int i, resp;
	resp = 0;
	output_low(NRF_SCK);
	for(i=0; i<8; i++)
	{
		resp <<= 1;
		if(bit_test(nData, 7-i))
			output_high(NRF_DO);
		else
			output_low(NRF_DO);
		//delay_us(1);
		delay_cycles(1);
		output_high(NRF_SCK);
		//delay_us(1);
		delay_cycles(1);
		output_low(NRF_SCK);
		resp |= input(NRF_DI);
	}	
	return resp;
}
	
byte _spi_read()
{
	int i;
	byte ch=0;

	for(i=0; i<8; i++)
	{
		output_low(NRF_SCK);

		//delay_us(1);
		delay_cycles(1);
		output_high(NRF_SCK);
		//delay_us(1);
		delay_cycles(1);
		if(input(NRF_DI))
			bit_set(ch, 7-i);
	}	
	
	output_low(NRF_SCK);
	
	return ch;
}

byte NRF_ReadRegister(int nAddress)
{
	byte nResult;
	
	//Every new command must be started by a high to low transition on CSN
	NRF_Select(FALSE);
	delay_us(1);	//Tcwh = 50ns, CSN Inactive time
	NRF_Select(TRUE);	//Chip select
	delay_us(1);	//Tcsd = 38ns, CSN to Data Valid
	
	_spi_write(nAddress&0x1F);
	nResult = _spi_read();
	
	NRF_Select(FALSE);
	
	return nResult;
}
	
byte NRF_WriteRegister(int nCommand, int nData)
{
	int nStatus;
	//Every new command must be started by a high to low transition on CSN
	NRF_Select(FALSE);
	delay_us(1);	//Tcwh = 50ns, CSN Inactive time
	NRF_Select(TRUE);	//Chip select
	delay_us(1);	//Tcsd = 38ns, CSN to Data Valid
	
	nStatus = _spi_write(nCommand);
	_spi_write(nData);
	
//	NRF_Select(FALSE);
	
	return nStatus;
		
}

void NRF_WriteCommand(int nCommand)
{
	//Every new command must be started by a high to low transition on CSN
	NRF_Select(FALSE);
	delay_us(1);	//Tcwh = 50ns, CSN Inactive time
	NRF_Select(TRUE);	//Chip select
	delay_us(1);	//Tcsd = 38ns, CSN to Data Valid
	
	_spi_write(nCommand);
		
}
	
void NRF_config_rf()
{
	//auto retransmit off
	//NRF_WriteRegister(0x24, 0x00);
	//Disable auto act
	//NRF_WriteRegister(0x21, 0x00);
	
	//auto retransmit 15 times, delay 500us
	NRF_WriteRegister(0x24, 0x1F);
	
	//Auto act on at all pipe 
	NRF_WriteRegister(0x21, 0x3F);
	
	//Address width = 5
	NRF_WriteRegister(0x23, 0x03);
	
	//Data rate = 1MB
	NRF_WriteRegister(0x26, 0x07);

	//7 bytes Payload
	NRF_WriteRegister(0x31, NRF_PACKET_SIZE);	
	
	
	//Set channel 2
	NRF_WriteRegister(0x25, 0x02);
	
	//Set TX address E7E7E7E7E7
	NRF_WriteCommand(0x30);
	_spi_write(0xE7);
	_spi_write(0xE7);
	_spi_write(0xE7);
	_spi_write(0xE7);
	_spi_write(0xE7);
	
	//Set RX address E7E7E7E7E7
	NRF_WriteCommand(0x2A);
	_spi_write(0xE7);
	_spi_write(0xE7);
	_spi_write(0xE7);
	_spi_write(0xE7);
	_spi_write(0xE7);
	
	NRF_Select(FALSE);

}
	
void NRF_config_tx()
{
	
	NRF_Enable(FALSE);	//RF Power down mode
	
	//PTX, CRC enabled
	NRF_WriteRegister(0x20, 0x38);
	
	//config RF interface, e.g. channel, address, data rate, etc
	NRF_config_rf();

}	

void NRF_config_rx()
{
	NRF_Select(FALSE);
	NRF_Enable(FALSE);
	
	//PRX, CRC enabled
	NRF_WriteRegister(0x20, 0x39);
	
	//config RF interface, e.g. channel, address, data rate, etc
	NRF_config_rf();
	
	//PWR_UP=1
	NRF_WriteRegister(0x20, 0x3B);
	
	NRF_Select(FALSE);
	
	//Enter RX mode (CE=1 && PRX)
	NRF_Enable(TRUE);
}

void NRF_reset_rx()
{
	int i;
	
	//Read RX payload
	NRF_WriteCommand(0x61);
	
	for(i=0; i<NRF_PACKET_SIZE; i++)
	{
		gNRFPacket[i] = _spi_read();
	}
	
	gNRFPacket[NRF_PACKET_SIZE] = 0;
	
	//Flush rx FIFO
	NRF_WriteCommand(0xE2);
	
	//Reset int
	NRF_WriteRegister(0x27, 0x40);
	
	NRF_Select(FALSE);

}	
	
void NRF_transmit_data(BYTE *pBuffer, int nLen)
{
	int i;
	
	NRF_Select(TRUE);
	
	//Clear previous ints
	NRF_WriteRegister(0x27, 0x7E);

	//PWR_UP=1
	NRF_WriteRegister(0x20, 0x3A);
	delay_ms(2);	//Takes 1.5ms from PowerDown to Standby mode
	
	//Clear TX fifo
	NRF_WriteCommand(0xE1);
	
	//7 bytes payload
	NRF_WriteCommand(0xA0);
	
	//Store payload
	for(i=0; i<nLen; i++)
	{
		_spi_write(pBuffer[i]);
	}	
	NRF_Select(FALSE);
	
	//Pulse CE to start transmission
	delay_ms(1);
	NRF_Enable(TRUE);
	delay_ms(1);
	NRF_Enable(FALSE);
	
}

void NRF_DumpRegister()
{
	int i;
	int ch;
	for(i=0; i<0x18; i++)
	{
		ch = 0;
		ch = NRF_ReadRegister(i);
		TRACE2("REG 0x%02X= 0x%02X\r\n", i, ch);
	}
}	

/*	

void NRF_Test() 
{
	char ch;
	int i;
	int nForceRx=0;
	int nMode = 0;	//default RX mode
	int nPacketCount = 0;
	
	PLLEN=1; //Enable 48MHz PLL
	LED_GREEN_OFF;

	delay_ms(200);

	setup_wdt(WDT_OFF);
	setup_timer_0(RTCC_INTERNAL);
	setup_timer_2(T2_DIV_BY_1,255,1);
	//setup_timer_2(T2_DISABLED,0,1);
	setup_timer_3(T3_INTERNAL|T3_DIV_BY_2);
	setup_timer_4(T4_DISABLED,0,1);
	setup_comparator(NC_NC_NC_NC);
	setup_vref(FALSE);

//	enable_interrupts(INT_RDA);
	set_timer0(5560);    //10ms interrupt
	enable_interrupts(INT_TIMER0);
	enable_interrupts(GLOBAL);

	usb_init();

	delay_ms(100);
	
	NRF_config_rx();
	
	i=0;
	while(1)
	{
		if(nDataInTick == 0)
		{
			LED_GREEN_OFF;
			LED_RED_OFF;
		}
		else
		{
			LED_GREEN_ON;
			LED_RED_ON;
		}		
		
			
		//wait for char from usb port
		if(usb_cdc_kbhit())
		{
			ch = usb_cdc_getc();
			if(ch == 'r')
			{
				NRF_config_rx();
				nMode = 0;
				nForceRx = 1;
			}	
			
			else if(ch == 'c')	//check registers
			{
				NRF_DumpRegister();
			}
			else if(ch == 't')	//go to transmit mode
			{
				printf(usb_cdc_putc,"Enter TX mode\r\n");
				NRF_config_tx();
				nMode = 1;
			}		
			else if(ch == 'v')
			{
				printf(usb_cdc_putc,"GPLDR Debugger\r\n");
			}	

			nDataInTick = 10;
		}
		
		//Do TX/RX	
		if(nMode == 1)	//Transmit mode
		{
			if(n1sTick == 0)	//1 second
			{
				n1sTick = 1;
				nDataInTick = 10;
				sprintf(gNRFPacket, "%07u", nPacketCount++);
				NRF_transmit_data(gNRFPacket, NRF_PACKET_SIZE);
				//printf(usb_cdc_putc, "Transmitting data\r\n");
			}	
		}
		else
		{

			if(input(NRF_IRQ) == 0 || nForceRx) //IRQ active low
			{
				nForceRx = 0;
				//	delay_ms(200);
				printf(usb_cdc_putc, "\r\n%03u Recv:", i++);
				nDataInTick = 20;
				NRF_reset_rx();
				delay_ms(10);
			}	
		}
	};

}


*/