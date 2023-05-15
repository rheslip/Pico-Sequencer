
// Copyright 2020 Rich Heslip
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
//
// this is adapted from my XVA1 menus system. this one is bitmapped so its a mix of character and pixel addressing
// implements a two level horizontal scrolling menu system
// main encoder selects top level menu by scrolling left-right thru the top menus
// 4 parameter encoders modify values in the submenus
// press and hold parameter or main encoder to scroll submenus left-Right
// once you get the idea its very fast to navigate

#define DISPLAY_X 20  // 20 char display
#define DISPLAY_Y 8   // 8 lines
#define DISPLAY_CHAR_HEIGHT 8 // character height in pixels - for bitmap displays
#define DISPLAY_CHAR_WIDTH 6 // character width in pixels - for bitmap displays
#define DISPLAY_X_MENUPAD 2   // extra space between menu items
#define TOPMENU_Y 0   // line to display top menus
#define MSG_Y   30   // line to display messages
#define FILENAME_Y 15 // line to display filename
#define SUBMENU_FIELDS 4 // max number of subparameters on a line ie for 20 char display each field has 5 characters
#define SUBMENU_Y 46 // y pos to display submenus (parameter names)
#define SUBMENU_VALUE_Y 56 // y pos to display parameter values
uint8_t submenu_X[]= {0,32,64,96};  // location of the submenus by pixel

enum paramtype{TYPE_NONE,TYPE_INTEGER,TYPE_FLOAT, TYPE_TEXT}; // parameter display types

// submenus 
struct submenu {
  const char *name; // display short name
  const char *longname; // longer name displays on message line
  int16_t min;  // min value of parameter
  int16_t max;  // max value of parameter
  int16_t step; // step size. if 0, don't print ie spacer
  enum paramtype ptype; // how its displayed
  const char ** ptext;   // points to array of text for text display
  int16_t *parameter; // value to modify
  void (*handler)(void);  // function to call on value change
};

// top menus
struct menu {
   const char *name; // menu text
   struct submenu * submenus; // points to submenus for this menu
   int8_t submenuindex;   // stores the index of the submenu we are currently using
   int8_t numsubmenus; // number of submenus - not sure why this has to be int but it crashes otherwise. compiler bug?
};

// timer and flag for managing temporary messages
#define MESSAGE_TIMEOUT 1500
long messagetimer;
bool message_displayed;

int16_t enab1 =1;
int16_t enab2 =2;
int16_t level1,pan1;

int16_t nul;    // dummy parameter and function for testing
void dummy( void) {}

// ********** menu structs that build the menu system below *********

// text arrays used for submenu TYPE_TEXT fields
const char * textoffon[] = {" OFF", "  ON"};
//{CHROMATIC,MAJOR,MINOR,HARMONIC_MINOR,MAJOR_PENTATONIC,MINOR_PENTATONIC,DORIAN,PHRYGIAN,LYDIAN,MIXOLYDIAN};
const char * scalenames[] = {"Chro","Maj", "Min","Hmin","MPen","mPen","Dor","Phry","Lyd","Mixo"};
const char * textrates[] = {" 8x"," 6x"," 4x"," 3x", " 2x","1.5x"," 1x","/1.5"," /2"," /3"," /4"," /8"," /16"};

// NOTE that the order and number of the text menus much match the graphical UI pages
// ie we keep the graphic display and its associated text menus in sync - uses variable UIpage for main menus - notes, gates etc
// current_track is the index of the sequence we are editing which indexes into the submenus ie note 1, note 2
// menus are created at compile time so we have to point to each sequencer array parameter individually
struct submenu note1params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&notes[0].divider,0,
  "ROOT","MIDI Root Note",1,115,1,TYPE_INTEGER,0,&notes[0].root,0,
  "SCAL","Scale",0,9,1,TYPE_TEXT,scalenames,&current_scale[0],0,
  "CHAN","MIDI Channel",1,16,1,TYPE_INTEGER,0,&MIDIchannel[0],0,
  "ENAB","Enable Track",0,1,1,TYPE_TEXT,textoffon,&trackenabled[0],0,
  " BPM","Beats Per Min",20,240,1,TYPE_INTEGER,0,&bpm,0,
  "MCLK","Use MIDI clock",0,1,1,TYPE_TEXT,textoffon,&useMIDIclock,0,
};

