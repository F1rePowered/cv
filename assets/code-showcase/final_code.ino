#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,  16, 2);
//custom characters:
byte arrowUp[8] = {
	0b00000,
	0b00100,
	0b01110,
	0b11111,
	0b11111,
	0b00100,
	0b00100,
	0b00100
};
  
byte arrowDown[8] = {
	0b00100,
	0b00100,
	0b00100,
	0b11111,
	0b11111,
	0b01110,
	0b00100,
	0b00000
};

byte musicalNote[8] = {
	0b00011,
	0b00011,
	0b00010,
	0b01110,
	0b10010,
	0b10010,
	0b01110,
	0b00000
};
// code maps buttons to MIDI note on/off events;
// requires an Arduino Leonardo;

#include <Wire.h> //facilitates I2C communication
#include <Adafruit_Trellis.h> //libraries for interacting with trellis board buttons
#include <Adafruit_UNTZtrument.h> //libraries for interacting with trellis board buttons
#include "MIDIUSB.h" //library for MIDI communication over USB


#define LED     13 // heartbeat LED on pin 13 (shows code is working) 
#define CHANNEL 1  // MIDI channel number

#ifndef instrumentSize //conditional compilation for instrument configurations with different numbers of trellis boards (for example there are instruments with 8 trellis boards)
//code for instrument with 4 trellis boards
//addr[] represents the I2C addresses assigned to the upper left, upper right, lower left, and lower right matrices, respectively. 
//this assumes an upright orientation, where the labels on the board are arranged in the normal reading direction.
Adafruit_Trellis     T[4];
Adafruit_UNTZtrument untztrument(&T[0], &T[1], &T[2], &T[3]); //object to interface with trellis boards
const uint8_t        addr[] = { 0x70, 0x71,
                                0x72, 0x73 };
#endif // instrumentSize

// MIDI note numbers are simply centered based on
// the number of Trellis buttons; each row doesn't necessarily
// correspond to an octave.
#define WIDTH     ((sizeof(T) / sizeof(T[0])) * 2)
#define N_BUTTONS ((sizeof(T) / sizeof(T[0])) * 16)
#define LOWNOTE   ((128 - N_BUTTONS) / 2)
bool motionDetected = false;

uint8_t       heart        = 0;  // Heartbeat LED counter
unsigned long prevReadTime = 0L; // Keypad polling timer

const int motionPin = 2;
int motionPinCurrent = LOW;

void setup() {
  Serial.begin(9600);
  pinMode(motionPin, INPUT);
  lcd.init();
  lcd.backlight();
  delay(9000);
  lcd.createChar(0, arrowUp);
  lcd.createChar(1, arrowDown);
  lcd.createChar(2, musicalNote);
  lcd.setCursor(0,0);
  lcd.print("Swipe motion sensor");
  lcd.setCursor(0,1);
  lcd.print("to start.");
  pinMode(LED, OUTPUT);
#ifndef instrumentSize
  untztrument.begin(addr[0], addr[1], addr[2], addr[3]); //initialisation of trellis boards with specified i2c addresses (excluded if has already been defined)
#endif // instrumentSize
  // Default Arduino I2C speed is 100 KHz, but the HT16K33 supports
  // 400 KHz.  We can force this for faster read & refresh, but may
  // break compatibility with other I2C devices...so be prepared to
  // comment this out, or save & restore value as needed.
#ifdef ARDUINO_ARCH_SAMD
//  Wire.setClock(400000L);
#endif
#ifdef _AVR_
//  TWBR = 12; // 400 KHz I2C on 16 MHz AVR
#endif
  untztrument.clear(); //turns off all the LEDs on the grid
  untztrument.writeDisplay();
}

void noteOn(byte channel, byte pitch, byte velocity) { //function defined to send MIDI note on event using MIDIUSB library
  midiEventPacket_t noteOn = {0x09, 0x90 | channel, pitch, velocity}; // byte channel,
  MidiUSB.sendMIDI(noteOn);
  MidiUSB.flush(); //clears midi buffer, ensures note is sent out
}

