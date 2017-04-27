/**
   Raduino_v1.07 for BITX40 - Allard Munters PE1NWL (pe1nwl@gooddx.net)

   This source file is under General Public License version 3.

   Most source code are meant to be understood by the compilers and the computers.
   Code that has to be hackable needs to be well understood and properly documented.
   Donald Knuth coined the term Literate Programming to indicate code that is written be
   easily read and understood.

   The Raduino is a small board that includes the Arduin Nano, a 16x2 LCD display and
   an Si5351a frequency synthesizer. This board is manufactured by Paradigm Ecomm Pvt Ltd

   To learn more about Arduino you may visit www.arduino.cc.

   The Arduino works by firt executing the code in a function called setup() and then it
   repeatedly keeps calling loop() forever. All the initialization code is kept in setup()
   and code to continuously sense the tuning knob, the function button, transmit/receive,
   etc is all in the loop() function. If you wish to study the code top down, then scroll
   to the bottom of this file and read your way up.

   Below are the libraries to be included for building the Raduino

   The EEPROM library is used to store settings like the frequency memory, caliberation data,
   callsign etc .
*/

#include <EEPROM.h>

/**
    The main chip which generates upto three oscillators of various frequencies in the
    Raduino is the Si5351a. To learn more about Si5351a you can download the datasheet
    from www.silabs.com although, strictly speaking it is not a requirment to understand this code.
    Instead, you can look up the Si5351 library written by Jason Mildrum, NT7S. You can download and
    install it from https://github.com/etherkit/Si5351Arduino to complile this file.
    NOTE: This sketch is based on version V2 of the Si5351 library. It will not compile with V1!

    The Wire.h library is used to talk to the Si5351 and we also declare an instance of
    Si5351 object to control the clocks.
*/
#include <Wire.h>
#include <si5351.h> // https://github.com/etherkit/Si5351Arduino/releases/tag/v2.0.1
Si5351 si5351;
#include "UpsideDown.h"

/**
   The Raduino board is the size of a standard 16x2 LCD panel. It has three connectors:

   First, is an 8 pin connector that provides +5v, GND and six analog input pins that can also be
   configured to be used as digital input or output pins. These are referred to as A0,A1,A2,
   A3,A6 and A7 pins. The A4 and A5 pins are missing from this connector as they are used to
   talk to the Si5351 over I2C protocol.

    A0     A1   A2   A3    +5v    GND   A6   A7
   BLACK BROWN RED ORANGE YELLOW GREEN BLUE VIOLET

   Second is a 16 pin LCD connector. This connector is meant specifically for the standard 16x2
   LCD display in 4 bit mode. The 4 bit mode requires 4 data lines and two control lines to work:
   Lines used are : RESET, ENABLE, D4, D5, D6, D7
   We include the library and declare the configuration of the LCD panel too
*/

#include <LiquidCrystal.h>
LiquidCrystal lcd(8, 9, 10, 11, 12, 13);

/**
   The Arduino, unlike C/C++ on a regular computer with gigabytes of RAM, has very little memory.
   We have to be very careful with variables that are declared inside the functions as they are
   created in a memory region called the stack. The stack has just a few bytes of space on the Arduino
   if you declare large strings inside functions, they can easily exceed the capacity of the stack
   and mess up your programs.
   We circumvent this by declaring a few global buffers as  kitchen counters where we can
   slice and dice our strings. These strings are mostly used to control the display or handle
   the input and output from the USB port. We must keep a count of the bytes used while reading
   the serial port as we can easily run out of buffer space. This is done in the serial_in_count variable.
*/

char c[30], b[30], printBuff[32];
int count = 0;
int sm;  // Signal meter
int s_level;
volatile int peak_sm = 0;
int last_sm;

/**
   We need to carefully pick assignment of pin for various purposes.
   There are two sets of completely programmable pins on the Raduino.
   First, on the top of the board, in line with the LCD connector is an 8-pin connector
   that is largely meant for analog inputs and front-panel control. It has a regulated 5v output,
   ground and six pins. Each of these six pins can be individually programmed
   either as an analog input, a digital input or a digital output.
   The pins are assigned as follows:
          A0,   A1,  A2,  A3,   +5v,   GND,  A6,  A7
        BLACK BROWN RED ORANGE YELLW  GREEN  BLUEVIOLET
        (while holding the board up so that back of the board faces you)

   Though, this can be assigned anyway, for this application of the Arduino, we will make the following
   assignment
   A2 will connect to the PTT line, which is the usually a part of the mic connector
   A3 is connected to a push button that can momentarily ground this line. This will be used to switch between different modes, etc.
   A6 is to implement a keyer, it is reserved and not yet implemented
   A7 is connected to a center pin of good quality 100K or 10K linear potentiometer with the two other ends connected to
   ground and +5v lines available on the connector. This implments the tuning mechanism
*/

