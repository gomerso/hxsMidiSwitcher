
/*
 * ---------------------------------------------------
 * 
 * Simple 8 button MIDI controller for Line 6 HX Stomp using Nano clone
 * 
 * MIDI connectivity based on https://www.instructables.com/id/Send-and-Receive-MIDI-with-Arduino/
 * MIDI pin 4 to TX1
 * 
 * 128x64 I2C OLED - the one I used was https://smile.amazon.co.uk/gp/product/B076PL474K
 * SDA to A4
 * SCL to A5
 * 
 * 8x momentary SPST switches connected between GND and D2 - D9
 * 
 * ---------------------------------------------------
 */



#include <MIDI.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # ((was 4) or -1 if sharing Arduino reset pin)
#define OLED_ADDR 0x3C  // address of my 128x64 OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// error blink codes
#define ERR_DISP_ALLOC 3 // display allocation error

static const unsigned ledPin = LED_BUILTIN; // use onboard LED as activity indicator
static const byte switchPin[] = {2,3,4,5,6,7,8,9,10,11}; // pins for footswitch inputs
static const byte switchCount = 10; // number of footswitches used
static bool switchPressed[switchCount]; // current state of footswitches
static bool switchLastState[switchCount]; //previous state of footswitches (used for long press detection)
static unsigned long lastPressMillis[switchCount]; // when the last button press was detected
static unsigned long lastReleaseMillis[switchCount]; // when the last button was released

// ******************************** MIDI MESSAGES *******************************

//CHANNEL
int midiChannel = 1;

// FXs

int ccOn = 127;
int ccOff = 0;

//SNAPSHOTS
int snapShots = 69;

int snapShot1 = 0;
int snapShot2 = 1;
int snapShot3 = 2;

//LOOPER
int recordOverdub = 60;
int playStop = 61;
int playOnce = 62;
int redoUndo = 63;
int fwdRev = 65;
int fullHalf = 66;

int looperRecord = 127;
int looperOverdub = 0;
int looperPlay = 127;
int looperStop = 0;
int looperRedo = 127;
int looperPlayOnce = 127;
int looperFwd = 0;
int looperRev = 127;
int looperFull = 0;
int looperHalf = 127;

int looperLight = 12
// ********************************************************************


// Created and binds the MIDI interface to the default hardware Serial port
MIDI_CREATE_DEFAULT_INSTANCE();


void errBlink(int errCode) {
  byte blinkTime = 200; // duration for each blink
  byte blinkGap = 200; // gap between each blink
  int burstWait = 1000; // wait time between bursts 
  for (;;) { // loop forever
    for (int i = 1; i <= errCode; i++) {
      digitalWrite(LED_BUILTIN,HIGH);
      delay(blinkTime);
      digitalWrite(LED_BUILTIN,LOW);
      delay(blinkGap);
    }
    delay(burstWait);
  }
} // end of errBlink()

void p2ledsoff(){
  digitalWrite(12,LOW);
}


void setup() {
  pinMode(ledPin, OUTPUT);  // setup activity LED pin for output
  pinMode(looperLight,OUTPUT);
  digitalWrite(looperLight)
  MIDI.begin(MIDI_CHANNEL_OMNI);  // Listen to all incoming messages
  
 // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
 if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) { // Address 0x3C for my 128x64 variant
   errBlink(ERR_DISP_ALLOC);
  }

  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  display.display();
  delay(500); // Pause for 0.5 seconds
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
  display.clearDisplay();
  
  // Initialise switches and related variable arrays
  for (int i=0;i<switchCount;i++) { 
    pinMode(switchPin[i], INPUT_PULLUP); // add pullup resistors to all footswitch input pins
    switchPressed[i] = false; //initialise switch state
    switchLastState[i] = false; //initialse last switch state
    lastReleaseMillis[i] = millis(); // initialise time switch last released
    lastPressMillis[i] = lastPressMillis[i] -1; // initialise time switch last pressed
  }

} // end of setup


void loop() {
  readButtons();
  displayUpdate();
  ledUpdate();
  midiSend();
//  lightPage2();
//  digitalWrite(12, HIGH);   // turn the LED on (HIGH is the voltage level)
//  delay(1000);                       // wait for a second
//  digitalWrite(12, LOW);    // turn the LED off by making the voltage LOW
//  delay(1000);        
}


/*
 * ----------------------------------------
 * 
 * Input related declarations and functions
 * 
 * ----------------------------------------
 */


