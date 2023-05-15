
// Copyright 2023 Rich Heslip
//
// Author: Rich Heslip 
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
/*
 * MIDI sequencer using RP2040
 * uses 16 multiplexed encoders for step data input
 * a separate encoder for menu inputs
 * SPI OLED display
 * R Heslip April 2023  
 * Graphics and text UI is processed on core 0, sequencer clocking and MIDI processed by core 1
 * 5/12/2023 - moved all the MidiUSB processing to core 1 - running MidiUSB.read() and handlers on core 0 was causing lockups
 * 5/13/23 - added display of note, gate length etc to top line
 */


#include <Arduino.h>
#include "RPi_Pico_TimerInterrupt.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <SH1106.h>
//#include <Adafruit_S6D02A1.h> // Hardware-specific library for S6D02A1
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include "Clickencoder.h"
//#include "StepSeq.h"



#define TRUE 1
#define FALSE 0

#define NTRACKS 4 // number of sequencer tracks - each track has notes, gates etc

// I used mostly int16 types - its what the menu requires and the compiler seems to deal with basic conversions

//int16_t steps = 8; // initial number of steps

bool startbutton;  // start/stop button state
bool running=true; // sequencer running flag
bool shift =0;

// graphical editing UI states - UI logic assumes an init state and an edit state for each
// text parameter editing system has its own state machine for historical reasons
// the text menu system requires parameters to be 16 bit integers which is why most of the data types are int16

enum UISTATES {NOTE_DRAW,NOTE_EDIT,GATE_DRAW,GATE_EDIT,VELOCITY_DRAW,VELOCITY_EDIT,OFFSET_DRAW,OFFSET_EDIT,PROBABILITY_DRAW,PROBABILITY_EDIT,RATCHET_DRAW,RATCHET_EDIT,MOD_DRAW,MOD_EDIT,DISPLAYOFF,DORMANT};
// initial states on each page
int16_t UIpages[] = {NOTE_DRAW,GATE_DRAW,VELOCITY_DRAW,OFFSET_DRAW,PROBABILITY_DRAW,RATCHET_DRAW,MOD_DRAW};
int16_t UIpage=0;
#define NUMUIPAGES sizeof(UIpages)/sizeof(int16_t)
bool menumode=0;  // when true we are in the text menu system
int16_t UI_state=NOTE_DRAW; // initial UI state

int16_t current_track=0; // track we are editing
int16_t MIDIchannel[NTRACKS] = {1,2,3,4}; // midi channel to use for sequencer notes
int16_t trackenabled[NTRACKS] = {1,0,0,0}; // 1 if track on is 1, 0 if off
int16_t CCchannel[NTRACKS] = {1,2,3,4}; // midi channel to use for CCs
int16_t mod_enabled[NTRACKS] = {0,0,0,0}; // 1 if mod sequencer for track is on, 0 if off

#define DISPLAY_BLANK_MS 120*1000  // display blanking time
int32_t displaytimer ; // display blanking timer

#define TEMPO    120
#define PPQN 24  // clocks per quarter note
int16_t bpm = TEMPO;
int32_t lastMIDIclock; // timestamp of last MIDI clock
int16_t MIDIclocks=PPQN*2; // midi clock counter
int16_t MIDIsync = 16;  // number of clocks required to sync BPM
int16_t useMIDIclock = 0; // true if we are using MIDI clock

enum CONTROLSTATES {IDLE,STARTUP,RUNNING,RUNJUSTSYNCED,SHUTDOWN}; // control state machine states
int16_t controlstate=0; // state machine state
#define DEBOUNCE_COUNT 8; // debounce time for buttons in timer interrupt periods
uint8_t startbut_count;
uint8_t shiftbut_count;

// sequencer object
//StepSeq seq = StepSeq(128);

const char * notenames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B","C"};

#define NENC 16 // number of encoders

