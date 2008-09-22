
#include <18F67J50.h>
#device adc=8
#DEVICE HIGH_INTS=TRUE

#FUSES NOWDT                    //No Watch Dog Timer
#FUSES PLL1	                //4MHz input
#FUSES NOCPUDIV                 //No CPU system clock divide
#FUSES NODEBUG                  //No Debug mode for ICD
#FUSES NOPROTECT                //Code not protected from reading
#FUSES H4_SW			//HS-PLL, USB-HS-PLL
#FUSES NOIESO

//#build(reset=0x1000, interrupt=0x1008) // Move the reset and interrupt vectors 
//#org 0, 0xFFF {}   // Reserve the bootloader memory area 


#use delay(clock=48000000)  
#use rs232(baud=38400,parity=N,xmit=PIN_C6,rcv=PIN_C7,bits=8)


// Includes all USB code and interrupts, as well as the CDC API
#include "usb_cdc.h"
#include <stdlib.h>

#define LED_GRN		PIN_E0	//output low to turn of green LED
#define LED_RED		PIN_E1
#define NRF_CSN		PIN_C2	//NRF chip select
#define NRF_CE		PIN_F5
#define NRF_SCK		PIN_C3
#define NRF_DI		PIN_C4
#define NRF_DO		PIN_C5
#define NRF_IRQ		PIN_B1

#define VBAT_SEN_EN_PIN	PIN_G0
#define VBAT_SEN_PIN	PIN_A1
#define VBUS_SEN_PIN	PIN_A0


#define NRF_PACKET_SIZE	7
BYTE gNRFPacket[NRF_PACKET_SIZE + 1];

#define MAIN_SW		PIN_B0


#define LED_GREEN_ON	output_low(LED_GRN)
#define LED_GREEN_OFF	output_high(LED_GRN)

#define LED_RED_ON	output_low(LED_RED)
#define LED_RED_OFF	output_high(LED_RED)

#define NRF_Enable(a)	a?output_high(NRF_CE):output_low(NRF_CE)
#define NRF_Select(a)	a?output_low(NRF_CSN):output_high(NRF_CSN)

#define USB_PLUGGED()	(input(PIN_A0)==1)

#define VBAT_SEN_ON()	(output_low(VBAT_SEN_EN_PIN))
#define VBAT_SEN_OFF()	(input(VBAT_SEN_EN_PIN))


int g_bSleep = 0;
int g_bSendEnabled = 0;
int g_nTxFailedCount = 0;


// external interrupt when button pushed and released
#INT_EXT HIGH
void ext_isr() 
{
    g_bSendEnabled =! g_bSendEnabled;       // toggle sleep flag
    ext_int_edge(H_TO_L);   // change so interrupts on press

    g_nTxFailedCount = 0;
    
    delay_ms(10);	//remove debounce
}

//Timers
int n1sTick=0;
int nDataInTick=0;

#int_RDA HIGH
void RDA_isr()
{
	int ch;

	ch = getc();

	//usb_cdc_putc_fast(ch);
	usb_cdc_putc(ch);
	
	nDataInTick = 10;
}


//10ms interrupt
#int_TIMER0
void TIMER0_isr() 
{
	set_timer0(5560);   //10ms

	//Turn on/off LED when data comes in
	if(nDataInTick)
		nDataInTick--;
/*
	if(++n1sTick > 100)	//Get 1sec timer
	{
		n1sTick = 0;
	}
*/
}

//1s interrupt
#int_TIMER1
void  TIMER1_isr(void)
 {
	set_timer1(32768);
	n1sTick = 0;
 } 
    

#BYTE OSCTUNE=0xF9B
#BIT PLLEN=OSCTUNE.6

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
	
	for(i=0; i<7; i++)
	{
		usb_cdc_putc(_spi_read());
	}
	
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
		printf(usb_cdc_putc, "REG 0x%02X= 0x%02X\r\n", i, ch);
		delay_ms(10);
		//usb_cdc_flush_out_buffer();
	}
}	

#byte INTCON2=0xFF1
#bit RBPU=INTCON2.7