static const bool switchDown = LOW; // because we are using pullup resistor and switch to GND
static const byte debounceTime = 20; // after processing a button press, ignore further input for some milliseconds
static const byte debounceDelay = 5; // amount of time to wait before retesting debounceTime
static const int longPressTime = 1000; // how long a switch has to be held to count as a long press
static int switchPressedCounter = 0; // how many switches are currently pressed
static byte nextCommand = -1; // most important pending action - the switch that was last pressed, or other command via multi or long press
static byte lastCommand = -1; // last command sent (used for display confirmation)
//static byte command = 0; // adding this to workout long presses
//static byte last_command = -1;
static unsigned long commandMillis = millis(); // the time that nextCommand was last set - ie the last switch to be pressed
static const byte pageDnCmd = 5*switchCount + 1;
static const byte pageUpCmd = 5*switchCount + 2; 
static const byte pagePatchReset = 5*switchCount + 3; 
static const byte tunerCmd =  5*switchCount + 4;
bool long_press = false;
static byte currentPage = 0; // the current page / bank to be displayed

static const byte pageCount =1; // how many pages we have configured

void readButtons() {  
  switchPressedCounter = 0;
  long_press = false;
  for (int i=0;i<switchCount;i++) {
    switchPressed[i] = ( digitalRead(switchPin[i]) == switchDown ); // set array element to true if switch is currently pressed, or false if not
    if (switchPressed[i] != switchLastState[i]) { //potential press or release detected
      if (switchPressed[i]) { // potential new press detected
        if ( millis() > (lastPressMillis[i] + debounceTime) ) { // genuine press and not switch bounce
          lastPressMillis[i] = millis();
          switchLastState[i] = true;
          nextCommand = i;
          commandMillis = millis();          
        }
      }
      else if (!switchPressed[i]) { //potential switch release detected
        if ( millis() > (lastReleaseMillis[i] + debounceTime ) ) { // genuine release and not switch bounce
          lastReleaseMillis[i] = millis();
          switchLastState[i] = false;
        }
      }
    }
    if (switchPressed[i]) {
      switchPressedCounter++;  //increment counter used to check multiple presses      
      if (  millis() > (lastPressMillis[i] + longPressTime)  ) { // long press detected
        long_press = true;
        lastPressMillis[i] = millis(); // reset timer so it doesn't re-trigger every loop
        nextCommand = i + switchCount; // use the next n numbers as a second bank of commands representing long press actions        
      }
    }
  }
  static bool comboActive = false; // remembers whether multiple presses were detected to avoid re-triggering every loop
  if (switchPressedCounter > 1 ) { // multiple presses detected
    if (!comboActive) {
      comboActive = true;
      if ( switchPressed[0] && switchPressed[1]) { // first two switches -> Page Down
        nextCommand = pageDnCmd;
        changePageDown();
        }
      else if ( switchPressed[1] && switchPressed[2]) { // second two switches -> Page Up
        nextCommand = pageUpCmd;
        changePageUp();
        }
      else if ( switchPressed[2] && switchPressed[3]) { // 3rd 2 switches -> tuner
        nextCommand = tunerCmd;
        }
        
          
      else if ( switchPressed[6] && switchPressed[7]) { // last two switches - reset to page 0 and patch 0
        nextCommand = pagePatchReset;
        currentPage = 0;
      }
      }
    }
  else {
    comboActive = false; // we can reset this as no more than one switch currently pressed
  }
  lastCommand = nextCommand;
} // end of read_buttons()

void changePageUp() {
  currentPage++;
  if (currentPage >= pageCount) { // we have gone past the last page
    currentPage = 0; // reset to first page
  }
}

void changePageDown() {
  currentPage--;
  if (currentPage > pageCount) { // we have scrolled back past the first page
    currentPage = (pageCount -1); // reset to last page
  }
}

void ledUpdate(void){
  digitalWrite(12,LOW);
  switch(currentPage){
    case 0:
      digitalWrite(12, LOW);
    case 1:
      digitalWrite(12,HIGH);
  }
}


/*
 * 
 * Display related functions
 * 
 */


void invSelection(int i=(lastCommand+1)) {
  if (lastCommand == i) { // highlight the last switch pressed by inverting the colours
    display.setTextColor(BLACK, WHITE);
  }
  else {
    display.setTextColor(WHITE, BLACK);
  }
}