// I/O definitions
#define A_MUX_0 6   // CD4067 address lines
#define A_MUX_1 7
#define A_MUX_2 8
#define A_MUX_3 9

#define ENCA_IN 15 // ports we read values from MUX
#define ENCB_IN 22
#define ENCSW_IN 14

#define MENU_ENCA_IN 2 // menu encoder
#define MENU_ENCB_IN 3
#define MENU_ENCSW_IN 4

#define START_STOP_BUTTON 5  // start/stop button
#define SHIFT_BUTTON 28      // UI function shift button

// encoders - use an array of clickencoder objects for the 16 multiplexed encoders
#define ENCDIVIDE 4  // divide by 4 works best with my encoders
ClickEncoder enc[NENC] = {
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE),
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE),
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE),
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE), 
  ClickEncoder(ENCA_IN,ENCB_IN,ENCSW_IN,ENCDIVIDE)
};

// encoder aliases for my Teensy DSP menu system
#define P0  10   
#define P1  11
#define P2  12
#define P3  13


ClickEncoder menuenc(MENU_ENCA_IN,MENU_ENCB_IN,MENU_ENCSW_IN,ENCDIVIDE); // menu encoder object

#define OLED_DISPLAY   // for graphics conditionals

//#define TFT18_DISPLAY  // use 1.8" TFT banggood display

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
//#define SCREEN_WIDTH 160 // OLED display width, in pixels
//#define SCREEN_HEIGHT 128 // OLED display height, in pixels
#define SCREEN_BUFFER_SIZE (SCREEN_WIDTH * ((SCREEN_HEIGHT + 7) / 8))
//#define WHITE S6D02A1_WHITE
//#define BLACK S6D02A1_BLACK

// Uncomment this block to use hardware SPI
#define OLED_DC     21
#define OLED_CS     17
#define OLED_RESET  20
#define TFT_DC     21
#define TFT_CS     17
#define TFT_RESET  20

Adafruit_SH1106 display(OLED_DC, OLED_RESET, OLED_CS);
//Adafruit_S6D02A1 display = Adafruit_S6D02A1(TFT_CS, TFT_DC, TFT_RESET);



#define TIMER_MICROS 1000 // interrupt period
// Init RPI_PICO_Timer
RPI_PICO_Timer ITimer(0);

// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create a new instance of the Arduino MIDI Library,
// and attach usb_midi as the transport.
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MidiUSB);


// timer interrupt handler
// scans thru the multiplexed encoders and handles the menu encoder
// debounces the buttons

bool TimerHandler0(struct repeating_timer *t)
{
  (void) t;

  for (int addr=0; addr< NENC;++addr) {
    digitalWrite(A_MUX_0, addr & 1);
    digitalWrite(A_MUX_1, addr & 2);
    digitalWrite(A_MUX_2, addr & 4);
    digitalWrite(A_MUX_3, addr & 8); 
    delayMicroseconds(3);         // address settling time 
    enc[addr].service();    // check the encoder inputs
  } 
  menuenc.service(); // handle the menu encoder which is on different port pins
  // debounce the buttons
  if (!(digitalRead(START_STOP_BUTTON))) { 
    if (startbut_count ==0) startbutton=TRUE;
    else --startbut_count;
  }
  else {
    startbutton=FALSE;
    startbut_count=DEBOUNCE_COUNT;
  }
  if (!(digitalRead(SHIFT_BUTTON))) { 
    if (shiftbut_count ==0) shift=TRUE;
    else --shiftbut_count;
  }
  else {
    shift=FALSE;
    shiftbut_count=DEBOUNCE_COUNT;
  }
  return true; // required by the timer lib
}

