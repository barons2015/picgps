
#include <18F67J50.h>
#device adc=10
#DEVICE HIGH_INTS=TRUE

#FUSES NOWDT                    //No Watch Dog Timer
#FUSES PLL1	                //4MHz input
#FUSES NOCPUDIV                 //No CPU system clock divide
#FUSES NODEBUG                  //No Debug mode for ICD
#FUSES NOPROTECT                //Code not protected from reading
#FUSES H4_SW			//HS-PLL, USB-HS-PLL
#FUSES NOIESO

#use delay(clock=48000000)  
#use rs232(baud=38400,parity=N,xmit=PIN_C6,rcv=PIN_C7,bits=8)
#rom int 0xf000ff={1}

// Includes all USB code and interrupts, as well as the CDC API
#include "usb_cdc.h"
#include <stdlib.h>

#define LED_GRN		PIN_E0	//output low to turn of green LED
#define LED_GREEN_ON	output_low(LED_GRN)
#define LED_GREEN_OFF	output_high(LED_GRN)
//Timers
int n10msTick=0;
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

	if(++n10msTick > 100)	//Get 1sec timer
	{
		n10msTick = 0;
	}

}

#BYTE OSCTUNE=0xF9B
#BIT PLLEN=OSCTUNE.6

void main() 
{
	char ch;
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

	enable_interrupts(INT_RDA);
	set_timer0(5560);    //10ms interrupt
	enable_interrupts(INT_TIMER0);
	enable_interrupts(GLOBAL);


	usb_init();

	//Waiting for port connected
	while(!usb_cdc_connected()) 
	{
		LED_GREEN_ON;
//		delay_ms(500);
//		LED_GREEN_OFF;
//		delay_ms(500);
	};

	printf(usb_cdc_putc,"GPCTS Debugger\r\n");

	while(1)
	{
		if(nDataInTick)
		{
			LED_GREEN_OFF;
		}
		else
		{
			LED_GREEN_ON;
		}		
		//wait for char from usb port
		if(usb_cdc_kbhit())
		{
			ch = usb_cdc_getc();	
			putc(ch);
			nDataInTick = 10;
		}	

	};

}


