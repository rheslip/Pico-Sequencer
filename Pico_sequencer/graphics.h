
// graphics related definitions and functions


// canvas is the part of the screen used for graphics
#ifdef OLED_DISPLAY
#define CANVAS_ORIGIN_X  0 // upper right corner
#define CANVAS_ORIGIN_Y  15 // upper right corner
#define CANVAS_HEIGHT 48 
#define CANVAS_WIDTH 128 
#endif

#ifdef TFT18_DISPLAY
#define CANVAS_ORIGIN_X  0 // upper right corner
#define CANVAS_ORIGIN_Y  24 // upper right corner
#define CANVAS_HEIGHT 80 
#define CANVAS_WIDTH 160 
#endif



// plot a note on the screen
// index = position 0-15
// note_offset = offset from root note - range +-12
void drawnote(int16_t index,int16_t note_offset) {
  int x=CANVAS_ORIGIN_X+(CANVAS_WIDTH/SEQ_STEPS)*index;
  int y=CANVAS_ORIGIN_Y + CANVAS_HEIGHT/2 - note_offset*2; //
  display.drawLine(x,y,x+(CANVAS_WIDTH/SEQ_STEPS), y, WHITE);
#ifdef OLED_DISPLAY
  display.display();
#endif
}

// erase a note on the screen
// index = position 0-15
// note_offset = offset from root note - range +-12
void undrawnote(int16_t index,int16_t note_offset) {
  int x=CANVAS_ORIGIN_X+(CANVAS_WIDTH/SEQ_STEPS)*index;
  int y=CANVAS_ORIGIN_Y + CANVAS_HEIGHT/2 - note_offset*2; //
  display.drawLine(x,y,x+(CANVAS_WIDTH/SEQ_STEPS), y, BLACK);
#ifdef OLED_DISPLAY
  display.display();
#endif
}

// draw all the notes in a note sequence
void drawnotes(sequencer p) {
  for (int i=0;i< SEQ_STEPS;++i) drawnote(i,p.val[i]);
}

// plot a bar on the screen
// index = position 0-15
// val - unscaled height of bar
// max - max value of val - used to scale val to the screen

void drawbar(int16_t index,int16_t val, int16_t max) {
  int x=CANVAS_ORIGIN_X+(CANVAS_WIDTH/SEQ_STEPS)*index;
  int y=map(val,0,max,CANVAS_ORIGIN_Y + CANVAS_HEIGHT,CANVAS_ORIGIN_Y); //
  int height=CANVAS_ORIGIN_Y + CANVAS_HEIGHT-y;
  display.fillRect(x,y,(CANVAS_WIDTH/SEQ_STEPS), height, WHITE);
#ifdef OLED_DISPLAY
  display.display();
#endif
}

void undrawbar(int16_t index,int16_t val, int16_t max) {
  int x=CANVAS_ORIGIN_X+(CANVAS_WIDTH/SEQ_STEPS)*index;
  int y=map(val,0,max,CANVAS_ORIGIN_Y + CANVAS_HEIGHT,CANVAS_ORIGIN_Y);
  int height=CANVAS_ORIGIN_Y + CANVAS_HEIGHT-y;
  display.fillRect(x,y,(CANVAS_WIDTH/SEQ_STEPS), height, BLACK);
#ifdef OLED_DISPLAY
  display.display();
#endif
}

// draw all the notes in a note sequence
void drawbars(sequencer p) {
  for (int i=0;i< SEQ_STEPS;++i) drawbar(i,p.val[i],p.max);
}

// plot index on the screen
// index = position 0-15
void drawindex(int16_t index) {
  int x=CANVAS_ORIGIN_X+(CANVAS_WIDTH/SEQ_STEPS)*index+4;
  int y=CANVAS_ORIGIN_Y -4; //
  display.fillCircle(x,y,2, WHITE);
#ifdef OLED_DISPLAY
  display.display();
#endif
}

void undrawindex(int16_t index) {
  int x=CANVAS_ORIGIN_X+(CANVAS_WIDTH/SEQ_STEPS)*index+4;
  int y=CANVAS_ORIGIN_Y -4; //
  display.fillCircle(x,y,2, BLACK);
#ifdef OLED_DISPLAY
  display.display();
#endif
}

// update the index on the screen - LED emulation
void updateindex(sequencer seq) {
  static int16_t last_index; // tracks the sequencer index
  if (seq.index != last_index) { // draw the index marker
    undrawindex(last_index);
    drawindex(seq.index);
    last_index=seq.index;
  }
}

// edit a note sequence
// both cores accessing the same data can be dangerous
// so I added code to idle core 1 while core 0 is changing sequencer values
void editnotes(sequencer *seq) {
  int16_t encvalue;
  for (int steppos=0; steppos< SEQ_STEPS;++steppos) {  
    if((encvalue=enc[steppos].getValue()) !=0) {
      undrawnote(steppos,seq->val[steppos]);
      rp2040.idleOtherCore();
      seq->val[steppos]=constrain(seq->val[steppos]+encvalue,-seq->max,seq->max); // values can be + or -
      rp2040.resumeOtherCore();
      drawnote(steppos,seq->val[steppos]);
    }
    if (enc[steppos].getButton()==ClickEncoder::Closed) { // set end of sequence with the button
      rp2040.idleOtherCore();
      seq->last=steppos;
      rp2040.resumeOtherCore();
    }
  }
}

// edit a bar graph type sequence - gates, velocity etc
void editbars(sequencer *seq) {
  int16_t encvalue;
  for (int steppos=0; steppos< SEQ_STEPS;++steppos) {  
    if((encvalue=enc[steppos].getValue()) !=0) {
      undrawbar(steppos,seq->val[steppos],seq->max);
      rp2040.idleOtherCore();
      seq->val[steppos]=constrain(seq->val[steppos]+encvalue,0,seq->max); // values can be 0 to max     
      rp2040.resumeOtherCore();
      drawbar(steppos,seq->val[steppos],seq->max);
    }
    if (enc[steppos].getButton()==ClickEncoder::Closed) { // set end of sequence with the button
      rp2040.idleOtherCore();
      seq->last=steppos;
      rp2040.resumeOtherCore();
    }
  }
}

void drawheader(String text){
//  display.clearDisplay();
  display.fillScreen(BLACK);
  display.setCursor(0,0);
  display.print(text+" ");
  display.print(current_track+1);
}