void main() 
{
	char ch;
	int i;
	int nForceRx=0;
	int nMode = 1;	//default TX mode
	int nPacketCount = 0;
	int nADCCount = 0;
	int nVBAT=0;
	int bBattLow = FALSE;
	
	g_bSleep = FALSE;
	
	PLLEN=1; //Enable 48MHz PLL
	RBPU = 0;	//Enabled port B pull up
	
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
//	set_timer0(5560);    //10ms interrupt
//	enable_interrupts(INT_TIMER0);
	
	//Setup Timer1 (32.768khz RTCC)
   setup_timer_1(T1_EXTERNAL|T1_DIV_BY_1|T1_CLK_OUT);
	enable_interrupts(INT_TIMER1);
	
	//Enable switch button interrupt
	ext_int_edge(H_TO_L);
	enable_interrupts(INT_EXT);
	
	enable_interrupts(GLOBAL);

	usb_init();

	delay_ms(100);
	
	if(nMode == 1)
		NRF_config_tx();
	else
		NRF_config_rx();
	
	
	i=0;
	while(1)
	{
		
		//Timer1 interrupt (1s each) or INT_RB interrupt will wake up the sleep
		if(!USB_PLUGGED())
			sleep();
		
		if(nMode == 1)
			NRF_config_tx();
		else
			NRF_config_rx();

/*
		if(g_bSendEnabled) 	     
   			enable_interrupts(INT_TIMER1);
		else 	     
   			disable_interrupts(INT_TIMER1);
*/   		
		//wait for char from usb port
		if(usb_cdc_kbhit())
		{
			ch = usb_cdc_getc();
			if(ch == 'r')
			{
				NRF_config_rx();
				printf(usb_cdc_putc,"Enter RX mode\r\n");
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
				printf(usb_cdc_putc,"GPSLDR Debugger 1.1\r\n");
			}	

			nDataInTick = 10;
		}
		
		//Do TX/RX	
		if(nMode == 1)	//Transmit mode
		{
			if(g_bSendEnabled && n1sTick == 0)	//1 second
			{
				n1sTick = 1;
				LED_GREEN_ON;
				
				if(nADCCount++ > 10)	//Check battery voltage very 10s
				{
					nADCCount=0;
					VBAT_SEN_ON();
					setup_adc_ports( sAN1 );
					setup_adc(ADC_CLOCK_INTERNAL );
					set_adc_channel( 1 );
					nVBAT = read_adc();
					if( nVBAT < 148) //voltage < 3.5V (n = v*255/6)
					{
						bBattLow = TRUE;
					}	
					else 
						bBattLow = FALSE;
					if(usb_cdc_connected())
						printf(usb_cdc_putc, "VBAT=%u\r\n", nVBAT);
					setup_adc( ADC_OFF );
					VBAT_SEN_OFF();
				}	
				if(bBattLow)
					LED_RED_ON;
				sprintf(gNRFPacket, "%07u", nPacketCount++);
				if(usb_cdc_connected())
					printf(usb_cdc_putc, "Sending %s...", gNRFPacket);
				NRF_transmit_data(gNRFPacket, NRF_PACKET_SIZE);
				
				{
					if(NRF_ReadRegister(0x17) & 0x10)	//Get send buffer status
					{
						if(usb_cdc_connected())
							printf(usb_cdc_putc, "OK\r\n");
						g_nTxFailedCount = 0;	//Reset failure counter
					}	
					else
					{
						if(usb_cdc_connected())	
							printf(usb_cdc_putc, "Failed\r\n");
						if(g_nTxFailedCount++ > 30)	//No ACK from CARGPS for 30sec
						{
							g_bSendEnabled = FALSE;	//Stop sending
						}	
					}	
				}
				
				delay_ms(50);
				LED_GREEN_OFF;	
				LED_RED_OFF;
			}	
		}
		else
		{

			if(input(NRF_IRQ) == 0 || nForceRx) //IRQ active low
			{
				nForceRx = 0;
				//	delay_ms(200);
				if(usb_cdc_connected())
					printf(usb_cdc_putc, "\r\n%03u Recv:", i++);
				nDataInTick = 20;
				NRF_reset_rx();
				delay_ms(10);
			}	
		}
	};

}