struct submenu note2params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&notes[1].divider,0,
  "ROOT","MIDI Root Note",1,115,1,TYPE_INTEGER,0,&notes[1].root,0,
  "SCAL","Scale",0,9,1,TYPE_TEXT,scalenames,&current_scale[1],0,
  "CHAN","MIDI Channel",1,16,1,TYPE_INTEGER,0,&MIDIchannel[1],0,
  "ENAB","Enable Seq",0,1,1,TYPE_TEXT,textoffon,&trackenabled[1],0,
  " BPM","Beats Per Min",20,240,1,TYPE_INTEGER,0,&bpm,0,
  "MCLK","Use MIDI clock",0,1,1,TYPE_TEXT,textoffon,&useMIDIclock,0,
};
struct submenu note3params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&notes[2].divider,0,
  "ROOT","MIDI Root Note",1,115,1,TYPE_INTEGER,0,&notes[2].root,0, 
  "SCAL","Scale",0,9,1,TYPE_TEXT,scalenames,&current_scale[2],0,
  "CHAN","MIDI Channel",1,16,1,TYPE_INTEGER,0,&MIDIchannel[2],0,
  "ENAB","Enable Track",0,1,1,TYPE_TEXT,textoffon,&trackenabled[2],0, 
  " BPM","Beats Per Min",20,240,1,TYPE_INTEGER,0,&bpm,0,
  "MCLK","Use MIDI clock",0,1,1,TYPE_TEXT,textoffon,&useMIDIclock,0,
};
struct submenu note4params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&notes[3].divider,0,
  "ROOT","MIDI Root Note",1,115,1,TYPE_INTEGER,0,&notes[3].root,0,
  "SCAL","Scale",0,9,1,TYPE_TEXT,scalenames,&current_scale[3],0,
  "CHAN","MIDI Channel",1,16,1,TYPE_INTEGER,0,&MIDIchannel[3],0,
  "ENAB","Enable Track",0,1,1,TYPE_TEXT,textoffon,&trackenabled[3],0,
  " BPM","Beats Per Min",20,240,1,TYPE_INTEGER,0,&bpm,0,
  "MCLK","Use MIDI clock",0,1,1,TYPE_TEXT,textoffon,&useMIDIclock,0,
};

struct submenu gate1params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&gates[0].divider,0,
};
struct submenu gate2params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&gates[1].divider,0,
};
struct submenu gate3params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&gates[2].divider,0,
};
struct submenu gate4params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&gates[3].divider,0,
};

struct submenu velocity1params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&velocities[0].divider,0,
};
struct submenu velocity2params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&velocities[1].divider,0,
};
struct submenu velocity3params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&velocities[2].divider,0,
};
struct submenu velocity4params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&velocities[3].divider,0,
};

struct submenu offset1params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&offsets[0].divider,0,
};
struct submenu offset2params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&offsets[1].divider,0,
};
struct submenu offset3params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&offsets[2].divider,0,
};
struct submenu offset4params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&offsets[3].divider,0,
};

struct submenu probability1params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&probability[0].divider,0,
  " LEN","Eucl Length",1,16,1,TYPE_INTEGER,0,&probability[0].euclen,eucprobability,
  "BEAT","Eucl Beats",1,16,1,TYPE_INTEGER,0,&probability[0].eucbeats,eucprobability,
  "OFFS","Eucl Offset",0,15,1,TYPE_INTEGER,0,&probability[0].root,eucprobability,
};
struct submenu probability2params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&probability[1].divider,0,
  " LEN","Eucl Length",1,16,1,TYPE_INTEGER,0,&probability[1].euclen,eucprobability,
  "BEAT","Eucl Beats",1,16,1,TYPE_INTEGER,0,&probability[1].eucbeats,eucprobability,
  "OFFS","Eucl Offset",0,15,1,TYPE_INTEGER,0,&probability[1].root,eucprobability,
};
struct submenu probability3params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&probability[2].divider,0,
  " LEN","Eucl Length",1,16,1,TYPE_INTEGER,0,&probability[2].euclen,eucprobability,
  "BEAT","Eucl Beats",1,16,1,TYPE_INTEGER,0,&probability[2].eucbeats,eucprobability,
  "OFFS","Eucl Offset",0,15,1,TYPE_INTEGER,0,&probability[2].root,eucprobability,
};
struct submenu probability4params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&probability[3].divider,0,
  " LEN","Eucl Length",1,16,1,TYPE_INTEGER,0,&probability[3].euclen,eucprobability,
  "BEAT","Eucl Beats",1,16,1,TYPE_INTEGER,0,&probability[3].eucbeats,eucprobability,
  "OFFS","Eucl Offset",0,15,1,TYPE_INTEGER,0,&probability[3].root,eucprobability,
};