#define SMETER (A0)
#define ANALOG_KEYER (A1)
#define CAL_BUTTON (A2)
#define FBUTTON (A3)
#define PTT (A6)
#define ANALOG_TUNING (A7)

/**
    The second set of 16 pins on the bottom connector are have the three clock outputs and the digital lines to control the rig.
    This assignment is as follows :
      Pin   1   2    3    4    5    6    7    8    9    10   11   12   13   14   15   16
           +5V +5V CLK0  GND  GND  CLK1 GND  GND  CLK2  GND  D2   D3   D4   D5   D6   D7
    These too are flexible with what you may do with them, for the Raduino, we use them to :
    - TX_RX line : Switches between Transmit and Receive after sensing the PTT or the morse keyer
    - CW_KEY line : turns on the carrier for CW
    These are not used at the moment.
*/

#define TX_RX (7)
#define CW_TONE (6)
#define CW_KEY (5)
#define TX_LPF_SEL (4)

/**
    The raduino has a number of timing parameters, all specified in milliseconds
    CW_TIMEOUT : how many milliseconds between consecutive keyup and keydowns before switching back to receive?
    The next set of three parameters determine what is a tap, a double tap and a hold time for the funciton button
    TAP_DOWN_MILLIS : upper limit of how long a tap can be to be considered as a button_tap
    TAP_UP_MILLIS : upper limit of how long a gap can be between two taps of a button_double_tap
    TAP_HOLD_MILIS : many milliseconds of the buttonb being down before considering it to be a button_hold
*/

#define TAP_UP_MILLIS (500)
#define TAP_DOWN_MILLIS (600)
#define TAP_HOLD_MILLIS (2000)
#define CW_TIMEOUT (600l) // in milliseconds, this is the parameter that determines how long the tx will hold between cw key downs

/**
   The Raduino supports two VFOs : A and B and receiver incremental tuning (RIT).
   we define a variables to hold the frequency of the two VFOs, RITs
   the rit offset as well as status of the RIT

   To use this facility, wire up push button on A3 line of the control connector
*/
#define VFO_A 0
#define VFO_B 1
char ritOn = 0;
char vfoActive = VFO_A;
unsigned long vfoA = 7100000L, vfoB = 7100000L, sideTone = 800;

/**
   In USB mode we need to apply some frequency offset, so that zerobeat in USB is same as in LSB
*/
int USB_OFFSET; // USB offset in Hz

/**
   We can set the VFO drive to a certain level (2,4,6,8 mA)
*/
int LSBdrive;
int USBdrive;

/**
   Raduino needs to keep track of current state of the transceiver. These are a few variables that do it
*/

char inTx = 0;
char keyDown = 0;
char isUSB = 0;
unsigned long cwTimeout = 0;
unsigned char txFilter = 0;

/** Tuning Mechanism of the Raduino
    We use a linear pot that has two ends connected to +5 and the ground. the middle wiper
    is connected to ANALOG_TUNNING pin. Depending upon the position of the wiper, the
    reading can be anywhere from 0 to 1023.
    If we want to use a multi-turn potentiometer with a tuning range of 500 kHz and a step
    size of 50 Hz we need 10,000 steps which is about 10x more than the steps that the ADC
    provides. Arduino's ADC has 10 bits which results in 1024 steps only.
    We can artificially expand the number of steps by a factor 10 by oversampling 100 times.
    As a result we get 10240 steps.
    The tuning control works in steps of 50Hz each for every increment between 10 and 10230.
    Hence the turning the pot fully from one end to the other will cover 50 x 10220 = 511 KHz.
    But if we use the standard 1-turn pot, then a tuning range of 500 kHz would be too much.
    (tuning would become very touchy). In the next few lines we can limit the tuning range
    depending on the potentiometer used and the band section of interest. Tuning beyond the
    limits is still possible by the 'scan-up' and 'scan-down' mode at the end of the pot.
    At the two ends, that is, the tuning starts stepping up or down in 10 KHz steps.
    To stop the scanning the pot is moved back from the edge.F
*/

int TUNING_RANGE; // tuning range (in kHz) of the tuning pot
unsigned long baseTune; // frequency (Hz) when tuning pot is at minimum position

unsigned long bfo_freq = 11998000L;
int old_knob = 0;

#define CW_OFFSET (800l)

#define LOWEST_FREQ  (6995000L) // absolute minimum frequency (Hz)
#define HIGHEST_FREQ (7500000L) //  absolute maximum frequency (Hz)

long frequency, stepSize = 100000L;

/**
   The raduino has multiple modes:
*/
#define MODE_NORMAL (0)  // normal operation
#define MODE_CALIBRATE (1) // calibrate VFO frequency in LSB mode
#define MODE_USBOFFSET (2) // adjust frequency offset in USB mode
#define MODE_DRIVELEVEL (3) // set VFO drive level
#define MODE_TUNERANGE (4) // set upper and lower limits of tuning pot
char mode = MODE_NORMAL;

