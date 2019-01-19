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
#define PIN_CTCSS_TONE 9

// This is the mask that is used to isolate the MSB of a 24-bit word
#define MASK23 (1L << 23)

Si5351 si5351;

// Desired center frequency
unsigned long vfo = 0;

unsigned int nDivider = 0;
unsigned int rDivider = 0;

void clkStrobeADF4001() {
  digitalWrite(PIN_ADF4001_CLK,1);
  digitalWrite(PIN_ADF4001_CLK,0);
}

void writeBitADF4001(bool bit) {
    int a = (bit) ? 1 : 0;
    digitalWrite(PIN_ADF4001_DATA,a);
    clkStrobeADF4001();
}

// This function writes a complete 24-bit word into the ADF4001, MSB first!
void writeADF4001(unsigned long w) {
  // Latch enable low.  This is usedful for triggering logic analyzers
  digitalWrite(PIN_ADF4001_LE,0);  
  // Clock the data bits into the shift register
  for (unsigned int i = 0; i < 24; i++) {
    // VERY IMPORTANT: THIS NEEDS TO BE BIT-WISE AND!
    writeBitADF4001((w & MASK23) != 0);
    // Rotate to the left one to expose the next least significant bit
    w = w << 1;
  }
  // Device latch are loaded on this leading edge
  digitalWrite(PIN_ADF4001_LE,1);  
  delayMicroseconds(1);
}

void initializeADF4001(char muxout) {
  
  // We are using the "Initialization Latch Method" of programming (see page 13)
  unsigned long latch = 0;
  unsigned long a = 0;

  // 1. Program the initialiation latch (0b11).  Make sure the F1 bit is programmed
  // to 0.
  latch = 0;
  // PD2: Power Down 2
  a = 0x0; // [Normal operation]
  latch |= (a << 21);
  // CPI6/CPI5/CPI4: Current Setting 2
  a = 0x3; // [2.5mA using 4.7k reistor]
  latch |= (a << 18);
  // CPI3/CPI2/CPI1: Current Setting 1
  a = 0x3; // [2.5mA using 4.7k reistor]
  latch |= (a << 15);
  // TC4/TC3/TC2/TC1: Timer Counter Control
  a = 0x0; // [3 cycles]
  latch |= (a << 11);
  // F5/F4: Fast Lock Mode and Fast Lock Mode Enable
  a = 0x0; // [Disabled]
  latch |= (a << 9);
  // F3: CP Three State
  a = 0x0; // [Normal]
  latch |= (a << 8);
  // F2: Phase detector polarity
  a = 0x0; // [Negative]
  //a = 0x1; // [Positive]
  latch |= (a << 7);
  // M3/M2/M1: MUXOUT Control
  //a = 0x1; // [Digital lock detect]
  //a = 0x3; // [VDD]
  //a = 0x7; // [GND]
  a = (muxout & 0x07);
  latch |= (a << 4);
  // PD1: Power Down 1
  a = 0x0; // [Normal]
  latch |= (a << 3);
  // F1: Counter Reset
  a = 0x0; // [Normal]
  latch |= (a << 2);
  // C2/C1: Control Bits
  a = 0x3;
  latch |= a;
  writeADF4001(latch);

  // 2. Do an R load
  latch = 0;
  // LDP: Lock Detect Precision
  a = 0x1; // [5 consecutive cycles needed]
  latch |= (a << 20);
  // T2/T1: Test Bits
  a = 0x0;
  latch |= (a << 18);
  // ABP2/ABP1: Anti-Backlash Width
  a = 0x0; // [2.9ns]
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

void flash() {
  digitalWrite(PIN_LED13,1);  
  delay(250);
  digitalWrite(PIN_LED13,0);  
  delay(250);  
}

long lastStamp = millis();
long interval = 1000;

unsigned long toneLastStamp = micros();
unsigned long toneInterval = 0;
bool tonePhase = false;

void setup() {

  pinMode(PIN_LED13,OUTPUT);
  pinMode(PIN_LOCK_IND,OUTPUT);
  pinMode(PIN_ADF4001_CLK,OUTPUT);
  pinMode(PIN_ADF4001_DATA,OUTPUT);
  pinMode(PIN_ADF4001_LE,OUTPUT);
  pinMode(PIN_ADF4001_MUXOUT,INPUT);
  pinMode(PIN_CTCSS_TONE,OUTPUT);

  digitalWrite(PIN_LED13,0);
  digitalWrite(PIN_LOCK_IND,0);
  digitalWrite(PIN_ADF4001_CLK,0);
  digitalWrite(PIN_ADF4001_DATA,0);
  digitalWrite(PIN_ADF4001_LE,1);
  digitalWrite(PIN_CTCSS_TONE,0);

  // Hello world check
  flash();
  flash();
  
  Serial.begin(9600);
  Serial.println("KC1FSZ FM Exciter Controller");

  // Si5351 initialization
  si5351.init(SI5351_CRYSTAL_LOAD_8PF,0,0);
  // Boost up drive strength
  si5351.drive_strength(SI5351_CLK0,SI5351_DRIVE_8MA);

  // W1TKZ Repeater + Offset
  vfo = 147030000 + 600000;
  // NOTE: THIS IS MESSED UP BECAUSE THE CLOCKS WERE SWAPPED ACCIDENTALLY!
  nDivider = 1;
  rDivider = 8;
  unsigned long plToneX10 = 1230;
  unsigned long plTonePeriodUs = (10000000 / plToneX10);
  toneInterval = plTonePeriodUs / 2;
  
  // Get the Si5351 frequency standard loaded
  unsigned long f = vfo / (unsigned long)rDivider;
  si5351.set_freq((unsigned long long)f * 100ULL,SI5351_CLK0);

  si5351.update_status();
  Serial.print("Si5351/a SYS_INIT: ");
  Serial.print(si5351.dev_status.SYS_INIT);
  Serial.print("  LOL_A: ");
  Serial.print(si5351.dev_status.LOL_A);
  Serial.print("  LOL_B: ");
  Serial.print(si5351.dev_status.LOL_B);
  Serial.print("  LOS: ");
  Serial.print(si5351.dev_status.LOS);
  Serial.print("  REVID: ");
  Serial.println(si5351.dev_status.REVID);

  Serial.print("VFO: ");
  Serial.println(f);

  // ADF4001 self-test
  delay(1000);
  
  // (MUXOUT=VDD)
  initializeADF4001(0x3);
  if (digitalRead(PIN_ADF4001_MUXOUT) != 1) {
    Serial.println("ADF4001 Self-Test Error (1)");
    flash();
  }

  // (MUXOUT=GND)
  initializeADF4001(0x7);
  if (digitalRead(PIN_ADF4001_MUXOUT) != 0) {
    Serial.println("ADF4001 Self-Test Error (0)");
    flash();
  }

  // ADF4001 iniitalization (MUXOUT=Digital Lock)
  initializeADF4001(0x1);
  // ADF4001 iniitalization (MUXOUT=N Counter Out)
  //initializeADF4001(0x2);
}

void loop() {
  
  // Check the PLL lock 
  int a = digitalRead(PIN_ADF4001_MUXOUT);
  // Display 
  digitalWrite(PIN_LOCK_IND,a);
  digitalWrite(PIN_LED13,a);

  // Periodic activity
  if (millis() - lastStamp > interval) {  
    lastStamp = millis();
  }

  // Tone
  unsigned long s = micros();
  if (s - toneLastStamp > toneInterval) {
    toneLastStamp = s;
    tonePhase = !tonePhase;
    digitalWrite(PIN_CTCSS_TONE,(tonePhase) ? 1 : 0);
  }
}
