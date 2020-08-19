
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


// error blink codes
#define ERR_DISP_ALLOC 3 // display allocation error

static const unsigned ledPin = LED_BUILTIN; // use onboard LED as activity indicator
static const byte switchPin[] = {2,3,4,5,6,7,8,9,10,11}; // pins for footswitch inputs
static const byte switchCount = 10; // number of footswitches used
static bool switchPressed[switchCount]; // current state of footswitches
static bool switchLastState[switchCount]; //previous state of footswitches (used for long press detection)
static unsigned long lastPressMillis[switchCount]; // when the last button press was detected
static unsigned long lastReleaseMillis[switchCount]; // when the last button was released
int page2led = 13;


// Created and binds the MIDI interface to the default hardware Serial port
MIDI_CREATE_DEFAULT_INSTANCE();


void errBlink(int errCode) {
  byte blinkTime = 200; // duration for each blink
  byte blinkGap = 200; // gap between each blink
  int burstWait = 1000; // wait time between bursts 
  for (;;) { // loop forever
    for (int i = 1; i <= errCode; i++) {
      digitalWrite(ledPin,HIGH);
      delay(blinkTime);
      digitalWrite(ledPin,LOW);
      delay(blinkGap);
    }
    delay(burstWait);
  }
} // end of errBlink()


void setup() {
  pinMode(ledPin, OUTPUT);  // setup activity LED pin for output

  MIDI.begin(MIDI_CHANNEL_OMNI);  // Listen to all incoming messages
  

  delay(500); // Pause for 0.5 seconds

  
  // Initialise switches and related variable arrays
  for (int i=0;i<switchCount;i++) { 
    pinMode(switchPin[i], INPUT_PULLUP); // add pullup resistors to all footswitch input pins
    switchPressed[i] = false; //initialise switch state
    switchLastState[i] = false; //initialse last switch state
    lastReleaseMillis[i] = millis(); // initialise time switch last released
    lastPressMillis[i] = lastPressMillis[i] -1; // initialise time switch last pressed
  }

} // end of setup

//int command = 0;
//int last_command = -1;
//unsigned long now;

void loop() {
  readButtons();
  midiSend();
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
    digitalWrite(13,LOW);
  }
}

void changePageDown() {
  currentPage--;
  if (currentPage > pageCount) { // we have scrolled back past the first page
    currentPage = (pageCount -1); // reset to last page
    digitalWrite(13,HIGH);
  }
}




/*
 * 
 * MIDI output related functions
 * 
 */

//void sendCommand(int _com) {
//  sendCommandPage1(_com);
//}


//void sendCommandPage1(int _com) {
  
void midiSend() {
  // do something
  if (nextCommand >=0) {
    if (nextCommand == pagePatchReset) { // SW7 & SW8 should reset page and patch to 0 regardless of which page/patch currently active
      MIDI.sendProgramChange(0,1);
    }
    else if(long_press) {
//      if(_com == 1){
      MIDI.sendControlChange(71,0,1); // Stomp mode
//      }
    }
    else if (nextCommand == tunerCmd) {
    MIDI.sendControlChange(68,68,1); //tuner
        }

    else {
    switch(currentPage) {
      case 0: // menu page 0 (1 of 2)
       switch(nextCommand) {
        case 7:
          MIDI.sendControlChange(49,0,1); //FS1
          break;
        case 6:
          MIDI.sendControlChange(50,0,1); //FS2
          break;
        case 5:
          MIDI.sendControlChange(51,0,1); //FS3
          break;
        case 4:
          MIDI.sendControlChange(52,0,1); //FS4
          break;
        case 3:
          MIDI.sendControlChange(69,0,1); //snapshot 1
          break;
        case 2:
          MIDI.sendControlChange(69,1,1); // snapshot 2
          break;
        case 1:
          MIDI.sendControlChange(69,2,1); //snapshot 3
          break;
        case 0:
          MIDI.sendControlChange(53,0,1); //FS5
          break;        
        
        } // end of menu page 0
        break;
      case 1: // menu page 1 (2 of 2)
       switch(nextCommand) {
        case 0:
          // -> FS1
          MIDI.sendControlChange(49,0,1); //FS1
          break;
        case 1:
          // -> FS2
          MIDI.sendControlChange(50,0,1); //FS2
          break;
        case 2:
          // -> FS3
          MIDI.sendControlChange(51,0,1); //FS3
          break;
        case 3:
          // -> PRESET MODE PAGE
          MIDI.sendControlChange(71,4,1); //next footswitch mode (temp functionality until I work out if I can change currentPage while in switch block)
          break;
        case 4:
          MIDI.sendControlChange(52,0,1); //FS4
          break;
        case 5:
          MIDI.sendControlChange(53,0,1); //FS5
          break;
        case 6:
          MIDI.sendControlChange(64,64,1); //tap tempo
          break;
        case 7:
          MIDI.sendControlChange(68,68,1); //tuner
          break;
        } // end of menu page 1
        break;
      
      
        break;
    } // end of outer nested switch case
}      
    nextCommand = -1; // re-initialise this so we don't send the same message repeatedly
  }
} // end midisend