/**
   Display Routines
   These two display routines print a line of characters to the upper and lower lines of the 16x2 display
*/

void printLine1(char *c) {
  if (strcmp(c, printBuff)) {
    lcd.setCursor(0, 0);
    lcd.print(c);
    strcpy(printBuff, c);
    count++;
  }
}

void printLine2(char *c) {
  lcd.setCursor(0, 1);
  lcd.print(c);
}

// returns a logarithmic scale from the raw signal voltage on the scale 0 - 19
int logscale(int s)
{
   if (s <= 5) return s;     
   if (s <= 12) return 6;   
   if (s <= 17) return 7;
   if (s <= 23) return 8;
   if (s <= 30) return 9;
   if (s <= 37) return 10;
   if (s <= 46) return 11;
   if (s <= 55) return 12;
   if (s <= 65) return 13;
   if (s <= 77) return 14;
   if (s <= 89) return 15;    
   if (s <= 102) return 16;
   if (s <= 116) return 17; 
   if (s <= 132) return 18;  
   return 19;
}

void updateFrequency()
{
    printFrequency(c, frequency);     
    lcd.setCursor(8,0);
    lcd.print(c);
    lcd.setCursor(0,0);
    lcd.print("[EI3KF]");
    
}

void updateSMeter()
{
    // Signal meter
    
    int i;
    int smi = sm / 2;
    int smp = peak_sm / 2;  
    printSignal(c, s_level);
    
#ifdef UPSIDE_DOWN
    lcd.setCursor(0,0);
    lcd.print("[");
    for (i=9; i>=smi+1; i--) 
    {
       if (i == smp) lcd.print("|");
       else lcd.print(" ");
    }
    if (sm%2==1) lcd.print((char) 0x6);
    else lcd.print(" ");
    for (i=0; i<smi; i++) lcd.print((char) 0x0);
    lcd.print("]");
    lcd.print(" ");
    lcd.print(c);
#else
    lcd.setCursor(0,1);
    lcd.print(c);
    lcd.print(" ");
    lcd.print("[");
    for (i=0; i<smi; i++) lcd.print((char) 0x0);
    if (sm%2==1) lcd.print((char) 0x6);
    else lcd.print(" ");
    for (i=smi+1; i<10; i++) 
    {
       if (i == smp) lcd.print("|");
       else lcd.print(" ");
    }
    lcd.print("]");
#endif
}

/**
   Building upon the previous  two functions,
   update Display paints the first line as per current state of the radio

   At present, we are not using the second line. YOu could add a CW decoder or SWR/Signal strength
   indicator
*/

void updateDisplay()
{
   updateFrequency();
   updateSMeter();
}


bool calbutton_prev = HIGH;
long cal;

/**
   To use calibration sets the accurate readout of the tuned frequency
   To calibrate, follow these steps:
   1. Tune in a LSB signal that is at a known frequency.
   2. Now, set the display to show the correct frequency,
      the signal will no longer be tuned up properly
   3. Use the "LSB calibrate" option in the "Settings" menu (or Press the CAL_BUTTON line to the ground (pin A2 - red wire))
   4. tune in the signal until it sounds proper.
   5. Press the FButton (or Release CAL_BUTTON)
   In step 4, when we say 'sounds proper' then, for a CW signal/carrier it means zero-beat
   and for LSB it is the most natural sounding setting.

   Calibration is a multiplying factor that is applied to the VFO frequency by the Si5351 library.
   We store it in the EEPROM's first four bytes and read it in setup() when the Radiuno is powered up
*/

void calibrate() {
  //static long cal;
  long cal_old;
  long vfo;
  static long shift;

  if (mode != MODE_CALIBRATE) {
    setLSB();

    //fetch the current correction factor from EEPROM
    EEPROM.get(0, cal_old);
    vfo = bfo_freq - frequency;
    vfo = (vfo + 5000L) / 10000L;

    if (cal_old > 0)
      cal_old = ((cal_old * vfo + 500000L) / 1000000L);
    else
      cal_old = ((cal_old * vfo - 500000L) / 1000000L);
    cal_old = -cal_old * 10L;

    shift = cal_old - (analogRead(ANALOG_TUNING) * 10) + 5000;
  }

  // if Fbutton is pressed again, or if the CALbutton is released, we save the setting

  if ((digitalRead(FBUTTON) == LOW) || ((digitalRead(CAL_BUTTON) == HIGH) && (calbutton_prev == LOW))) {

    mode = MODE_NORMAL;
    printLine2((char *)"LSB Calibrated!  ");

    //Write the 4 bytes of the correction factor into the eeprom memory.
    EEPROM.put(0, cal);
    delay(700);

    printLine2((char *)"--- SETTINGS ---");

    // return to the original frequency that the VFO was set to before Calibration was executed
    long knob = knob_position(); // get the current tuning knob position
    baseTune = frequency - (50L * knob * TUNING_RANGE / 500);
    updateDisplay();
    calbutton_prev = HIGH;
  }
  else {
    // while the calibration is in progress (CAL_BUTTON is held down), keep tweaking the
    // frequency as read out by the knob, display the change in the second line
    mode = MODE_CALIBRATE;

    // The tuning knob gives readings from 0 to 1000
    // Each step is taken as 10 Hz and the mid setting of the knob is taken as zero

    cal = (analogRead(ANALOG_TUNING) * 10) - 5000 + shift;

    si5351.set_freq((bfo_freq - frequency) * 100LL, SI5351_CLK2);

    ltoa(cal, b, DEC);
    strcpy(c, "offset:");
    strcat(c, b);
    strcat(c, " Hz    ");
    printLine2(c);
    //calculate the correction factor in ppb and apply it
    cal = (cal * -1000000000LL) / (bfo_freq - frequency) ;
    si5351.set_correction(cal);
  }
}