struct submenu ratchet1params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&ratchets[0].divider,0,
};
struct submenu ratchet2params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&ratchets[1].divider,0,
};
struct submenu ratchet3params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&ratchets[2].divider,0,
};
struct submenu ratchet4params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&ratchets[3].divider,0,
};

struct submenu mod1params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&mods[0].divider,0,
  "CHAN","CC MIDI Channel",1,16,1,TYPE_INTEGER,0,&CCchannel[0],0,
  "  CC","CC Number",0,127,1,TYPE_INTEGER,0,&mods[0].root,0,
  "ENAB","Mod On/Off",0,1,1,TYPE_TEXT,textoffon,&mod_enabled[0],0,
};
struct submenu mod2params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&mods[1].divider,0,
  "CHAN","CC MIDI Channel",1,16,1,TYPE_INTEGER,0,&CCchannel[1],0,
  "  CC","CC Number",0,127,1,TYPE_INTEGER,0,&mods[1].root,0,
  "ENAB","Mod On/Off",0,1,1,TYPE_TEXT,textoffon,&mod_enabled[1],0,
};
struct submenu mod3params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&mods[2].divider,0,
  "CHAN","CC MIDI Channel",1,16,1,TYPE_INTEGER,0,&CCchannel[2],0,
  "  CC","CC Number",0,127,1,TYPE_INTEGER,0,&mods[3].root,0,
  "ENAB","Mod On/Off",0,1,1,TYPE_TEXT,textoffon,&mod_enabled[2],0,
};
struct submenu mod4params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "RATE","Clock Rate",0,12,-1,TYPE_TEXT,textrates,&mods[3].divider,0,
  "CHAN","CC MIDI Channel",1,16,1,TYPE_INTEGER,0,&CCchannel[3],0,
  "  CC","CC Number",0,127,1,TYPE_INTEGER,0,&mods[3].root,0,
  "ENAB","Mod On/Off",0,1,1,TYPE_TEXT,textoffon,&mod_enabled[3],0,
};

/*

struct submenu env0params[] = {
  // name,longname,min,max,step,type,*textfield,*parameter,*handler
  "ATAK","Attack Time MS",0,9999,1,TYPE_INTEGER,0,&nul,0,
  "DCAY","Decay Time MS",0,9999,1,TYPE_INTEGER,0,&nul,0,
  "SUST","Sustain Level",0,1000,10,TYPE_FLOAT,0,&nul,0,
  "RELS","Release Time MS",0,9999,1,TYPE_INTEGER,0,&nul,0,
  "DELY","Delay Time MS",0,9999,1,TYPE_INTEGER,0,&nul,0,
  "HOLD","HOLD Time MS",0,9999,1,TYPE_INTEGER,0,&nul,0,
};
*/

// top level menu structure - each top level menu contains one submenu
struct menu mainmenu[] = {
  // name,submenu *,initial submenu index,number of submenus
  "Note 1",note1params,0,sizeof(note1params)/sizeof(submenu),
  "Note 2",note2params,0,sizeof(note2params)/sizeof(submenu),
  "Note 3",note3params,0,sizeof(note3params)/sizeof(submenu),
  "Note 4",note4params,0,sizeof(note4params)/sizeof(submenu),
  "Gate 1",gate1params,0,sizeof(gate1params)/sizeof(submenu),
  "Gate 2",gate2params,0,sizeof(gate2params)/sizeof(submenu),
  "Gate 3",gate3params,0,sizeof(gate3params)/sizeof(submenu),
  "Gate 4",gate4params,0,sizeof(gate4params)/sizeof(submenu),
  "Velocity 1",velocity1params,0,sizeof(velocity1params)/sizeof(submenu),
  "Velocity 2",velocity2params,0,sizeof(velocity2params)/sizeof(submenu),
  "Velocity 3",velocity3params,0,sizeof(velocity3params)/sizeof(submenu),
  "Velocity 4",velocity4params,0,sizeof(velocity4params)/sizeof(submenu),
  "Offset 1",offset1params,0,sizeof(offset1params)/sizeof(submenu),
  "Offset 2",offset2params,0,sizeof(offset2params)/sizeof(submenu),
  "Offset 3",offset3params,0,sizeof(offset3params)/sizeof(submenu),
  "Offset 4",offset4params,0,sizeof(offset4params)/sizeof(submenu),
  "Probability 1",probability1params,0,sizeof(probability1params)/sizeof(submenu),
  "Probability 2",probability2params,0,sizeof(probability2params)/sizeof(submenu),
  "Probability 3",probability3params,0,sizeof(probability3params)/sizeof(submenu),
  "Probability 4",probability4params,0,sizeof(probability4params)/sizeof(submenu),
  "Ratchets 1",ratchet1params,0,sizeof(ratchet1params)/sizeof(submenu),
  "Ratchets 2",ratchet2params,0,sizeof(ratchet2params)/sizeof(submenu),
  "Ratchets 3",ratchet3params,0,sizeof(ratchet3params)/sizeof(submenu),
  "Ratchets 4",ratchet4params,0,sizeof(ratchet4params)/sizeof(submenu),
  "Mods 1",mod1params,0,sizeof(mod1params)/sizeof(submenu),
  "Mods 2",mod2params,0,sizeof(mod2params)/sizeof(submenu),
  "Mods 3",mod3params,0,sizeof(mod3params)/sizeof(submenu),
  "Mods 4",mod4params,0,sizeof(mod4params)/sizeof(submenu),
};

