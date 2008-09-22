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

#byte INTCON2=0xFF1
#byte RCSTA1=0xFAC
#byte RCSTA2=0xF9C
#byte RCREG1=0xFAF
#byte RCREG2=0xFAA

#bit RBPU=INTCON2.7
#bit SPEN1=RCSTA1.7
#bit SPEN2=RCSTA2.7
#bit CREN1=RCSTA1.4
#bit CREN2=RCSTA2.4
#bit FERR1=RCSTA1.2	//1 = Framing error (can be updated by reading RCREGx register and receiving next valid byte)
#bit FERR2=RCSTA2.2
#bit OERR1=RCSTA1.1	//1 = Overrun error (can be cleared by clearing bit CREN)
#bit OERR2=RCSTA2.1	//1 = Overrun error (can be cleared by clearing bit CREN)


#use delay(clock=48000000)

//GPS sentence input
//
//debug ouput
//#use rs232(baud=115200,parity=N,xmit=PIN_G1,rcv=PIN_G2,bits=8, stream=DEBUG)
#use rs232(baud=38400,parity=N,xmit=PIN_C6,rcv=PIN_C7,bits=8, stream=MDLCAR)
#define GPS_STREAM MDLCAR

//I2C definition used in 2402.c
#define EEPROM_SDA  PIN_A4
#define EEPROM_SCL  PIN_A5

#use i2c(Master,sda=PIN_A4,scl=PIN_A5)

//#define _DEBUG
#ifdef _DEBUG
#define TRACE0(format)   fprintf(debug, format)
#define TRACE1(format, arg1)   fprintf(debug, format, arg1)
#define TRACE2(format, arg1, arg2)   fprintf(debug, format, arg1, arg2)
#define TRACE3(format, arg1, arg2, arg3)   fprintf(debug, format, arg1, arg2, arg3)
#define TRACE4(format, arg1, arg2, arg3, arg4)   fprintf(debug, format, arg1, arg2, arg3, arg4)
#else 
#define TRACE0(format)   
#define TRACE1(format, arg1)
#define TRACE2(format, arg1, arg2)
#define TRACE3(format, arg1, arg2, arg3)
#define TRACE4(format, arg1, arg2, arg3, arg4)
#endif