/*
*** unfortunately the SPI display drivers do not use a framebuffer so this blanking method will not work
// turn off the display to avoid burn in but keep the display buffer intact
// the next call to display.display() will automagically restore what was there plus whatver has been drawn since

void blankdisplay(void) {
  uint8_t tempbuf[SCREEN_BUFFER_SIZE];
  uint8_t * buf, * tbuf;
  buf=display.getBuffer();
  tbuf= tempbuf;
  memcpy(tbuf,buf,SCREEN_BUFFER_SIZE); // copy frame buffer to temp storage
  display.clearDisplay();
  display.display();  // turn off the OLEDs
  memcpy(buf,tbuf,SCREEN_BUFFER_SIZE); // restore the old frame buffer
}

// update the display and reset the display blanking timer
void displayupdate(){
  display.display();
  displaytimer=millis();
}
*/

// midi related stuff - after initialization all MIDI stuff runs on core1 for timing accuracy
// splitting it across both cores causes MidiUSB to hang eventually

// note that the Adafruit stack expects MIDI channel to be 1-16, not 0-15
void noteOn(byte channel, byte pitch, byte velocity) {
  MidiUSB.sendNoteOn(pitch,velocity,channel+1);
//  Serial.printf("Noteon ch %d pitch %d vel %d \n",channel,pitch,velocity);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  MidiUSB.sendNoteOff(pitch,velocity,channel+1);
//  Serial.printf("Noteoff ch %d pitch %d vel %d \n",channel,pitch,velocity);
}

// First parameter is the event type (0x0B = control change).
// Second parameter is the event type, combined with the channel.
// Third parameter is the control number number (0-119).
// Fourth parameter is the control value (0-127).

void controlChange(byte channel, byte control, byte value) {
  MidiUSB.sendControlChange(control,value,channel+1);
}


// set up as include files because I'm too lazy to create proper header and .cpp files
#include "scales.h"   //
#include "seq.h"   // has to come after midi note on/of
#include "menusystem.h"  // has to come after display and encoder objects creation
#include "graphics.h"   // has to come after display object creation

// these functions are here to avoid forward references. should really do proper include files!
// there is some risk in processing MIDI start/stop etc on on core 0 since core 1 could also be using MIDI
// core 0 syncing sequencers while core 1 is doing clocks is a bit dicey
// idling core 1 when using these resources should work

// process MIDI start message - start playing from beginning 
void handleStart(void){
  all_notes_off();  // in case notes are already playing
  sync_sequencers(); // sync all sequencers 
  MIDIsync=16; // sync BPM again
  rp2040.idleOtherCore();  // so core 1 doesn't also modify state machine
  controlstate=RUNNING; // force core 1 to playing state
  rp2040.resumeOtherCore();
}

// process MIDI clock messages
// count MIDI clocks for a while to get a decent average and then compute BPM from it
// if external MIDI clock is enabled use it as the master clock
void handleClock(void){
  long qn,clockperiod;
  clockperiod= (long)(((60.0/(float)bpm)/PPQN)*1000); // for call to clocktick(). use calculated BPM which is more stable - MIDI clock has a lot of jitter
  --MIDIclocks;
  if (MIDIclocks ==0 ) {
    MIDIclocks=PPQN*2;
    qn=millis()-lastMIDIclock;
    lastMIDIclock=millis();
    if (MIDIsync >0) --MIDIsync;
    if (MIDIsync ==0) {
      if ((qn < 6200) && (qn > 480))  bpm=2*60.0*1000/qn; // check that clock value is between 20 and 240BPM so we don't trash the current bpm
    }
//    Serial.printf("%d %d\n",qn,bpm);
  }
  if (useMIDIclock) clocktick(clockperiod); 
}

// process MIDI stop message - stop playing
void handleStop(void){
  all_notes_off();  // so notes don't hang
  rp2040.idleOtherCore();  // so core 1 doesn't also modify state machine
  controlstate=IDLE; // put core 1 in idle state machine state
  rp2040.resumeOtherCore();
}

// process MIDI continue message - continue playing
void handleContinue(void){
  rp2040.idleOtherCore();  // so core 1 doesn't also modify state machine
  controlstate=RUNNING; // put core 1 in playing state
  rp2040.resumeOtherCore(); 
}