#define NUM_MAIN_MENUS sizeof(mainmenu)/ sizeof(menu)
menu * topmenu=mainmenu;  // points at current menu
int16_t topmenuindex=0;  // keeps track of which top menu item we are displaying

// ******* menu handling code ************

// display the top menu
void drawtopmenu( int8_t index) {
    display.setCursor ( 0, TOPMENU_Y ); 
    display.print("                    "); // kludgy line erase
    display.setCursor ( 0, TOPMENU_Y ); 
    display.print(topmenu[index].name);
    display.display();
}

// display a sub menu item and its value
// index is the index into the current top menu's submenu array
// pos is the relative x location on the screen ie field 0,1,2 or 3 
void drawsubmenu( int8_t index, int8_t pos) {
    submenu * sub;
    // print the name text
    //display.setCursor ((DISPLAY_X/SUBMENU_FIELDS)*pos*DISPLAY_CHAR_WIDTH+DISPLAY_X_MENUPAD, SUBMENU_Y ); // set cursor to parameter name field - staggered short names
    display.setCursor (submenu_X[pos], SUBMENU_Y); // set cursor to parameter name field - staggered long names
    sub=topmenu[topmenuindex].submenus; //get pointer to the submenu array
    if (index < topmenu[topmenuindex].numsubmenus) display.print(sub[index].name); // make sure we aren't beyond the last parameter in this submenu
    else display.print("     ");
    
    // print the value
    display.setCursor (submenu_X[pos], SUBMENU_VALUE_Y ); // set cursor to parameter value field
    display.print("     "); // erase old value
    display.setCursor (submenu_X[pos], SUBMENU_VALUE_Y ); // set cursor to parameter value field
    if ((sub[index].step !=0) && (index < topmenu[topmenuindex].numsubmenus)) { // don't print dummy parameter or beyond the last submenu item
      int16_t val=*sub[index].parameter;  // fetch the parameter value   // 
      //if (val> sub[index].max) *sub[index].parameter=val=sub[index].max; // check the parameter range and limit if its out of range ie we loaded a bad patch
     // if (val< sub[index].min) *sub[index].parameter=val=sub[index].min; // check the parameter range and limit if its out of range ie we loaded a bad patch
      char temp[5];
      switch (sub[index].ptype) {
        case TYPE_INTEGER:   // print the value as an unsigned integer          
          sprintf(temp,"%4d",val); // lcd.print doesn't seem to print uint8 properly
          display.print(temp);  
          display.print(" ");  // blank out any garbage
          break;
        case TYPE_FLOAT:   // print the int value as a float 
          sprintf(temp,"%1.2f",(float)val/1000); // menu should have int value between -9999 +9999 so float is -9.99 to +9.99
          display.print(temp);  
          display.print(" ");  // blank out any garbage
          break;
        case TYPE_TEXT:  // use the value to look up a string
          if (val > sub[index].max) val=sub[index].max; // sanity check
          if (val < 0) val=0; // min index is 0 for text fields
          display.print(sub[index].ptext[val]); // parameter value indexes into the string array
          display.print(" ");  // blank out any garbage
          break;
        default:
        case TYPE_NONE:  // blank out the field
          display.print("     ");
          break;
      } 
    }

    display.display(); 
}

