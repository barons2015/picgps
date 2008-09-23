////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*

							CompactFlash MP3 Player with PIC16F877L(PLCC) VS1001K LPH7779-LCD(Nokia 3310)

			Design & programming by Raphael Abrams 2002-2003 --- http://www.walrus.com/~raphael/html/mp3.html --- raphael@walrus.com

					LCD interface by Michel Bavin 2003 --- http://users.skynet.be/bk317494/ ---  bavin@skynet.be


ver 4.0n
December 10, 2003.

*/
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#include <string.h>

#use fast_io(D)

#byte userport	=0xf83	//port d
//#byte rstport = 0xf82   //port c
#byte tris_userport	=0xf95	//tris d
//#byte tris_rstport = 0xf94    //tris c

//#define NOKIA_SPI
// NOKIA_LCD // **************************************************************************
#bit nok_sclk =userport.0	//RD0
#bit nok_sda =userport.1	//RD1
#bit nok_dc =userport.2		//RD2
#bit nok_cs =userport.3		//RD3
#bit nok_res =userport.4    //RD4

#define USERPORT_ON  0xE0	// iiio oooo
#define USERPORT_OFF 0xEF

#define NOKIA_GRAPH
#ifdef NOKIA_GRAPH

BYTE nokia_vram[504]; 	//84*48 pixels
int16 nCharPos=0;

//#locate nokia_vram=0x100

char nokia_getpixel(int x, int y);
void nokia_putpixel(int x, int y, int nColor);
void nokia_refresh(void); 
#endif

//5*7 font
BYTE CONST TABLE5 [240]={
						0x00,0x00,0x00,0x00,0x00,	// 20 space	 		ASCII table for NOKIA LCD: 96 rows * 5 bytes= 480 bytes
						0x00,0x00,0x5f,0x00,0x00,	// 21 !
						0x00,0x07,0x00,0x07,0x00,	// 22 "
						0x14,0x7f,0x14,0x7f,0x14,	// 23 #
						0x24,0x2a,0x7f,0x2a,0x12,	// 24 $
						0x23,0x13,0x08,0x64,0x62,	// 25 %
						0x36,0x49,0x55,0x22,0x50,	// 26 &
						0x00,0x05,0x03,0x00,0x00,	// 27 '
						0x00,0x1c,0x22,0x41,0x00,	// 28 (
						0x00,0x41,0x22,0x1c,0x00,	// 29 )
						0x14,0x08,0x3e,0x08,0x14,	// 2a *
						0x08,0x08,0x3e,0x08,0x08,	// 2b +
						0x00,0x50,0x30,0x00,0x00,	// 2c ,
						0x08,0x08,0x08,0x08,0x08,	// 2d -
						0x00,0x60,0x60,0x00,0x00,	// 2e .
						0x20,0x10,0x08,0x04,0x02,	// 2f /
						0x3e,0x51,0x49,0x45,0x3e,	// 30 0
						0x00,0x42,0x7f,0x40,0x00,	// 31 1
						0x42,0x61,0x51,0x49,0x46,	// 32 2
						0x21,0x41,0x45,0x4b,0x31,	// 33 3
						0x18,0x14,0x12,0x7f,0x10,	// 34 4
						0x27,0x45,0x45,0x45,0x39,	// 35 5
						0x3c,0x4a,0x49,0x49,0x30,	// 36 6
						0x01,0x71,0x09,0x05,0x03,	// 37 7
						0x36,0x49,0x49,0x49,0x36,	// 38 8
						0x06,0x49,0x49,0x29,0x1e,	// 39 9
						0x00,0x36,0x36,0x00,0x00,	// 3a :
						0x00,0x56,0x36,0x00,0x00,	// 3b ;
						0x08,0x14,0x22,0x41,0x00,	// 3c <
						0x14,0x14,0x14,0x14,0x14,	// 3d =
						0x00,0x41,0x22,0x14,0x08,	// 3e >
						0x02,0x01,0x51,0x09,0x06,	// 3f ?
						0x32,0x49,0x79,0x41,0x3e,	// 40 @
						0x7e,0x11,0x11,0x11,0x7e,	// 41 A
						0x7f,0x49,0x49,0x49,0x36,	// 42 B
						0x3e,0x41,0x41,0x41,0x22,	// 43 C
						0x7f,0x41,0x41,0x22,0x1c,	// 44 D
						0x7f,0x49,0x49,0x49,0x41,	// 45 E
						0x7f,0x09,0x09,0x09,0x01,	// 46 F
						0x3e,0x41,0x49,0x49,0x7a,	// 47 G
						0x7f,0x08,0x08,0x08,0x7f,	// 48 H
						0x00,0x41,0x7f,0x41,0x00,	// 49 I
						0x20,0x40,0x41,0x3f,0x01,	// 4a J
						0x7f,0x08,0x14,0x22,0x41,	// 4b K
						0x7f,0x40,0x40,0x40,0x40,	// 4c L
						0x7f,0x02,0x0c,0x02,0x7f,	// 4d M
						0x7f,0x04,0x08,0x10,0x7f,	// 4e N
						0x3e,0x41,0x41,0x41,0x3e};	// 4f O


