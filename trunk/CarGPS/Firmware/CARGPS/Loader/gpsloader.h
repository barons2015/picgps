#include <18F67J50.h>
#device adc=10

#DEVICE HIGH_INTS=TRUE

#FUSES NOWDT                 //No Watch Dog Timer
#FUSES PLL1	                //4MHz input
#FUSES NOCPUDIV             //No CPU system clock divide
#FUSES NODEBUG              //No Debug mode for ICD
#FUSES NOPROTECT            //Code not protected from reading
#FUSES H4_SW				//HS-PLL, USB-HS-PLL
#FUSES NOIESO

#byte INTCON2=0xFF1
#byte RCSTA1=0xFAC
#byte RCSTA2=0xF9C

#use delay(clock=48000000)

//#use rs232(baud=38400,parity=N,xmit=PIN_C6,rcv=PIN_C7,bits=8)
//#use rs232(baud=38400,parity=N,xmit=PIN_G1,rcv=PIN_G2,bits=8, stream=debug)
//#use i2c(master, sda=PIN_D6, scl=PIN_D5)

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