void displayLine(const __FlashStringHelper *str0, const __FlashStringHelper *str1, const __FlashStringHelper *str2, const __FlashStringHelper *str3, int startBtn) {
display.print(F("|"));
invSelection(0+startBtn);
display.print(str0);
invSelection();
display.print(F("|"));
invSelection(1+startBtn);
display.print(str1);
invSelection();
display.print(F("|"));
invSelection(2+startBtn);
display.print(str2);
invSelection();
display.print(F("|"));
invSelection(3+startBtn);
display.print(str3);
invSelection();
display.println(F("|"));
}


void displayUpdate(void) { // maybe change this to put labels in arrays, but this will do for now
  display.clearDisplay();
  display.setCursor(0, 0);     // Start at top-left corner
  switch (currentPage) {
    case 0:
      displayLine(F("SNAP"),F("SNAP"),F("SNAP"),F("FS5 "),4);
      displayLine(F("SHOT"),F("SHOT"),F("SHOT"),F(" /  "),4);
      displayLine(F(" 1  "),F(" 2  "),F(" 3  "),F(" UP "),4);   
      display.println(F("---------------------"));
      displayLine(F("    "),F("    "),F("    "),F("FS4 "),0);
      displayLine(F("FS1 "),F("FS2 "),F("FS3 "),F(" /  "),0);
      displayLine(F("    "),F("    "),F("    "),F("DOWN"),0);
      break;
   
    case 1:
      displayLine(F("FS4 "),F("FS5 "),F("TAP "),F("    "),4);
      displayLine(F(" /  "),F(" /  "),F("TMPO"),F("TUNE"),4);
      displayLine(F("DOWN"),F(" UP "),F("    "),F("    "),4);
      display.println(F("---------------------"));
      displayLine(F("    "),F("    "),F("    "),F("NEXT"),0);
      displayLine(F("FS1 "),F("FS2 "),F("FS3 "),F(" FS "),0);
      displayLine(F("    "),F("    "),F("    "),F("MODE"),0);   
      break;
 
    }
  display.display();
}

/*
 * 
 * MIDI output related functions
 * 
 */

  
void midiSend() {
  // do something
  if (nextCommand >=0) {
    if (nextCommand == pagePatchReset) { // SW7 & SW8 should reset page and patch to 0 regardless of which page/patch currently active
      MIDI.sendProgramChange(0,midiChannel);
    }
    else if (nextCommand == tunerCmd) {
    MIDI.sendControlChange(68,68,midiChannel); //tuner
        }
    else {
    switch(currentPage) {
      case 0: // menu page 0 (1 of 2)
       switch(nextCommand) {
        case 4:
          MIDI.sendControlChange(snapShots,snapShot1,midiChannel); // snapshot 1
          break;
        case 3:
          MIDI.sendControlChange(snapShots,snapShot2,midiChannel); // snapshot 2
          break;
        case 2:
          MIDI.sendControlChange(snapShots,snapShot3,midiChannel); // snapshot 3
          break;
        case 1:
          MIDI.sendControlChange(51,ccOff,midiChannel); //FS3
          break;
        case 0:
          MIDI.sendControlChange(52,ccOff,midiChannel); // FS4
          break;
        case 5:
          MIDI.sendControlChange(60,ccOn,midiChannel); //looper record
          break;
        case 6:
          MIDI.sendControlChange(61,ccOn,midiChannel); // looper play
          break;        
        case 7:
          MIDI.sendControlChange(62,ccOn,midiChannel); //looper play once
          break;
        case 8:
          MIDI.sendControlChange(60,ccOff,midiChannel); //looper overdub
          break;    
        case 9:
          MIDI.sendControlChange(63,ccOn,midiChannel); // looper undo/redo
          break;   
        case 10:
          MIDI.sendControlChange(71,ccOff,midiChannel); // stomp mode
          break;
        case 11:
          MIDI.sendControlChange(71,1,midiChannel); // Scroll mode
          break;
        case 12:
          MIDI.sendControlChange(71,4,midiChannel); // next mode
          break;
        case 13:
          MIDI.sendControlChange(53,ccOff,midiChannel); // FS5
          break;
        case 14:
          MIDI.sendProgramChange(101,midiChannel);// preset 101
          break;
        case 15:
          MIDI.sendProgramChange(1,midiChannel);
        case 16:
          MIDI.sendProgramChange(2,midiChannel);
        case 17:
          MIDI.sendControlChange(61,ccOff,midiChannel); // looper stop
        case 18:
          changePageUp();
        case 19:
          changePageDown();
        } // end of menu page 0
        break;

        break;
            
        break;
    } // end of outer nested switch case
}      
    nextCommand = -1; // re-initialise this so we don't send the same message repeatedly
  }
} // end midisend