void setup() {
  Serial.begin(115200);

  pinMode(A_MUX_0, OUTPUT);    // encoder mux addresses
  pinMode(A_MUX_1, OUTPUT);  
  pinMode(A_MUX_2, OUTPUT);  
  pinMode(A_MUX_3, OUTPUT);
  pinMode(ENCA_IN, INPUT_PULLUP);  // menu encoder and switches
  pinMode(ENCB_IN, INPUT_PULLUP);    
  pinMode(ENCSW_IN, INPUT_PULLUP); 
  pinMode(START_STOP_BUTTON, INPUT_PULLUP);
  pinMode(SHIFT_BUTTON, INPUT_PULLUP);

 // setRX(pin_size_t pin);  // assign RP2040 SPI0 pins
  SPI.setCS(17);
  SPI.setSCK(18);
  SPI.setTX(19);



// set up timer interrupt 
  // Interval in unsigned long microseconds
  if (ITimer.attachInterruptInterval(TIMER_MICROS, TimerHandler0))
    Serial.println("Started ITimer OK");
  else {
    Serial.println("Can't set ITimer. Select another freq. or timer");
  } 
   
#if defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040)
  // Manual begin() is required on core without built-in support for TinyUSB such as mbed rp2040
  TinyUSB_Device_Init(0);
#endif
  // Initialize MIDI, and listen to all MIDI channels
  // This will also call usb_midi's begin()
  MidiUSB.begin(MIDI_CHANNEL_OMNI);

  // attach MIDI message handler functions
//  MIDI.setHandleNoteOn(handlenoteOn);
//  MIDI.setHandleNoteOff(handlenoteOff);

  MidiUSB.setHandleClock(handleClock);
  MidiUSB.setHandleStop(handleStop);
  MidiUSB.setHandleStart(handleStart);
  MidiUSB.setHandleContinue(handleContinue);

  // wait until device mounted
  while( !TinyUSBDevice.mounted() ) delay(1);

  display.begin(SH1106_SWITCHCAPVCC);
   // Use this initializer if using a 1.8" TFT screen:
//  display.initR(INITR_BLACKTAB);      // Init TFT, black tab

  display.fillScreen(BLACK);
  display.setRotation(2);  // mounted upside down
// text display tests

	display.setTextColor(WHITE,BLACK); // foreground, background  
  display.setCursor(0,30);
  display.println("   Pico Sequencer");
#ifdef OLED_DISPLAY
  display.display();
#endif
  delay(3000);

  display.fillScreen(BLACK);
  displaytimer=millis(); // reset display blanking timer
/*
   // start sequencer and set callbacks
  seq.begin(TEMPO, steps);
  seq.setMidiHandler(handleSequencerMIDI);
  seq.setStepHandler(handleSequencerStep);
  seq.start();
  */
}

