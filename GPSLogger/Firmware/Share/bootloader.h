///////////////////////////////////////////////////////////////////////////
////                       BOOTLOADER.H                                ////
////                                                                   ////
////  This include file must be included by any application loaded     ////
////  by the example bootloader (ex_bootloader.c).                     ////
////                                                                   ////
////  The directives in this file relocate the reset and interrupt     ////
////  vectors as well as reserving space for the bootloader.           ////
////                                                                   ////
////  LOADER_END and LOADER_SIZE may need to be adjusted for a         ////
////  specific chip and bootloader.                                    ////
///////////////////////////////////////////////////////////////////////////
////        (C) Copyright 1996,2004 Custom Computer Services           ////
//// This source code may only be used by licensed users of the CCS    ////
//// C compiler.  This source code may only be distributed to other    ////
//// licensed users of the CCS C compiler.  No other use,              ////
//// reproduction or distribution is permitted without written         ////
//// permission.  Derivative programs created using this software      ////
//// in object code form are not restricted in any way.                ////
///////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////// 
// Some defines for my bootloader. 
// 
// Include this file from the bootloader with MY_BOOTLOADER defined. 
// Include this file from the applicatio with MY_BOOTLOADER not defined. 
//////////////////////////////////////////////////////////////////////////////// 

#define RESET_VECTOR            0x0000      // Defined by hardware 
#define HIGH_INT_VECTOR         0x0008      // Defined by hardware 
#define NORMAL_INT_VECTOR       0x0018      // Defined by hardware 
#define INTERRUPT_REMAP_END     HIGH_INT_VECTOR + 0x1B // End of the remap code 

#define LOADER_START            INTERRUPT_REMAP_END+1 + 2
#define LOADER_END              0x04FFF        // Defined by size of the bootloader code and/or protection fuses 
#define LOADER_RESET            RESET_VECTOR 
#define LOADER_HIGH_INT         LOADER_START 
#define LOADER_NORMAL_INT       LOADER_HIGH_INT + 0x10 

#define APPLICATION_START       LOADER_END + 2 
#define APPLICATION_RESET       APPLICATION_START + RESET_VECTOR 
#define APPLICATION_HIGH_INT    APPLICATION_START + HIGH_INT_VECTOR 
#define APPLICATION_NORMAL_INT  APPLICATION_START + NORMAL_INT_VECTOR 

#ifdef _BOOTLOADER 
  #build(reset=LOADER_RESET, interrupt=LOADER_HIGH_INT)           // Move the reset and interrupt vectors 
#else 
  #build(reset=APPLICATION_RESET, interrupt=APPLICATION_HIGH_INT) // Move the reset and interrupt vectors 
  #org 0, LOADER_END {}   // Reserve the bootloader memory area 
#endif 

// A global flag indicating the bootloader is active or not. 
// This flag must be present in both the bootloader and application at the same 
// address, that's why we place it here in the bootloader.h with a #locate 
// instruction to fixed address 0x10. Address 0x10 was choosen arbitrarily in 
// the start of memory above the CCS scratch register area (0x00 to 0x??). 
int8 BootloaderActive; 
#locate BootloaderActive=10 


/*
#define LOADER_END   0x22FF
#define LOADER_SIZE	0x21FF

#define APP_START	LOADER_END+2
#define APP_RESET	APP_START
#define APP_INT	APP_START+8

#ifndef _bootloader
#build(reset=APP_RESET, interrupt=APP_INT)
#org 0,LOADER_END {}
#else
#org LOADER_END+0x10, 0x7FFF {}
#endif
*/

#define BOOTLOADER_CFG_ADDR	255