/**
   When LSB is calibrated, USB may still not be zero beat.
   USB calibration sets the accurate readout of the tuned frequency by applying an extra offset in USB mode only
   To calibrate, follow these steps:
   1. First make sure the LSB is calibrated correctly
   2. Tune in a USB signal that is at a known frequency.
   3. Now, set the display to show the correct frequency,
      the signal will no longer be tuned up properly
   4. Use the "USB calibrate" option in the "Settings" menu
   5. tune in the signal until it sounds proper.
   6. Press the FButton again.
   In step 5, when we say 'sounds proper' then, for a CW signal/carrier it means zero-beat
   and for USB it is the most natural sounding setting.

   We store the USBoffset in the EEPROM and read it in setup() when the Radiuno is powered up
*/

void USBoffset() {

  static int USB_OFFSET_old;
  static int shift;

  if (mode != MODE_USBOFFSET) {
    setUSB();
    //fetch the current USB offset value from EEPROM
    EEPROM.get(4, USB_OFFSET_old);
    shift = USB_OFFSET_old - (analogRead(ANALOG_TUNING) * 10) + 5000;
  }

  // The tuning knob gives readings from 0 to 1000
  // Each step is taken as 10 Hz and the mid setting of the knob is taken as zero
  USB_OFFSET = (analogRead(ANALOG_TUNING) * 10) - 5000 + shift;

  // if Fbutton is pressed again, we save the setting
  if (digitalRead(FBUTTON) == LOW) {
    mode = MODE_NORMAL;
    printLine2((char *)"USB Calibrated! ");

    //Write the 2 bytes of the USB offset into the eeprom memory.
    EEPROM.put(4, USB_OFFSET);
    delay(700);
    printLine2((char *)"--- SETTINGS ---");

    // return to the original frequency that the VFO was set to before USBoffset adjustment
    long knob = knob_position(); // get the current tuning knob position
    baseTune = frequency - (50L * knob * TUNING_RANGE / 500);
    setLSB();
  }
  else {
    // while USB offset adjustment is in progress, keep tweaking the
    // frequency as read out by the knob, display the change in the second line
    mode = MODE_USBOFFSET;
    si5351.set_freq((bfo_freq + frequency - USB_OFFSET) * 100LL, SI5351_CLK2);
    itoa(USB_OFFSET, b, DEC);
    strcpy(c, "offset:");
    strcat(c, b);
    strcat(c, " Hz    ");
    printLine2(c);
  }
}

/**
   The setFrequency is a little tricky routine, it works differently for USB and LSB
   The BITX BFO is permanently set to lower sideband, (that is, the crystal frequency
   is on the higher side slope of the crystal filter).
   
   LSB: The VFO frequency is subtracted from the BFO. Suppose the BFO is set to exactly 12 MHz
   and the VFO is at 5 MHz. The output will be at 12.000 - 5.000  = 7.000 MHz
   USB: The BFO is subtracted from the VFO. Makes the LSB signal of the BITX come out as USB!!
   Here is how it will work:
   Consider that you want to transmit on 14.000 MHz and you have the BFO at 12.000 MHz. We set
   the VFO to 26.000 MHz. Hence, 26.000 - 12.000 = 14.000 MHz. Now, consider you are whistling a tone
   of 1 KHz. As the BITX BFO is set to produce LSB, the output from the crystal filter will be 11.999 MHz.
   With the VFO still at 26.000, the 14 Mhz output will now be 26.000 - 11.999 = 14.001, hence, as the
   frequencies of your voice go down at the IF, the RF frequencies will go up!

   Thus, setting the VFO on either side of the BFO will flip between the USB and LSB signals.
*/

void setFrequency(unsigned long f) {

  if (isUSB) {
    si5351.set_freq((bfo_freq + f - USB_OFFSET) * 100ULL, SI5351_CLK2);
  }
  else {
    si5351.set_freq((bfo_freq - f) * 100ULL, SI5351_CLK2);
  }
  frequency = f;
}

