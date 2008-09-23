#include <18F67J10.h>
#device adc=10

//#define 4XCLOCK

#FUSES NOWDT                 	//No Watch Dog Timer
#FUSES WDT128                	//Watch Dog Timer uses 1:128 Postscale

#ifdef 4XCLOCK
#FUSES H4_SW  
#else
#FUSES HS                    	//High speed Osc (> 4mhz)
#endif

#FUSES NODEBUG               	//No Debug mode for ICD
#FUSES NOXINST               	//Extended set extension and Indexed Addressing mode disabled (Legacy mode)
#FUSES STVREN                	//Stack full/underflow will cause reset
#FUSES NOPROTECT             	//Code not protected from reading
#FUSES FCMEN                 	//Fail-safe clock monitor enabled
#FUSES NOIESO                	//Internal External Switch Over mode disabled
//#FUSES PRIMARY               	//Primary clock is system clock when scs=00

#ifdef 4XCLOCK
#use delay(clock=32000000)
#else
#use delay(clock=8000000)
#endif

#use rs232(baud=38400,parity=N,xmit=PIN_C6,rcv=PIN_C7,bits=8)
#use rs232(baud=38400,parity=N,xmit=PIN_G1,rcv=PIN_G2,bits=8, stream=debug)
#use i2c(master, sda=PIN_D6, scl=PIN_D5)

#byte OSCTUNE=0xF9B
#byte INTCON2=0xFF1
#bit RBPU=INTCON2.7

//#define _DEBUG
#ifdef _DEBUG
#define TRACE0(format)	fprintf(debug, format)
#define TRACE1(format, arg1)	fprintf(debug, format, arg1)
#define TRACE2(format, arg1, arg2)	fprintf(debug, format, arg1, arg2)
#define TRACE3(format, arg1, arg2, arg3)	fprintf(debug, format, arg1, arg2, arg3)
#define TRACE4(format, arg1, arg2, arg3, arg4)	fprintf(debug, format, arg1, arg2, arg3, arg4)
#else 
#define TRACE0(format)	
#define TRACE1(format, arg1)
#define TRACE2(format, arg1, arg2)
#define TRACE3(format, arg1, arg2, arg3)
#define TRACE4(format, arg1, arg2, arg3, arg4)
#endif