BYTE CONST TABLE6 [240]={
						0x7f,0x09,0x09,0x09,0x06,	// 50 P
						0x3e,0x41,0x51,0x21,0x5e,	// 51 Q
						0x7f,0x09,0x19,0x29,0x46,	// 52 R
						0x46,0x49,0x49,0x49,0x31,	// 53 S
						0x01,0x01,0x7f,0x01,0x01,	// 54 T
						0x3f,0x40,0x40,0x40,0x3f,	// 55 U
						0x1f,0x20,0x40,0x20,0x1f,	// 56 V
						0x3f,0x40,0x38,0x40,0x3f,	// 57 W
						0x63,0x14,0x08,0x14,0x63,	// 58 X
						0x07,0x08,0x70,0x08,0x07,	// 59 Y
						0x61,0x51,0x49,0x45,0x43,	// 5a Z
						0x00,0x7f,0x41,0x41,0x00,	// 5b [
						0x02,0x04,0x08,0x10,0x20,	// 5c
						0x00,0x41,0x41,0x7f,0x00,	// 5d
						0x04,0x02,0x01,0x02,0x04,	// 5e
						0x40,0x40,0x40,0x40,0x40,	// 5f
						0x00,0x01,0x02,0x04,0x00,	// 60
						0x20,0x54,0x54,0x54,0x78,	// 61 a
						0x7f,0x48,0x44,0x44,0x38,	// 62 b
						0x38,0x44,0x44,0x44,0x20,	// 63 c
						0x38,0x44,0x44,0x48,0x7f,	// 64 d
						0x38,0x54,0x54,0x54,0x18,	// 65 e
						0x08,0x7e,0x09,0x01,0x02,	// 66 f
						0x0c,0x52,0x52,0x52,0x3e,	// 67 g
						0x7f,0x08,0x04,0x04,0x78,	// 68 h
						0x00,0x44,0x7d,0x40,0x00,	// 69 i
						0x20,0x40,0x44,0x3d,0x00,	// 6a j
						0x7f,0x10,0x28,0x44,0x00,	// 6b k
						0x00,0x41,0x7f,0x40,0x00,	// 6c l
						0x7c,0x04,0x18,0x04,0x78,	// 6d m
						0x7c,0x08,0x04,0x04,0x78,	// 6e n
						0x38,0x44,0x44,0x44,0x38,	// 6f o
						0x7c,0x14,0x14,0x14,0x08,	// 70 p
						0x08,0x14,0x14,0x18,0x7c,	// 71 q
						0x7c,0x08,0x04,0x04,0x08,	// 72 r
						0x48,0x54,0x54,0x54,0x20,	// 73 s
						0x04,0x3f,0x44,0x40,0x20,	// 74 t
						0x3c,0x40,0x40,0x20,0x7c,	// 75 u
						0x1c,0x20,0x40,0x20,0x1c,	// 76 v
						0x3c,0x40,0x30,0x40,0x3c,	// 77 w
						0x44,0x28,0x10,0x28,0x44,	// 78 x
						0x0c,0x50,0x50,0x50,0x3c,	// 79 y
						0x44,0x64,0x54,0x4c,0x44,	// 7a z
						0x00,0x08,0x36,0x41,0x00,	// 7b
						0x00,0x00,0x7f,0x00,0x00,	// 7c
						0x00,0x41,0x36,0x08,0x00,	// 7d
						0x10,0x08,0x08,0x10,0x08,	// 7e
						0x78,0x46,0x41,0x46,0x78};	// 7f