void noteOff(byte channel, byte pitch, byte velocity) { //function defined to send MIDI note off event using MIDIUSB library
  midiEventPacket_t noteOff = {0x08, 0x80 | channel, pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
  MidiUSB.flush();
}

void loop() {
  motionPinCurrent = digitalRead(motionPin); //updates motion pin state
 if (motionPinCurrent == HIGH && !motionDetected){
  motionDetected = true;
  Serial.println("Motion detected");
 }//if motion is detected once, and has not been detected before, bool activates
  unsigned long t = millis(); //returns the nr of milliseconds since the arduino has been powered up: acts as a timer, stores it in the variable t
  if((t - prevReadTime) >= 20L) { // 20ms = min Trellis poll time (checks if 20ms have passed since last time trellis button states were read)
    if(motionDetected && untztrument.readSwitches()) { // Button state change? (checks for a button state change, readSwitches is a function that is part of the Adafruit library, returns true if button states have changed) (also checks if motion has been detected to make buttons usable)
      for(uint8_t i=0; i<N_BUTTONS; i++) { // For each button... (sets up loop that iterates N_BUTTONS times through N_BUTTONS, being the number of buttons on the trellis boards)
        // get column/row for button, convert to MIDI note number
        uint8_t x, y, note;
        untztrument.i2xy(i, &x, &y); // gets the x and y coordinates of the current button using i2xy function which is part of the Adafruit library
        note = LOWNOTE + y * WIDTH + x; //calculates a MIDI note number (note variable) based on the button's position, calculation involves button row(y), column(x) and constants LOWNOTE and WIDTH
        //this code segment is essentially responsible for periodically checking the state of the Trellis buttons, 
        //detecting any changes, and updating the MIDI notes accordingly
        //also makes sure that the code won't excessively poll button states, maintains a minimum time interval of 20 ms between reads
        if(untztrument.justPressed(i)) {
          noteOn(CHANNEL, note, 127); //going to the highest note that can be played(note on meaning that the note is being played at maximum value)
          //CHANNEL constant is defined as 1, as we are controlling one single MIDI instrument and all messages are meant for the same sound generator (all MIDI messages are sent on the same channel)
          untztrument.setLED(i); //used to turn on the LED associated with a specific button on the trellis board
          lcd.clear();
          lcd.setCursor(4,1);
          lcd.write(byte(2));
          lcd.setCursor(11,1);
          lcd.write(byte(2));
          lcd.setCursor(10,1); //sets text start to column 10 row 1
          lcd.write(byte(1)); //display custom character 1
          lcd.setCursor(9,1);
          lcd.write(byte(1));
          lcd.setCursor(8,1);
          lcd.write(byte(1));
          lcd.setCursor(7,1);
          lcd.write(byte(1));
          lcd.setCursor(6,1);
          lcd.write(byte(1));
          lcd.setCursor(5,1);
          lcd.write(byte(1));
          //displays a couple of down arrows and two musical notes on the sides(custom characters) in the middle of the lcd screen and on the sides respectively, musical notes are on upper row
        } else if(untztrument.justReleased(i)) {
          noteOff(CHANNEL, note, 0); //going back to the lowest note than can be played(note off meaning the note is basically not played as the value is 0)
          untztrument.clrLED(i); //used to turn off the LED associated with a specific button on the trellis board
          lcd.clear();
          lcd.setCursor(4,0);
          lcd.write(byte(2));
          lcd.setCursor(11,0);
          lcd.write(byte(2));
          lcd.setCursor(10,1); //sets text start to column 10 row 1
          lcd.write(byte(0)); //display custom character 0
          lcd.setCursor(9,1);
          lcd.write(byte(0));
          lcd.setCursor(8,1);
          lcd.write(byte(0));
          lcd.setCursor(7,1);
          lcd.write(byte(0));
          lcd.setCursor(6,1);
          lcd.write(byte(0));
          lcd.setCursor(5,1);
          lcd.write(byte(0));
          //displays a couple of up arrows and two musical notes on the sides(custom characters) in the middle of the lcd screen and on the sides respectively, musical notes are on bottom row
        }
      }
      untztrument.writeDisplay(); //used to update the state of an LED, sends updated LED states to trellis boards
      //ensures that the LEDs on the Trellis boards accurately reflect the current state of the buttons as tracked in the software
    }
    prevReadTime = t;
    digitalWrite(LED, ++heart & 32); // blink = alive
  }
}
