
/*
 * ---------------------------------------------------
 * 
 * Simple 10 button MIDI controller for Line 6 HX Stomp using Nano clone
 * 
 * MIDI connectivity based on https://www.instructables.com/id/Send-and-Receive-MIDI-with-Arduino/
 * MIDI pin 4 to TX1
 * 
 * 
 * 10x momentary SPST switches connected between GND and D2 - D11
 * 
 * ---------------------------------------------------
 */



#include <MIDI.h>

// error blink codes
#define ERR_DISP_ALLOC 3 // display allocation error

static const unsigned ledPin = LED_BUILTIN; // use onboard LED as activity indicator
static const unsigned looperLed = 12;
static const byte switchPin[] = {2,3,4,5,6,7,8,9,10,11}; // pins for footswitch inputs
static const byte switchCount = 10; // number of footswitches used
static bool switchPressed[switchCount]; // current state of footswitches
static bool switchLastState[switchCount]; //previous state of footswitches (used for long press detection)
static unsigned long lastPressMillis[switchCount]; // when the last button press was detected
static unsigned long lastReleaseMillis[switchCount]; // when the last button was released



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


void setup() {
  pinMode(ledPin, OUTPUT);  // setup activity LED pin for output
  pinMode(looperLed,OUTPUT);
  digitalWrite(looperLed,LOW);
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
//      if ( switchPressed[0] && switchPressed[1]) { // first two switches -> Page Down
//        nextCommand = pageDnCmd;
//        changePageDown();
//        }
//      if ( switchPressed[0] && switchPressed[1]) { // first two switches -> Page Down
//        nextCommand = pageDnCmd;
//        changePageDown();
//        }
//      else if ( switchPressed[1] && switchPressed[2]) { // second two switches -> Page Up
//        nextCommand = pageUpCmd;
//        changePageUp();
//        }
//      else 
      if ( switchPressed[2] && switchPressed[3]) { // 3rd 2 switches -> tuner
        nextCommand = tunerCmd;
        }
        
          
      else if ( switchPressed[8] && switchPressed[9]) { // last two switches - reset to page 0 and patch 0
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
//
//void changePageUp() {
//  currentPage++;
//  if (currentPage >= pageCount) { // we have gone past the last page
//    currentPage = 0; // reset to first page
//  }
//}
//
//void changePageDown() {
//  currentPage--;
//  if (currentPage > pageCount) { // we have scrolled back past the first page
//    currentPage = (pageCount -1); // reset to last page
//  }
//}

void changePage(){
  if (currentPage = 1){
    currentPage = 2};
    digitalWrite(looperLed, HIGH);
  else
    (currentPage = 1};
    digitalWrite(looperLed,LOW);
  }
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
      MIDI.sendProgramChange(0,1);
    }
    else if (nextCommand == tunerCmd) {
    MIDI.sendControlChange(68,68,1); //tuner
        }
    else {
    switch(currentPage) {
      case 0: // menu page 0 (1 of 2)
       switch(nextCommand) {
        case 4:
          MIDI.sendControlChange(69,0,1); // snapshot 1
          break;
        case 3:
          MIDI.sendControlChange(69,1,1); // snapshot 2
          break;
        case 2:
          MIDI.sendControlChange(69,2,1); // snapshot 3
          break;
        case 1:
          MIDI.sendControlChange(51,0,1); //FS3
          break;
        case 0:
          MIDI.sendControlChange(52,0,1); // FS4
          break;
        case 5:
          MIDI.sendControlChange(60,127,1); //looper record
          break;
        case 6:
          MIDI.sendControlChange(61,127,1); // looper play
          break;        
        case 7:
          MIDI.sendControlChange(62,127,1); //looper play once
          break;
        case 8:
          MIDI.sendControlChange(60,0,1); //looper overdub
          break;    
        case 9:
          MIDI.sendControlChange(63,127,1); // looper undo/redo
          break;   
        case 10:
          MIDI.sendControlChange(71,0,1); // stomp mode
          break;
        case 11:
          MIDI.sendControlChange(71,1,1); // Scroll mode
          break;
        case 12:
          MIDI.sendControlChange(71,4,1); // next mode
          break;
        case 13:
          MIDI.sendControlChange(53,0,1); // FS5
          break;
        case 14:
          MIDI.sendProgramChange(101,1);// preset 101
          break;
        case 15:
          MIDI.sendProgramChange(1,1);
        case 16:
          MIDI.sendProgramChange(2,1);
        case 17:
          MIDI.sendControlChange(61,0,1); // looper stop
        case 18:
          changePage();
        case 19:
          changePage();
        } // end of menu page 0
        break;
//      case 1: // menu page 0 (2 of 2)
//       switch(nextCommand) {
//        case 0:
//          MIDI.sendControlChange(52,0,1); // FS4
//          break;
//        case 1:
//          MIDI.sendControlChange(51,0,1); //FS3
//          break;
//        case 2:
//          MIDI.sendControlChange(69,2,1); // snapshot 3
//          break;
//        case 3:
//          MIDI.sendControlChange(69,1,1); // snapshot 2
//          break;
//        case 4:
//          MIDI.sendControlChange(69,0,1); // snapshot 1
//          break;  
//        case 5:
//          MIDI.sendControlChange(60,127,0); //looper record
//          break;
//        case 6:
//          MIDI.sendControlChange(61,127,1); // looper play
//          break;        
//        case 7:
//          MIDI.sendControlChange(62,127,1); //looper play once
//          break;
//        case 8:
//          MIDI.sendControlChange(60,0,1); //looper overdub
//          break;    
//        case 9:
//          MIDI.sendControlChange(63,127,1); // looper undo/redo
//          break;   
//        case 10:
//          MIDI.sendControlChange(71,0,1); // stomp mode
//          break;
//        case 11:
//          MIDI.sendControlChange(71,1,1); // Scroll mode
//          break;
//        case 12:
//          MIDI.sendControlChange(71,4,1); // next mode
//          break;
//        case 13:
//          MIDI.sendControlChange(53,0,1); // FS5
//          break;
//        case 14:
//          MIDI.sendProgramChange(101,1);// preset 101
//          break;
//        case 15:
//          MIDI.sendProgramChange(1,1);
//        case 16:
//          MIDI.sendProgramChange(2,1);
//        case 17:
//          MIDI.sendControlChange(61,0,1); // looper stop
//        case 18:
//          changePageUp();
//        case 19:
//          changePageDown();
//        } // end of menu page 0
        break;
            
        break;
    } // end of outer nested switch case
}      
    nextCommand = -1; // re-initialise this so we don't send the same message repeatedly
  }
} // end midisend
