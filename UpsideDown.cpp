#include "Arduino.h"
#include "UpsideDown.h"

extern long frequency;

#ifdef UPSIDE_DOWN

char number_chars[10] = { '0', (char) 0x01, (char) 0x02, (char) 0x03, (char) 0x04, (char) 0x05, '9', (char) 0x07, '8', '6' };

byte one[8] = {
  B01110,
  B00100,
  B00100,
  B00100,
  B00100,
  B00110,
  B00100,
  B00000
};

byte two[8] = {
  B11111,
  B00010,
  B00100,
  B01000,
  B10000,
  B10001,
  B01110,
  B00000
};

byte three[8] = {
  B01110,
  B10001,
  B10000,
  B01000,
  B00100,
  B01000,
  B11111,
  B00000
};

byte four[8] = {
  B01000,
  B01000,
  B11111,
  B01001,
  B01010,
  B01100,
  B01000,
  B00000
};

byte five[8] = {
  B01110,
  B10001,
  B10000,
  B10000,
  B01111,
  B00001,
  B11111,
  B00000
};

byte seven[8] = {
  B00100,
  B00100,
  B00100,
  B01000,
  B01000,
  B10000,
  B11111,
  B00000
};

#endif

byte bar[8] = {
  B00000,
  B11011,
  B11011,
  B11011,
  B11011,
  B11011,
  B00000,
  B00000
};

byte half_bar[8] = {
#ifdef UPSIDE_DOWN
  B00000,
  B00011,
  B00011,
  B00011,
  B00011,
  B00011,
  B00000,
  B00000
#else
  B00000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B00000,
  B00000
#endif
};



void setupUpsideDown(LiquidCrystal * lcd) 
{
  cli();
  lcd->noDisplay();
  delay(25);
  lcd->createChar(0, bar);
  delay(25);
#ifdef UPSIDE_DOWN
  // We'll reuse zero as is
  lcd->createChar(1, one);
  delay(25);
  lcd->createChar(2, two);
  delay(25);
  lcd->createChar(3, three);
  delay(25);
  lcd->createChar(4, four);
  delay(25);
  lcd->createChar(5, five);
  delay(25);
  lcd->createChar(6, half_bar);
  delay(25);
  // We can use an inverted 9 as a 6
  lcd->createChar(7, seven);
  delay(25);
  // 8 can be used as is
  // We can use an inverted 6 as a 9

#else
  lcd->createChar(6, half_bar);
  delay(25);
#endif
  lcd->display();
  sei();
}




void printFrequency(char * buffer, unsigned long f)
{
   int i;
   char b[10];
   sprintf(b, "%7ld", frequency); 
   
#ifdef UPSIDE_DOWN
   char br[9];
   for (i=0; i<7; i++)
   {
       br[6-i] = number_chars[b[i]-48];
   }
   br[7] = 0;
   sprintf(buffer, "%.2s:%.3s%c%.1s", br+1, br+3, (char) 0b10100101,  br+6);
#else
   sprintf(buffer, "%.1s%c%.3s:%.3s", b, (char) 0b10100101, b+1, b+4);
#endif
}


void printSignal(char * buffer, int s)
{   
#ifdef UPSIDE_DOWN
   if (s < 10) sprintf(buffer, "%cS ", number_chars[s]);
   else if (s == 10) sprintf(buffer, "0%c+", number_chars[1]);
   else sprintf(buffer, "0%c+", number_chars[2]);
#else
   if (s < 10) sprintf(buffer, " S%d", s);
   else sprintf(buffer, "+%d", s);
#endif
}