/**
   The Checkt TX toggles the T/R line. The current BITX wiring up doesn't use this
   but if you would like to make use of RIT, etc, You must wireup an NPN transistor to to the PTT line
   as follows :
   emitter to ground,
   base to TX_RX line through a 4.7K resistr(as defined at the top of this source file)
   collecter to the PTT line
   Now, connect the PTT to the control connector's PTT line (see the definitions on the top)
   Yeah, surprise! we have CW supported on the Raduino
*/

void checkTX() {
  // We don't check for ptt when transmitting cw
  // as long as the cwTimeout is non-zero, we will continue to hold the
  // radio in transmit mode
  if (cwTimeout > 0)
    return;

  if (digitalRead(PTT) == 0 && inTx == 0) {
    inTx = 1;
    digitalWrite(TX_RX, 1);
    updateDisplay();
  }

  if (digitalRead(PTT) == 1 && inTx == 1) {
    inTx = 0;
    digitalWrite(TX_RX, 0);
    updateDisplay();
  }
}

/* CW is generated by injecting the side-tone into the mic line.
   Watch http://bitxhacks.blogspot.com for the CW hack
   nonzero cwTimeout denotes that we are in cw transmit mode.

   This function is called repeatedly from the main loop() hence, it branches
   each time to do a different thing

   There are three variables that track the CW mode
   inTX     : true when the radio is in transmit mode
   keyDown  : true when the CW is keyed down, you maybe in transmit mode (inTX true)
              and yet between dots and dashes and hence keyDown could be true or false
   cwTimeout: Figures out how long to wait between dots and dashes before putting
              the radio back in receive mode
*/

void checkCW() {

  if (keyDown == 0 && analogRead(ANALOG_KEYER) < 50) {
    //switch to transmit mode if we are not already in it
    if (inTx == 0) {
      digitalWrite(TX_RX, 1);
      //give the relays a few ms to settle the T/R relays
      delay(50);
    }
    inTx = 1;
    keyDown = 1;
    tone(CW_TONE, sideTone);
    updateDisplay();
  }

  //reset the timer as long as the key is down
  if (keyDown == 1) {
    cwTimeout = CW_TIMEOUT + millis();
  }

  //if we have a keyup
  if (keyDown == 1 && analogRead(ANALOG_KEYER) > 150) {
    keyDown = 0;
    noTone(CW_TONE);
    cwTimeout = millis() + CW_TIMEOUT;
  }

  //if we are in cw-mode and have a keyuup for a longish time
  if (cwTimeout > 0 && inTx == 1 && cwTimeout < millis()) {
    //move the radio back to receive
    digitalWrite(TX_RX, 0);
    inTx = 0;
    cwTimeout = 0;
    updateDisplay();
  }
}

/**
   A trivial function to wrap around the function button
*/
int btnDown() {
  if (digitalRead(FBUTTON) == HIGH)
    return 0;
  else
    return 1;
}

/**
   The Function Button is used for several functions
   NORMAL menu (normal operation):
   1 short (<0.5 sec) press: swap VFO A/B
   2 short presses: toggle RIT on/off
   3 short presses: toggle LSB/USB
   long press (>2 Sec): VFO A=B
   VERY long press (>5 sec): go to SETTINGS menu
   SETTINGS menu:
   1 short press: LSB calibration
   2 short presses: USB calibration
   3 short presses: VFO drive level in LSB mode
   4 short presses: VFO drive level in USB mode
   5 short presses: Tuning range upper&lower limit settings
   long press: exit SETTINGS menu - go back to NORMAL menu
*/
void checkButton() {

  static int clicks, action;
  static long t1, t2;
  static char pressed = 0;
  static char SETTINGSmenu = 0; // whether or not we are in the SETTINGS menu

  if (!btnDown()) {
    t2 = millis() - t1; //time elapsed since last button press
    if (pressed == 1) {
      if (!SETTINGSmenu && t2 > 5000) { // detect VERY long press to go to SETTINGS menu
        SETTINGSmenu = 1;
        printLine2((char *)"--- SETTINGS ---");
        clicks = 10;
      }
      else if (!SETTINGSmenu && t2 > 1200) { //detect long press to reset the VFO's
        resetVFOs();
        delay(700);
        printLine2((char *)"                ");
        clicks = 0;
      }
      else if (SETTINGSmenu && t2 > 1200) { //detect long press to exit the SETTINGS menu
        SETTINGSmenu = 0;
        clicks = 0;
        printLine2((char *)" --- NORMAL --- ");
        delay(700);
        printLine2((char *)"                ");
      }
    }
    if (t2 > 500) { // max time between button clicks
      action = clicks;
      if (SETTINGSmenu) {
        clicks = 10;
      }
      else {
        clicks = 0;
      }
    }
    pressed = 0;
  }
  else {
    delay(10);
    if (btnDown()) {
      // button was really pressed, not just some noise
      if (pressed == 0) {
        pressed = 1;
        t1 = millis();
        action = 0;
        clicks++;
        if (clicks > 15) {
          clicks = 11;
        }
        if (clicks > 3 && clicks < 10) {
          clicks = 1;
        }
        switch (clicks) {
          //Normal menu options
          case 1:
            printLine2((char *)"swap VFO's      ");
            break;
          case 2:
            printLine2((char *)"toggle RIT      ");
            break;
          case 3:
            printLine2((char *)"toggle LSB/USB  ");
            break;

          //SETTINGS menu options
          case 11:
            printLine2((char *)"LSB calibration ");
            break;
          case 12:
            printLine2((char *)"USB calibration ");
            break;
          case 13:
            printLine2((char *)"VFO drive - LSB ");
            break;
          case 14:
            printLine2((char *)"VFO drive - USB ");
            break;
          case 15:
            printLine2((char *)"set tuning range");
            break;
        }
      }
      else if ((millis() - t1) > 600)
        printLine2((char *)"                ");
    }
  }
  switch (action) {
    // Normal menu options
    case 1:
      swapVFOs();
      delay(700);
      printLine2((char *)"                ");
      break;
    case 2:
      toggleRIT();
      delay(700);
      printLine2((char *)"                ");
      break;
    case 3:
      toggleMode();
      delay(700);
      printLine2((char *)"                ");
      break;

    // SETTINGS MENU options
    case 11:
      calibrate();
      break;
    case 12:
      USBoffset();
      break;
    case 13:
      setLSB();
      VFOdrive();
      break;
    case 14:
      setUSB();
      VFOdrive();
      break;
    case 15:
      set_tune_range();
      break;
  }
}