//--------------------------------------prototypes
//#SEPARATE
void 	nokia_init(void);
//#SEPARATE
void 	nokia_write_command(char bytefornokia_command);
//#SEPARATE
void 	nokia_write_data(char bytefornokia_data);
//#SEPARATE
void 	nokia_write_dorc(char bytefornokia);
//#SEPARATE
void 	nokia_gotoxy(byte xnokia, byte ynokia);
//#SEPARATE
void 	nokia_printchar(byte cvar);
//#SEPARATE
void 	nokia_clean_ddram(void);

void 	table_to_nokialcd(void);

//--------------------------------------end prototypes
int16 ddram;
int16 charpos;
char char_row,charsel,chardata;
char char_invert;

void TestLCD()
{

	nokia_init();				// nokia 3310 lcd init.	**********************************************************************

	nokia_write_command(0x0d);	// mod control inverse video change

	nokia_gotoxy(0,1);			// nokia 3310 lcd cursor x y position
	printf(nokia_printchar,"Wei Fang");	// ***************************************************************************

	delay_ms(1000);
	nokia_write_command(0x0c);	// mod control normal change

}



/////////////////////////////////////////////////////////////////////////////////////////
//#SEPARATE
void nokia_init(void)
{
#use standard_io(D)
      tris_userport	= USERPORT_ON;
      //tris_rstport = tris_rstport&0xDF;   //xxoxxxxx

      char_invert = 0;


		delay_us(200);

		nok_dc=1;				// bytes are stored in the display data ram, address counter, incremented automatically
		nok_cs=1;				// chip disabled
		delay_us(200);

		nok_res=0;				// reset chip during 250ms
		delay_ms(250);			// works with less.....
		nok_res=1;

		nokia_write_command(0x21);	// set extins extended instruction set
		nokia_write_command(0xbe);	// Vop  v1: 0xc8 (for 3V)// v2: 0xa0 (for 3V) 0xbe	********************************************************************************************************************
		nokia_write_command(0x13);	// bias
		nokia_write_command(0x20);	// horizontal mode from left to right, X axe are incremented automatically , 0x22 for vertical addressing ,back on normal instruction set too
		nokia_write_command(0x09);	// all on

		delay_ms(50);

		nokia_clean_ddram();		// reset DDRAM, otherwise the lcd is blurred with random pixels

		delay_ms(10);

		nokia_write_command(0x08);	// mod control blank change (all off)
		delay_ms(10);

		nokia_write_command(0x0c);	// mod control normal change

      tris_userport=USERPORT_OFF;	// iiii iooi
#use fast_io(D)

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//#SEPARATE
void nokia_clean_ddram(void)
{
#ifdef NOKIA_GRAPH
	for (ddram=0;ddram<504;ddram++){nokia_vram[ddram] = 0x00;}
	
#else
	nokia_gotoxy(0,0);			// 84*6=504		clear LCD
	for (ddram=504;ddram>0;ddram--){nokia_write_data(0x00);}
#endif

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//#SEPARATE
void nokia_write_command(char bytefornokia_command)
{
	tris_userport	=USERPORT_ON;	// iiii oooo

	nok_dc=0;	// byte is a command it is read with the eight SCLK pulse
	nok_cs=0;	// chip enabled
	nokia_write_dorc(bytefornokia_command);
	nok_cs=1;	// chip disabled

	tris_userport	=USERPORT_OFF;	// iiii iooo

}
/////////////////////////////////////////////////////////////////////////////////
//#SEPARATE
void nokia_write_data(char bytefornokia_data)
{
#ifdef NOKIA_GRAPH
   	if(char_invert)
      		bytefornokia_data = ~bytefornokia_data;
      	//if(nCharPos >= 504)
      	//	nCharPos = 0;
	nokia_vram[nCharPos++] = bytefornokia_data;
	
#else
	tris_userport	=USERPORT_ON;	// iooo iooo

	nok_dc=1;
	nok_cs=0;	// chip enabled
   if(char_invert)
      bytefornokia_data = ~bytefornokia_data;
	nokia_write_dorc(bytefornokia_data);
	nok_cs=1;	// chip disabled

	tris_userport	=USERPORT_OFF;	// iiii iooo
#endif

}
//////////////////////////////////////////////////////////////////////////////////
//#SEPARATE
void nokia_write_dorc(char bytefornokia)			// serial write data or command subroutine
{
	signed char caa;
	for (caa=7;caa>=0;caa--)
	{
		nok_sclk=0;
		delay_us(2);
		nok_sda = bit_test(bytefornokia, caa);
		nok_sclk=1;
	}	

}

//////////////////////////////////////////////////////////////////////////////////
//#SEPARATE
void nokia_gotoxy(byte xnokia, byte ynokia)		// Nokia LCD 3310 Position cursor
{
#ifdef NOKIA_GRAPH
	nCharPos = ynokia;
	nCharPos *= 84;
	nCharPos += xnokia;
#else
	nokia_write_command(0x40|(ynokia&0x07));	// Y axe initialisation: 0100 0yyy
	nokia_write_command(0x80|(xnokia&0x7f));	// X axe initialisation: 1xxx xxxx
#endif
}
//////////////////////////////////////////////////////////////////////////////////
//#SEPARATE
void nokia_printchar(byte cvar)					// Write 1 character to LCD
{
	charsel=cvar;
	table_to_nokialcd();
}

//////////////////////////////////////////////////////////////////////////////////

void table_to_nokialcd(void)	// extract ascii from tables & write to LCD
{
	if (charsel<0x20)return;
	if (charsel>0x7f)return;

	for (char_row=0;char_row<5;char_row++)
	{		// 5 bytes

		if (charsel<0x50)
			{charpos=(((charsel&0xff)-0x20)*5);chardata=TABLE5[(charpos+char_row)];}				// use TABLE5
		if (charsel>0x4f){charpos=(((charsel&0xff)-0x50)*5);chardata=TABLE6[(charpos+char_row)];}				// use TABLE6

		nokia_write_data(chardata);		// send data to nokia
		
	}

	nokia_write_data(0x00);		// 	1 byte (always blank)

}
//#SEPARATE
void nokia_power_down(void)
{
   nokia_clean_ddram();
   nokia_write_command(0x25);
}
//#SEPARATE
void nokia_power_up()
{
   nokia_write_command(0x21);
}
//#SEPARATE
void nokia_char_invert(int bInvert)
{
     char_invert = bInvert;
}

#ifdef NOKIA_GRAPH
char nokia_getpixel(int x, int y)
{
	int16 nPos;
	char nPixel;
	nPos = (y/8);
	nPos = nPos*84+x;

	nPixel = nokia_vram[nPos];

	nPixel = bit_test(nPixel, (y%8));
	return nPixel;
}

void nokia_putpixel(int x, int y, int nColor)
{
	int16 nPos;
	char nPixel;

	
	nPos = y;
	nPos >>= 3;
	nPos = nPos*84+x;

	nPixel = nokia_vram[nPos];

	if(nColor)
		bit_set(nPixel, (y%8));
	else
		bit_clear(nPixel, (y%8));

	nokia_vram[nPos] = nPixel;

}

void nokia_drawtextxy(int x, int y, char *pText, int nColor)
{
	
}

void nokia_line(int x1, int y1, int x2, int y2, int color)
{
	signed int16 deltax, deltay, numpixels;
	signed int16 i,
    d, dinc1, dinc2,
    x, xinc1, xinc2,
    y, yinc1, yinc2;


  	deltax = abs(x2 - x1);
  	deltay = abs(y2 - y1);

  	//Initialize all vars based on which is the independent variable }
  	if( deltax >= deltay )
  	{

      //{ x is independent variable }
      numpixels = deltax + 1;
      d = (2 * deltay) - deltax;
      dinc1 = deltay<<1;
      dinc2 = (deltay - deltax) * 2;
      xinc1 = 1;
      xinc2 = 1;
      yinc1 = 0;
      yinc2 = 1;
   	}
  	else
  	{

      //{ y is independent variable }
      numpixels = deltay + 1;
      d = (2 * deltax) - deltay;
      dinc1 = deltax << 1;
      dinc2 = (deltax - deltay) *2;
      xinc1 = 0;
      xinc2 = 1;
      yinc1 = 1;
      yinc2 = 1;
    }

  //{ Make sure x and y move in the right directions }
  if( x1 > x2 )
  {
      xinc1 = -xinc1;
      xinc2 = -xinc2;
   }
  if( y1 > y2 )
    {
      yinc1 = -yinc1;
      yinc2 = -yinc2;
    }

  //{ Start drawing at <x1, y1> }
  x = x1;
  y = y1;

  //{ Draw the pixels }
  for( i = 1; i< numpixels; i++)
   {
      nokia_putpixel(x, y, color);

      if( d < 0 )
       {
          d = d + dinc1;
          x = x + xinc1;
          y = y + yinc1;
       }
      else
        {
          d = d + dinc2;
          x = x + xinc2;
          y = y + yinc2;
        }
    }

}

void nokia_circle(int x, int y, int r, int color)
{

	signed int x1, y1;

	signed int p;

	 x1 = 0; y1 = r; p= 1-r;
	while(x1<=y1)
	{
 		//Make all the 8 parts
 		nokia_putpixel(x+x1,y+y1,color);
		nokia_putpixel(x+x1,y-y1,color);
 		nokia_putpixel(x-x1,y+y1,color);
		nokia_putpixel(x-x1,y-y1,color);
		nokia_putpixel(x+y1,y+x1,color);
		nokia_putpixel(x+y1,y-x1,color);
		nokia_putpixel(x-y1,y+x1,color);
		nokia_putpixel(x-y1,y-x1,color); 
 
 		if(p < 0)
 		{
  			p += 2*x1 + 3;
 		}
 		else
 		{
  			p += 2*(x1-y1)+5;
  			--y1;
 		}
 
 		++x1;
	}
 
}
void nokia_refresh(void)
{
	int16 i;
	
	//Set DRAM address 0
	nokia_write_command(0x40);	// Y axe initialisation: 0100 0yyy
	nokia_write_command(0x80);	// X axe initialisation: 1xxx xxxx
	
	tris_userport	=USERPORT_ON;	// iooo iooo

	nok_dc=1;
	nok_cs=0;	// chip enabled
	
	for (i=0; i<504; i++)
		nokia_write_dorc(nokia_vram[i]);

	nok_cs=1;	// chip disabled
	tris_userport	=USERPORT_OFF;	// iiii iooo
}

#endif