// first Pico core does UI etc - not super time critical
void loop() {
  
  ClickEncoder::Button button;
  int16_t encvalue,edited_step,edited_val;

  if ((millis()-displaytimer) > DISPLAY_BLANK_MS) {
    UI_state=DISPLAYOFF;
  } 
/*
  if ((millis()-displaytimer) > 500) { // debug printing
    // Serial.printf("step= %d val=%d \n",edited_step,edited_val);
    displaytimer=millis();
  }
*/

  if (shift && !menumode) { // enter menu mode
    display.fillScreen(BLACK); // erase screen
    topmenuindex=UIpage*NTRACKS+current_track; // link text menus to graphics page
    drawtopmenu(topmenuindex); // repaint the menu for the current sequencer
    drawsubmenus();
    menumode=TRUE; // shift button toggles onscreen menus
  }

  if (!shift && menumode) { // exit menu mode
    UI_state=UIpages[UIpage]; // exiting text menus so force a redraw
    menumode=FALSE;
  }


  if (menumode) {
    domenus();  // call the text menu state machine
    displaytimer=millis(); // reset display blanking timer
  }
  else {

// UI state machine- main encoder scrolls thru the sequencer graphic pages
    switch (UI_state) {
      case NOTE_DRAW:
        drawheader("Note");
        drawnotes(notes[current_track]);
        drawindex(notes[current_track].index);
        UI_state=NOTE_EDIT;
        break;
      case NOTE_EDIT:
        edited_step=editnotes(&notes[current_track]); // must call by reference to change the structure values
        if (edited_step) {  // show the note index, degree in scale, note name and octave
          edited_val=notes[current_track].val[edited_step-1];
          int16_t nameindex=constrain(edited_val+notes[current_track].root,0,127)%12;
          int16_t octave=constrain(edited_val+notes[current_track].root,0,127)/12;
          display.setCursor(6*6,0);  // display which note was changed
          display.printf(":%d %d %s%d    \n",edited_step,edited_val,notenames[nameindex],octave); 
          display.display();
          displaytimer=millis(); // reset display blanking timer
        }
        updateindex(notes[current_track]); // show the index on screen
        break;

      case GATE_DRAW:
        drawheader("Gate");
        drawbars(gates[current_track]);
        drawindex(gates[current_track].index);
        UI_state=GATE_EDIT;
        break;
      case GATE_EDIT:
        edited_step=editbars(&gates[current_track]);
        if (edited_step) {  // show the gate value
          edited_val=gates[current_track].val[edited_step-1];
          display.setCursor(6*6,0);  
          display.printf(":%d %d%%  ",edited_step,edited_val*100/GATERANGE); 
          display.display();
          displaytimer=millis(); // reset display blanking timer
        }         
        updateindex(gates[current_track]); // show the index on screen
        break;  

      case VELOCITY_DRAW:
        drawheader("Velocity");
        drawbars(velocities[current_track]);
        drawindex(velocities[current_track].index);
        UI_state=VELOCITY_EDIT;
        break;
      case VELOCITY_EDIT:
        edited_step=editbars(&velocities[current_track]);
        if (edited_step) {  // show the velocity value
          edited_val=velocities[current_track].val[edited_step-1];
          display.setCursor(10*6,0);  
          display.printf(":%d %d%% ",edited_step,edited_val*100/VELOCITYRANGE); 
          display.display();
          displaytimer=millis(); // reset display blanking timer
        }  
        updateindex(velocities[current_track]); // show the index on screen
        break;  

      case OFFSET_DRAW:  // offsets added to the note sequence
        drawheader("Offset");
        drawnotes(offsets[current_track]);
        drawindex(offsets[current_track].index);
        UI_state=OFFSET_EDIT;
        break;
      case OFFSET_EDIT:
        edited_step=editnotes(&offsets[current_track]); // must call by reference to change the structure
        if (edited_step) {  // show the note index and degree in scale
          edited_val=offsets[current_track].val[edited_step-1];
          display.setCursor(8*6,0);  // display which note was changed
          display.printf(":%d %d  ",edited_step,edited_val); 
          display.display();
          displaytimer=millis(); // reset display blanking timer
        }        
        updateindex(offsets[current_track]); // show the index on screen
        break;

      case PROBABILITY_DRAW:
        drawheader("Probability");
        drawbars(probability[current_track]);
        drawindex(probability[current_track].index);
        UI_state=PROBABILITY_EDIT;
        break;
      case PROBABILITY_EDIT:
        edited_step=editbars(&probability[current_track]);
        if (edited_step) {  // show the probability
          edited_val=probability[current_track].val[edited_step-1];
          display.setCursor(13*6,0);  // display which note was changed
          display.printf(":%d %3d%%",edited_step,edited_val*100/PROBABILITYRANGE); 
          display.display();
          displaytimer=millis(); // reset display blanking timer
        } 
        updateindex(probability[current_track]); // show the index on screen
        break;  

      case RATCHET_DRAW:
        drawheader("Ratchets");
        drawbars(ratchets[current_track]);
        drawindex(ratchets[current_track].index);
        UI_state=RATCHET_EDIT;
        break;
      case RATCHET_EDIT:
        edited_step=editbars(&ratchets[current_track]);
        if (edited_step) {  // show the number of ratchets
          edited_val=ratchets[current_track].val[edited_step-1];
          display.setCursor(10*6,0);  
          display.printf(":%d %d   ",edited_step,edited_val); 
          display.display();
          displaytimer=millis(); // reset display blanking timer
        }        
        updateindex(ratchets[current_track]); // show the index on screen
        break; 

      case MOD_DRAW:
        drawheader("Mod");
        drawbars(mods[current_track]);
        drawindex(mods[current_track].index);
        UI_state=MOD_EDIT;
        break;
      case MOD_EDIT:
        edited_step=editbars(&mods[current_track]);
        if (edited_step) {  // show the number of ratchets
          edited_val=mods[current_track].val[edited_step-1];
          display.setCursor(5*6,0);  
          display.printf(":%d %d   ",edited_step,edited_val); 
          display.display();
          displaytimer=millis(); // reset display blanking timer
        }        
        updateindex(mods[current_track]); // show the index on screen
        break; 

      case DISPLAYOFF:
        display.fillScreen(BLACK); // protect OLED from burning in
        display.display(); 
        UI_state=DORMANT;
        break;
      
      case DORMANT:
        break; // turning menu encoder will start screen up again

      default:
        UI_state = UIpages[0];
    }

    button=menuenc.getButton();
    if (encvalue=menuenc.getValue()) { // scroll thru UI pages
      displaytimer=millis(); // reset display blanking timer
//      if (button == ClickEncoder::Closed) { // we are changing tracks
      if (!digitalRead(MENU_ENCSW_IN)) { // button value not working for some reason
        current_track+=encvalue;
        current_track=constrain(current_track,0,NTRACKS-1); // handle wrap around
        UI_state=UIpages[UIpage]; // forces redraw
      }
      else {
        UIpage+=encvalue;  // next page
        UIpage=constrain(UIpage,0,NUMUIPAGES-1); // handle wrap around
        UI_state=UIpages[UIpage];
      //Serial.printf("encvalue= %d UIpage=%d UI_state=%d\n",encvalue,UIpage,UI_state);
      }
    }
  }

}