void swapVFOs() {
  if (vfoActive == VFO_B) {
    vfoActive = VFO_A;
    vfoB = frequency;
    frequency = vfoA;
  }
  else {
    vfoActive = VFO_B;
    vfoA = frequency;
    frequency = vfoB;
  }
  setFrequency(frequency);
  long knob = knob_position(); // get the current tuning knob position
  baseTune = frequency - (50L * knob * TUNING_RANGE / 500);
  updateDisplay();
  printLine2((char *)"VFO's swapped!  ");
}

void toggleRIT() {
  if (ritOn) {
    ritOn = 0;
    printLine2((char *)"RIT is now OFF! ");
  }
  else {
    ritOn = 1;
    printLine2((char *)"RIT is now ON!  ");
  }
  updateDisplay();
}

void toggleMode() {
  if (isUSB) {
    setLSB();
    printLine2((char *)"Mode is now LSB! ");
  }
  else {
    setUSB();
    printLine2((char *)"Mode is now USB! ");
  }
}

void setUSB() {
  isUSB = 1;
  //si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA);
  set_drive_level(USBdrive);
  setFrequency(frequency);
  updateDisplay();
}

void setLSB() {
  isUSB = 0;
  //si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_4MA);
  set_drive_level(LSBdrive);
  setFrequency(frequency);
  updateDisplay();
}

void resetVFOs() {
  printLine2((char *)"VFO A=B !       ");
  vfoA = vfoB = frequency;
  updateDisplay();
  return;
}

void VFOdrive() {

  static int drive;
  static int drive_old;
  static int shift;

  if (mode != MODE_DRIVELEVEL) {
    if (isUSB)
      drive_old = USBdrive / 2 - 1;
    else
      drive_old = LSBdrive / 2 - 1;

    shift = analogRead(ANALOG_TUNING);
  }
  //generate drive level values 2,4,6,8 from tuning pot
  drive = 2 * (((int((analogRead(ANALOG_TUNING) - shift) / 50) + drive_old) & 3) + 1);

  // if Fbutton is pressed again, we save the setting

  if (digitalRead(FBUTTON) == LOW) {
    mode = MODE_NORMAL;
    printLine2((char *)"drive level set!");

    if (isUSB) {
      USBdrive = drive;
      //Write the 2 bytes of the USBdrive level into the eeprom memory.
      EEPROM.put(8, drive);
    }
    else {
      LSBdrive = drive;
      //Write the 2 bytes of the LSBdrive level into the eeprom memory.
      EEPROM.put(6, drive);
    }
    delay(700);

    printLine2((char *)"--- SETTINGS ---");

    long knob = knob_position(); // get the current tuning knob position
    baseTune = frequency - (50L * knob * TUNING_RANGE / 500);
  }
  else {
    // while the drive level adjustment is in progress, keep tweaking the
    // drive level as read out by the knob and display it in the second line
    mode = MODE_DRIVELEVEL;
    set_drive_level(drive);

    itoa(drive, b, DEC);
    strcpy(c, "drive level: ");
    strcat(c, b);
    strcat(c, "mA");
    printLine2(c);
  }
}

