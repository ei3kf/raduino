#include <LiquidCrystal.h>

// Uncomment if you want the inverted display
//#define UPSIDE_DOWN 


void setupUpsideDown(LiquidCrystal * lcd);
void printFrequency(char * buffer, unsigned long f);
void printSignal(char * buffer, int s);
