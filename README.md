# Pico Sequencer - four track generative MIDI sequencer based on Raspberry Pi Pico

This sequencer is inspired by quite a few I've used before but mostly by Monome's Kria. It can be used as a simple one to sixteen step sequencer with four tracks but what makes it generative is there are actually six independent sequencers per track.
Each sequencer has its own length which defaults to 16 steps on power up. 
Sequencer step values are edited by rotating the encoder for that step. Press a step encoder to set the sequence length e.g. press encoder 8 to make the sequence 8 steps. Each sequencer has its own clock rate which defaults to 1x (1 beat) but can range from 8 times faster to divided by 16. This results in 32nd durations at 8x
to 4 bar duration at /16 (assuming 4/4 time). The fun starts when you start changing clock rates and sequence lengths - the phase of each sequencer will change relative to the others. This results in rhythmic and melodic patterns that can have a length much longer than the individual sequence lengths.

* Note sequencer - Notes are displayed as a simple piano roll as offsets +- one octave from the root note. The root note for each note sequence is set in the associated menu along with its clock rate, scale, MIDI channel and the option to turn it on or off.
	
* Gate sequencer - gates are displayed as vertical bars - longer bar indicates longer gate length. Range is 0% (note is off) to 100% which ties this note to the next. Ties can be cascaded for longer note lengths and interesting rhythmic effects. 
The gate sequencer triggers note on and note off events. The clock rate for gate sequences is set in its associated menu.
	
* Velocity sequencer - sets note velocity. Velocity is displayed as vertical bars - longer bar indicates higher MIDI velocity, range 0 to 127 in 10% increments. Clock rate is set in the associated menu.
	
* Offset sequencer - this adds an offset or transpose to the note sequence. Offsets have the same range as notes +- one octave and are displayed the same way as notes. When triggered by the gate sequencer, the note that will play is root+note+offset. 
Clock rate is set in the associated menu.
	
* Probability sequencer - this sequencer determines the probability that the note will play. Probability is displayed as vertical bars-longer bar indicates higher probability, range 0 to 100% on 10% increments. 
You can create euclidean rhythm patterns in the probability sequencer by setting the eulidean length, beats and offset in the associated menu. Probability clock rate is also set in the associated menu.
	
* Ratchet sequencer - you can add ratchets (repeats) to any step by adjusting the vertical bar for that step with its encoder. Ratchets range from no repeats (default) to 4 repeats. Ratcheting works by subdividing the gate period by the number of ratchets on that step. 
This does not currently work with tied steps however. Ratchet clock rate is also set in the associated menu. Note that the clock rate affects the rate at which the ratchet sequencer advances, not the rate of ratcheting.

The Start/Stop button is used to start and stop the sequencer. Holding the Shift button and pressing Start/Stop will reset all sequencers back to the first step and synchronize their clocks.


Graphical UI

The current sequencer is drawn on the display as a piano roll for notes and offsets or as a series of bars for the other sequencers. Rotating the menu encoder scrolls through the six sequencer displays. Press and rotate the menu encoder to switch tracks.


Pressing the Shift button will bring up a text menu of the parameters (clock rates etc) for the sequencer that is currently on the screen. Encoders 11,12,13 and 14 are used to change the four values which are arranged left to right. 
In some cases e.g. note sequencers there are more parameters that can be accessed by rotating the menu encoder. When the shift button is released the sequencer graphics will be redrawn on the screen. The menus were separated from the sequencer display because the screen real estate is very limited.

Scales can be selected from the note menu. There are 10 scales: chromatic, major, minor, harmonic minor, major pentatonic, minor pentatonic, dorian, phrygian, lydian and mixolydian. Note that each track can have its own scale.

Tempo can be set on each note track from 20-240 BPM. Although its shown in each note menu for consistency there is only one BPM value which is used for all tracks.

External MIDI clock and transport works automatically - if the host supplies clock the Pico Sequencer will sync to it. Note that it takes quite a few clocks (typically a few bars) for the sequencer to sync its clock to the host. Hopefully I can speed this up in future.
MIDI start, stop and pause messages from the host are also processed. Host control has not been tested extensively but seems to work OK with AUM on iPadOS.

At this point the sequencer is USB MIDI only. I will probably add serial MIDI, CV and Gate outputs to it at some point.


My Comments on the Pico Sequencer:


I find its best to start with something simple - one track with maybe 4 notes, play with the gates etc to get something musically interesting before tweeking too much other stuff. 
If you start out with a bunch of different sequencer lengths, clock rates etc the results will likely be kind of wonky.
If you are  getting a lot of bum notes check that the tracks have musically related scales and roots. You will usually want all tracks on the same root and scale, or roots up or down by octaves.


The code uses both cores of the Pico. First core is used for scanning the encoders and handling the graphical UI and menus. For timing accuracy the second core is dedicated to clocking the sequencers and sending MIDI notes.
The code has gotten a little bit out of hand as I added more features. Not terrible but its not object oriented and there are a lot of global variables and perhaps not obvious interactions. I wrote the menu code a couple of years ago and I keep tweeking it for every new project.
I wish I was a better C++ programmer.

Still todo:

* Step modes - step forward, backward, ping pong, random etc

* CC control - might be interesting to have CC or note control of some parameters. e.g. a global transpose so you can "play" the sequencer with a keyboard or another sequencer. Route CC LFOs to change sequencer lengths etc.

* Mutation/Randomization - I'd like to come up with some "musical sounding" note randomization algorithms. Picking a random note in the scale does not work very well but this is what most randomizers do.

* Perhaps a "reset to defaults" for when you get really deep in the weeds


The Pico sequencer uses fairly simple and inexpensive hardware:

* 128x64 OLED display

* Sixteen Encoders with switches for setting step values and sequence lengths
	
* Three 4067 analog mux modules for interfacing the sixteen step encoders to the Pico
	
* One Menu encoder with switch

* Two buttons - Start/Stop and Shift 



No schematics yet but you can pretty much figure it out by looking at the code. All connections are directly to the Pico port pins with the exception of the sixteen step encoders which are multiplexed by the 4067's.



# Software Dependendencies:

* Adafruit Graphics library and the SSD1306 driver (code actually uses SH1106 but SSD1306 is recommended) https://github.com/adafruit/Adafruit-GFX-Library https://github.com/adafruit/Adafruit_SSD1306

* Adafruit TinyUSB

* Clickencoder (included in the sketch library directory)

* Pico Timer library https://github.com/khoih-prog/RPI_PICO_TimerInterrupt

Compiled with Arduino 2.01 with Arduino Pico installed. Select the TinyUSB stack in the Arduino IDE tools menu build options.


Rich Heslip May 2023

 