void set_tune_range() {

  static int limitval;
  static char lowerlimit;

  if (mode != MODE_TUNERANGE) {
    baseTune = 7000000L;
    lowerlimit = 1; // whether we are setting the upper or lower limit of the tuning range
  }

  //generate values 7000-7500 from the tuning pot
  limitval = baseTune / 1000 + 10 * int(analogRead(ANALOG_TUNING) / 20);


  // if Fbutton is pressed again, we save the setting

  if (digitalRead(FBUTTON) == LOW) {

    if (lowerlimit) {
      baseTune = limitval * 1000L;
      //Write the 4 bytes of the lower limit into the eeprom memory.
      EEPROM.put(10, baseTune);
      lowerlimit = 0;
      printLine2((char *)"lower limit set!");
    }
    else {
      TUNING_RANGE = limitval - (baseTune / 1000L);
      //Write the 2 bytes of the upper limit into the eeprom memory.
      EEPROM.put(14, TUNING_RANGE);
      printLine2((char *)"upper limit set!");
      mode = MODE_NORMAL;
    }

    delay(700);
    printLine2((char *)"--- SETTINGS ---");

  }
  else {
    mode = MODE_TUNERANGE;
    itoa(limitval, b, DEC);
    if (lowerlimit)
      strcpy(c, "lower: ");
    else
      strcpy(c, "upper: ");
    strcat(c, b);
    strcat(c, " kHz ");
    printLine2(c);
  }
}

long knob_position() {
  long knob = 0;
  // the knob value normally ranges from 0 through 1023 (10 bit ADC)
  // in order to expand the range by a factor 10, we need 10^2 = 100x oversampling
  for (int i = 0; i < 100; i++) {
    knob = knob + analogRead(ANALOG_TUNING) - 10; // take 100 readings from the ADC
  }
  knob = (knob + 5L) / 10L; // take the average of the 100 readings and multiply the result by 10
  //now the knob value ranges from -100 through 10130
  return knob;
}

void set_drive_level(int level) {
  switch (level) {
    case 2:
      si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
      break;
    case 4:
      si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_4MA);
      break;
    case 6:
      si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_6MA);
      break;
    case 8:
      si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_8MA);
      break;
  }
}

/**
   The Tuning mechansim of the Raduino works in a very innovative way. It uses a tuning potentiometer.
   The tuning potentiometer that a voltage between 0 and 5 volts at ANALOG_TUNING pin of the control connector.
   This is read as a value between 0 and 1000. By 100x oversampling ths range is expanded by a factor 10.
   Hence, the tuning pot gives you 10,000 steps from one end to the other end of its rotation. Each step is 50 Hz,
   thus giving maximum 500 Khz of tuning range. The tuning range is scaled down depending on the limit settings.
   The standard tuning range (for the standard 1-turn pot) is 50 Khz. But it is also possible to use a 10-turn pot
   to tune accross the entire 40m band. In that case you need to change the values for TUNING_RANGE and baseTune.
   When the potentiometer is moved to either end of the range, the frequency starts automatically moving
   up or down in 10 Khz increments
*/

void doTuning() {

  long knob = knob_position(); // get the current tuning knob position

  // the knob is fully on the low end, move down by 10 Khz and wait for 200 msec
  if (knob < -80 && frequency > LOWEST_FREQ) {
    baseTune = baseTune - 10000L;
    frequency = baseTune + (50L * knob * TUNING_RANGE / 500);
    updateDisplay();
    setFrequency(frequency);
    delay(200);
  }
  // the knob is full on the high end, move up by 10 Khz and wait for 200 msec
  else if (knob > 10120L && frequency < HIGHEST_FREQ) {
    baseTune = baseTune + 10000L;
    frequency = baseTune + (50L * knob * TUNING_RANGE / 500);
    setFrequency(frequency);
    updateDisplay();
    delay(200);
  }
  // the tuning knob is at neither extremities, tune the signals as usual ("flutter fix" by Jerry, KE7ER)
  else if (knob != old_knob) {
    static char dir_knob;
    if ( (knob > old_knob) && ((dir_knob == 1) || ((knob - old_knob) > 5)) ||
         (knob < old_knob) && ((dir_knob == 0) || ((old_knob - knob) > 5)) )   {
      if (knob > old_knob) {
        dir_knob = 1;
        frequency = baseTune + (50L * (knob - 5) * TUNING_RANGE / 500);
      } else {
        dir_knob = 0;
        frequency = baseTune + (50L * knob * TUNING_RANGE / 500);
      }
      old_knob = knob;
      setFrequency(frequency);
      updateDisplay();
    }
  }
}