// second core setup
// second core dedicated to clock and MIDI processing
void setup1() {
  delay (1000); // wait for main core to start up peripherals
}

// second core dedicated to clocks and note on/off for timing accuracy - graphical UI causes redraw delays etc
// implemented as a simple state machine
// start button toggles sequencers on and off
// shift + start button resyncs sequencers
void loop1(){
  MidiUSB.read(); // read any new MIDI messages
  switch (controlstate) {
    case IDLE:
      if (startbutton && shift) sync_sequencers(); // start all sequencers at beginning
      if (startbutton && !shift) controlstate= STARTUP;
      break;
    case STARTUP:
      if (!startbutton) { // don't do anything till startbutton is released
        controlstate=RUNNING;
      }
      break;
    case RUNNING:
      if (!useMIDIclock) do_clocks(); // clock the sequencers and handle notes
      if (startbutton && shift) { // we can sync the sequencers while its running
        sync_sequencers();
        controlstate=RUNJUSTSYNCED;
      }
      if (startbutton && !shift) { // stop the sequencers
        all_notes_off(); 
        controlstate= SHUTDOWN;
      }
      break;
    case RUNJUSTSYNCED: // just synced, wait for start button release
      if (!useMIDIclock) do_clocks(); // clock the sequencers and handle notes
      if (!startbutton) { // till startbutton is released
        controlstate=RUNNING;
      }
      break;
    case  SHUTDOWN:
      if (!startbutton) { // don't do anything till startbutton is released
        controlstate=IDLE;
      }
      break;
    default:
      controlstate=IDLE;
  }
}