// display the sub menus of the current top menu

void drawsubmenus() {
  int8_t index = topmenu[topmenuindex].submenuindex;
  for (int8_t i=0; i< SUBMENU_FIELDS; ++i) drawsubmenu(index++,i);
}

//adjust the topmenu index and update the menus and submenus
// dir - int value to add to the current top menu index ie L-R scroll
void scrollmenus(int8_t dir) {
  topmenuindex+= dir;
  if (topmenuindex < 0) topmenuindex = NUM_MAIN_MENUS -1; // handle wrap around
  if (topmenuindex >= NUM_MAIN_MENUS ) topmenuindex = 0; // handle wrap around
  display.clearDisplay();
  drawtopmenu(topmenuindex);
  drawsubmenus();    
}

// same as above but scrolls submenus
void scrollsubmenus(int8_t dir) {
  if (dir !=0) { // don't redraw if there is no change
    dir= dir*SUBMENU_FIELDS; // sidescroll SUBMENU_FIELDS at a time
    topmenu[topmenuindex].submenuindex+= dir;
    if (topmenu[topmenuindex].submenuindex < 0) topmenu[topmenuindex].submenuindex = 0; // stop at first submenu
    if (topmenu[topmenuindex].submenuindex >= topmenu[topmenuindex].numsubmenus ) topmenu[topmenuindex].submenuindex -=dir; // stop at last submenu     
    display.clearDisplay();  // for now, redraw everything
    drawtopmenu(topmenuindex);
    drawsubmenus(); 
  }     
}

// show a message on 2nd line of display - it gets auto erased after a timeout
void showmessage(const char * message) {
  display.setCursor(0, MSG_Y); 
  display.print(message);
  messagetimer=millis();
  message_displayed=true;
}

// clear the message on the message line
void erasemessage(void) {
    display.setCursor(0, MSG_Y); 
    display.print("                    "); 
    message_displayed=false; 
}

void domenus(void) {
  int16_t encoder;
  int8_t index; 
  int16_t encodervalue[4]; 
  ClickEncoder::Button button; 
  // process the menu encoder - scroll submenus, scroll main menu when button down
  encoder=menuenc.getValue(); // compiler bug - can't do this inside the if statement

  if ( enc != 0) {  // if encoder is rotated, side scroll to more menu parameters if there are any
      scrollsubmenus(encoder);           
  }


// process parameter encoder buttons - button gestures are used as shortcuts/alternatives to using main encoder
// hold button and rotate to scroll submenus 
/*
  if (enc[P0].getButton() == ClickEncoder::Closed) scrollsubmenus(enc[P0].getValue());  // if button pressed, side scroll submenus
  if (enc[P1].getButton() == ClickEncoder::Closed) scrollsubmenus(enc[P1].getValue());
  if (enc[P2].getButton() == ClickEncoder::Closed) scrollsubmenus(enc[P2].getValue());
  if (enc[P3].getButton() == ClickEncoder::Closed) scrollsubmenus(enc[P3].getValue());
 */

  index= topmenu[topmenuindex].submenuindex; // submenu field index
  submenu * sub=topmenu[topmenuindex].submenus; //get pointer to the current submenu array
 
  
 // process parameter encoders
  encodervalue[0]=enc[P0].getValue();  // read encoders
  encodervalue[1]=enc[P1].getValue();
  encodervalue[2]=enc[P2].getValue();
  encodervalue[3]=enc[P3].getValue();

  for (int field=0; field<4;++field) { // loop thru the on screen submenus
    if (encodervalue[field]!=0) {  // if there is some input, process it
      int16_t temp=*sub[index].parameter + encodervalue[field]*sub[index].step; // menu code uses ints - convert to floats when needed
      if (temp < (int16_t)sub[index].min) temp=sub[index].min;
      if (temp > (int16_t)sub[index].max) temp=sub[index].max;
      rp2040.idleOtherCore();  // stop core 1 while we change the value
      *sub[index].parameter=temp;
      rp2040.resumeOtherCore();
      if (sub[index].handler != 0) (*sub[index].handler)();  // call the handler function
      erasemessage(); // undraw old longname
      showmessage(sub[index].longname);  // show the long name of what we are editing
      drawsubmenu(index,field);
    }
    ++index;
    if (index >= topmenu[topmenuindex].numsubmenus) break; // check that we have not run out of submenus
  }
}



