#include <18F67J50.h>
#device adc=10
#DEVICE HIGH_INTS=TRUE

#FUSES NOWDT                    //No Watch Dog Timer
#FUSES PLL_DIV1                   //4MHz input
#FUSES CPUDIV1                    //No CPU system clock divide
#FUSES NODEBUG                  //No Debug mode for ICD
#FUSES NOPROTECT                //Code not protected from reading
#FUSES NOEXTCPU               //Disable extended CPU
#FUSES HSPLLUSBHSPLL

#byte INTCON2=0xFF1
#byte RCSTA1=0xFAC
#byte RCSTA2=0xF9C

#bit RBPU=INTCON2.7
#bit SPEN1=RCSTA1.7
#bit SPEN2=RCSTA2.7

#use delay(clock=48000000)
#use rs232(baud=38400,parity=N,xmit=PIN_C6,rcv=PIN_C7,bits=8, ERRORS, stream=GPS)
#use rs232(baud=115200,parity=N,xmit=PIN_G1,rcv=PIN_G2,bits=8, stream=BT_232)
//#use rs232(baud=115200,parity=N,xmit=PIN_F3,rcv=PIN_F4,bits=8, stream=DEBUG, FORCE_SW )

//#use rs232(baud=38400,parity=N,xmit=PIN_C6,rcv=PIN_C7,bits=8)
//#use rs232(baud=38400,parity=N,xmit=PIN_G1,rcv=PIN_G2,bits=8, stream=debug)
//#use i2c(master, sda=PIN_D6, scl=PIN_D5)

#use i2c(Master,FAST,sda=PIN_D5,scl=PIN_D6)

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
