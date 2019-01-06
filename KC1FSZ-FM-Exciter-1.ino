// Control code for FM Exciter 1.0
// Bruce MacKinnn KC1FSZ
// 5 January 2019
//
#include <SPI.h>
#include <Wire.h>

// The Etherkit library
#include <si5351.h>

#define PIN_ADF4001_CLK 2
#define PIN_ADF4001_DATA 3
#define PIN_ADF4001_LE 4
#define PIN_ADF4001_MUXOUT 5
#define PIN_LOCK_IND 6
#define PIN_LED13 13 

// This is the mask that is used to isolate the MSB of a 24-bit word
#define MASK23 (1 << 23)

Si5351 si5351;

// Desired center frequency
unsigned long vfo = 0;

unsigned int nDivider = 0;
unsigned int rDivider = 0;

void clkStrobeADF4001() {
  digitalWrite(PIN_ADF4001_CLK,1);
  digitalWrite(PIN_ADF4001_CLK,0);
}

void latchStrobeADF4001() {
  digitalWrite(PIN_ADF4001_LE,1);
  digitalWrite(PIN_ADF4001_LE,0);
}

void writeBitADF4001(bool bit) {
    digitalWrite(PIN_ADF4001_DATA,(bit) ? 1 : 0);
    clkStrobeADF4001();
}

// This function writes a complete 24-bit word into the ADF4001, MSB first!
void writeADF4001(unsigned long w) {
  // Clock the data bits into the shift register
  for (unsigned int i = 0; i < 24; i++) {
    writeBitADF4001(w && MASK23 != 0);
    // Rotate to the left one to expose the next least significant bit
    w = w << 1;
  }
  // Load the register
  latchStrobeADF4001();
}

void initializeADF4001() {
  
  // We are using the "Initialization Latch Method" of programming (see page 13)
  unsigned long latch = 0;
  unsigned long a = 0;

  // 1. Program the initialiation latch (11).  Make sure the F1 bit is programmed
  // to 0.
  latch = 0;
  // PD1: Power Down 2
  a = 0x0; [Normal operation]
  latch |= (a << 21);
  // CPI6/CPI5/CPI4: Current Setting 2
  a = 0x3; [2.5mA using 4.7k reistor]
  latch |= (a << 18);
  // CPI3/CPI2/CPI1: Current Setting 1
  a = 0x3; [2.5mA using 4.7k reistor]
  latch |= (a << 15);
  // TC4/TC3/TC2/TC1: Timer Counter Control
  a = 0x0; [3 cycles]
  latch |= (a << 11);
  // F5/F4: Fast Lock Mode and Fast Lock Mode Enable
  a = 0x0; [Disabled]
  latch |= (a << 9);
  // F3: CP Three State
  a = 0x0; [Normal]
  latch |= (a << 8);
  // F2: Phase detector polarity
  a = 0x1; [Positive]
  latch |= (a << 7);
  // M3/M2/M1: MUXOUT Control
  a = 0x1; [Digital lock detect]
  latch |= (a << 4);
  // PD1: Power Down 1
  a = 0x0; [Normal]
  latch |= (a << 3);
  // F1: Counter Reset
  a = 0x0; [Normal]
  latch |= (a << 2);
  // C2/C1: Control Bits
  a = 0x3;
  latch |= a;
  writeADF4001(latch);

  // 2. Do an R load
  latch = 0;
  // LDP: Lock Detect Precision
  a = 0x1; [5 consecutive cycles needed]
  latch |= (a << 20);
  // ABP2/ABP1: Anti-Backlash Width
  a = 0x0; [2.9ns]
  latch |= (a << 16);
  // R14->R1: 14-bit reference counter 
  a = rDivider;
  latch |= (a << 2);
  // C2/C1: Control Bits
  a = 0x0;
  latch |= a;
  writeADF4001(latch);
  
  // 3. Do an N load
  latch = 0;
  // G1: CP Gain
  a = 0x0; // [Charge pump using setting 1] 
  latch |= (a << 21);
  // N13->N1: 13-bit N counter
  a = nDivider;
  latch |= (a << 8);
  // C2/C1: Control Bits
  a = 0x1;
  latch |= a;
  writeADF4001(latch);  
}

void setup() {

  pinMode(PIN_LED13,OUTPUT);
  pinMode(PIN_LOCK_IND,OUTPUT);
  pinMode(PIN_ADF4001_CLK,OUTPUT);
  pinMode(PIN_ADF4001_DATA,OUTPUT);
  pinMode(PIN_ADF4001_LE,OUTPUT);
  pinMode(PIN_ADF4001_MUXOUT,INPUT);

  digitalWrite(PIN_LED13,0);
  digitalWrite(PIN_LOCK_IND,0);
  digitalWrite(PIN_ADF4001_CLK,0);
  digitalWrite(PIN_ADF4001_DATA,0);
  digitalWrite(PIN_ADF4001_LE,0);

  // Hello world check
  digitalWrite(PIN_LED13,1);  
  delay(250);
  digitalWrite(PIN_LED13,0);  
  delay(250);
  digitalWrite(PIN_LED13,1);  
  delay(250);
  digitalWrite(PIN_LED13,0);  
  delay(250);
  
  //Serial.begin(9600);

  // Si5351 initialization
  si5351.init(SI5351_CRYSTAL_LOAD_8PF,0,0);
  // Boost up drive strength
  si5351.drive_strength(SI5351_CLK0,SI5351_DRIVE_8MA);

  // W1TKZ Repeater RX
  vfo = 147030000;
  nDivider = 8;
  rDivider = 1;

  // Get the Si5351 frequency standard loaded
  unsigned long f = vfo / (unsigned long)nDivider;
  si5351.set_freq((unsigned long long)f * 100ULL,SI5351_CLK0);

  // Get the ADF4001 PLL setup 
  initializeADF4001();
}

void loop() {
  // Check the PLL lock 
  int a = digitalRead(PIN_ADF4001_MUXOUT);
  // Display 
  digitalWrite(PIN_LOCK_IND,a);
}