/**
   setup is called on boot up
   It setups up the modes for various pins as inputs or outputs
   initiliaizes the Si5351 and sets various variables to initial state

   Just in case the LCD display doesn't work well, the debug log is dumped on the serial monitor
   Choose Serial Monitor from Arduino IDE's Tools menu to see the Serial.print messages
*/
void setup() {
  String ver, raduino_version = "v1.07a";

  lcd.begin(16, 2);
  printBuff[0] = 0;

  // Start serial and initialize the Si5351
  Serial.begin(9600);
  analogReference(DEFAULT);
  Serial.println("*Raduino booting up");
  Serial.println(raduino_version);

  //configure the function button to use the internal pull-up
  pinMode(FBUTTON, INPUT_PULLUP);
  digitalWrite(FBUTTON, HIGH);

  pinMode(PTT, INPUT);
  digitalWrite(PTT, HIGH);

  pinMode(CAL_BUTTON, INPUT_PULLUP);
  digitalWrite(CAL_BUTTON, HIGH);

  pinMode(CW_KEY, OUTPUT);
  pinMode(CW_TONE, OUTPUT);
  digitalWrite(CW_KEY, 0);
  digitalWrite(CW_TONE, 0);
  digitalWrite(TX_RX, 0);

  EEPROM.get(16, ver); // previous sketch version
  delay(10);

  // when Fbutton or CALbutton is pressed during power up
  // or after a version update
  // all settings will be restore to the standard "factory" values

  if ((digitalRead(CAL_BUTTON) == LOW) || (digitalRead(FBUTTON) == LOW) || (ver != raduino_version)) {
    printLine1((char *)"loading standard");
    printLine2((char *)"settings...     ");
    EEPROM.put(0, 0L); //corr factor
    delay(10);
    EEPROM.put(4, 1500); //USB offset
    delay(10);
    EEPROM.put(6, 4); //VFO drive level in LSB mode
    delay(10);
    EEPROM.put(8, 8); //VFO drive level in USB mode
    delay(10);
    EEPROM.put(10, 7100000UL); //baseTune
    delay(10);
    EEPROM.put(14, 50); //tuning range
    delay(10);
    EEPROM.put(16, raduino_version); //version number string
    delay(2000);
  }

  strcpy (c, "Raduino ");
  raduino_version.toCharArray(b, 30);
  strcat (c, b);
  printLine1(c);
  printLine2((char *)"             ");
  delay(1000);

  //fetch user settings from EEPROM
  //Serial.println("EEPROM: LSB corr, USB offset, LSBdrive, USBdrive, lowerlimit, tuning range, version_number");

  EEPROM.get(0, cal);
  //Serial.println(cal);
  delay(10);

  EEPROM.get(4, USB_OFFSET);
  //Serial.println(USB_OFFSET);
  delay(10);

  EEPROM.get(6, LSBdrive);
  //Serial.println(LSBdrive);
  delay(10);

  EEPROM.get(8, USBdrive);
  //Serial.println(USBdrive);
  delay(10);

  EEPROM.get(10, baseTune);
  //Serial.println(baseTune);
  delay(10);

  EEPROM.get(14, TUNING_RANGE);
  //Serial.println(TUNING_RANGE);
  delay(10);

  EEPROM.get(16, raduino_version);
  //Serial.println(raduino_version);
  delay(10);

  //initialize the SI5351 and apply the correction factor
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000L, cal);

  Serial.println("*Initiliazed Si5351\n");

  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);
  Serial.println("*Fixed PLL\n");
  si5351.output_enable(SI5351_CLK0, 0);
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 1);

  setLSB();

  Serial.println("*Output enabled PLL\n");
  si5351.set_freq(500000000L , SI5351_CLK2);
  Serial.println("*Si5350 ON\n");
  mode = MODE_NORMAL;
  delay(10);

  //If no FButton is installed, and you still want to use custom tuning range settings,
  //uncomment the following 2 lines and adapt the values to your liking:
  //TUNING_RANGE = 50;    // tuning range (in kHz) of the tuning pot
  //baseTune = 7100000UL; // frequency (Hz) when tuning pot is at minimum position
}

void loop() {
  switch (mode) {
    case 0: // for backward compatibility: execute calibration when CAL button is pressed
      if (digitalRead(CAL_BUTTON) == LOW) {
        mode = MODE_CALIBRATE;
        calbutton_prev = LOW;
        si5351.set_correction(0);
        printLine1((char *)"Calibrating: Set");
        printLine2((char *)"to zerobeat.    ");
        delay(2000);
        return;
      }
      break;
    case 1: //LSB calibration
      calibrate();
      delay(50);
      return;
    case 2: //USB calibration
      USBoffset();
      delay(50);
      return;
    case 3: //set VFO drive level
      VFOdrive();
      delay(50);
      return;
    case 4: // set upper and lower limits of tuning range
      set_tune_range();
      delay(50);
      return;
  }
  /*
    checkCW();
    checkTX(); */
  checkButton();
  
  // Read signal level
  int sma = analogRead(SMETER);
  sm = logscale(sma); 
  if (sma < 10) s_level = sm;
  else if (sma < 80) s_level = 9;
  else if (sma < 110) s_level = 10;
  else s_level = 20;
  if (sm != last_sm) updateSMeter();
  last_sm = sm;
  doTuning();
  delay(50);
}
