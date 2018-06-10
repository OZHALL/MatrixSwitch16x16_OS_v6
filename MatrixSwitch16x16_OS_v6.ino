 /*************************************************** 
  OS for 16x16 Matrix Switch Controller
  Based on the Teensy LC and the AD75019 16x16 matrix switch
  Also using two Adafruit 16x8 minimatrix LED boards (accessed via I2C)
  and two custom boards each with 8 bourns 45 mm faders with LED (PTL series) (accessed via I2C)

  History
  Date        Dev   Mod
  2017-07-22  ozh   Begin with the test driver for the minimatrix & fader boards
                    Set up the "model" for the AD75019 (Routes[])
                    Set up a test driver which initializes the model and sets up a default routing
                    Toggle the routing on output 1 (of 16) (Routes[0]) between input 1 and 2 (of 16) 
                    Setup the "view" to update the minimatrix to display the routing of the first 8 output channels
  2017-07-23  ozh   all of the above code is working 
  2017-07-23  ozh   added the AD75019 serialization code.  made the AD75019 another "view" for the model.  This works (sort of).
                    there is a timing issue. with writing "non-zero" input (X) values to the switch.  (I'm only testing output (Y) 0 now.
  2017-07-23  ozh   the "timing issue" may have been just a "grounding" problem between the outputs and the P22-2600.  It disappeared as I added more patch cables.
                    (note: this seems odd.  w/o a ground the levels would have been (constantly?) quiet or non-existant.  It may have also been a patching issue on P22-2600???
  2017-07-24  ozh   Worked out a kink where each fader board was sending 16 fader values (not 8).  It was a fix to the fader firmware.
                    The model is not being updated by the faders.  The position is correctly reflected on the miniMatrix LEDs.
                    I'm getting way too much flicker on the minimatrix LEDs.  (because of frequent updates and the "clear" command)
                    I also still have a little bleed between adjacent faders.  
  2017-07-24  ozh   I slowed the clock ADC clock down on the fader board to get rid of the "bleed".  seems ok now.
                    I've make the MiniMatrix LED update more intelligent - keep a "prev" value to turn off old LED before turning on the new one
                    This is working sort of.  we still have a data integrity issue getting data from the fader board.
                    Also, I only update the switches of an actuall change occurred to the LEDs.
                    Finally, it appears I may have an issue with my AD75019 update routine. half the time it fails.
                    I think the failure has to do with the flakey fader values.
  2017-07-25   ozh  I changed from 4x oversample/average to 6x (to 10x) and the output from the fader boards stabilized!
                    whoops, when I take off the programming pins, it gets flakey again (but ADC is much more stable).  
                    If I connect ground from the control board to one of the jacks it starts working reliably! 
  2017-07-26   ozh  Added switch support.  JP9 (pin 17, load/save) works fine.  JP8 (pin 16, mode switch) is not nearly stable enough.  Need to chase that down.          
  2017-08-13   ozh  First full hardware prototype is assembled.
  2017-08-13   ozh  Bot switches are working and are decoding properly.  Still need logic to operate on them
  2017-08-13   ozh  updating the Routes with the 1-8/9-16 switch seems to be working, but the display is not working right.
                    It works correctly for 1-8 but not 9-16 (display remains blank).  If you switch IPs, the second display
                    correctly reflects 1-8 if in that mode.
  2017-08-14   ozh  got the second display towork correctly w/mode switching!
  2017-08-14   ozh  v1.4 begin to code support for VCA control
  2017-08-15   ozh  spent most of the evening tracing & debugging hardware issues (found one bug in the VCA/Mix board - missing trace for input 3 from 30K to 500R).
                    note that something is funky with output 4 (which is the only one that goes thru the TL072 summers).
  2017-08-15   ozh  Unscrambled "routing" on VCA boards (1-4 are connected to 5 and mix 6-8)  (5-8 are connected to 1-4)
                    And added VCA control via MAX529 (also toasted one of my V2164s! ... don't short inputs to either power pin).
  2017-08-16   ozh  Implemented soft takeover for Routes.  
  2017-08-16   ozh  Added pot input (patch) and logic for switch patches (write/load switch)
  2017-08-18   ozh  Storing presets works fine, but there is only 128KB of EEProm in the TeensyLC!!!  that's enough for 2 presets
  2017-08-18   ozh  Changed a few things, now we have 4 presets
  2017-08-23   ozh  Boosted AD75019 clock frequency to get rid of load errors!!!
  2017-08-28   ozh  removed most of the delay in putByte to crank the SPI clock frequency even higher.
  2017-09-01   ozh  add I2C EEPROM support (24LC256) for program storage - 
                    huge debug cycle (5 hours?) to find out that after writing a block (of up to 64 bytes, on a (64b) page boundary
                    a 5 ms delay is required to let the chip do it's erase/write cycle!!!
  2017-09-03   ozh  removed delay from displayPatch().  that radically changed my response time (now the patch knob rapidly responds to changes)
                    also, this got rid of the noise when I load a patch!!!  ( I didn't see that one coming! )
  2017-09-03   ozh  added ledFaderChase
  
  V2.1 is stable
  2017-09-04   ozh  added control of the DAC from the "Patch" knob when 1-8/9-16 switch is in the middle position
  2017-09-05   ozh  fixed soft takeover.  also, prevented load & write while in "Mid Mode"

  v2.3 add LFOs for  the "mid" mode
  2017-09-09   ozh  begin by creating interrupt service routine - this is working
  
  v2.4 add wave tables & hardcode to output # 8 volume
  2017-09-09   ozh  AM is working with Rate from Patch Pot.  The initial amountis summed with scTrim[7].  
                    The following are hardcoded: destination (chnl 8),  amount (0.5), wave (sin) 
  2017-09-10   ozh  Significant headway in "Mid" mode with reading faders into a separate buffer for the LFOs.
                    We're still only changing the Rate for the first LFO, but I believe we have all of the values captured  
  2017-09-11   ozh  Rate, Level, Destination and Wave are now supported
                    It is awkward to set the destination.  If we're going to have this, we should display it properly.  
                    Alternately, map 1 LFO per channel 1 - 8 and skip the routing.
                    Possibly skip the sync as well so that waves are easier to select. (in fact I've made that change)
  2017-09-12   ozh  ran out of RAM when compiling with USB=Serial+MIDI
                    switched to USB=Serial for debugging
                    The 4 LFOs seem to be working now.  Still need to 
                    1) avoid collisions from 
                      a) changes to the scTrims when in 1-8 or 9-16 mode (it's audible)
                      b) avoid having more than one LFO hit the same destination (flashing LED & skip update???)
                    2) save the LFO settings
  2017-09-12   ozh  coded "collision avoidance" between scTrim and LFO updates.  it works, but I still think I've got something backwards in my level calculations.
                    because, I get a lot of "level > 255" messages at un expected levels.
  2017-09-14   ozh  now saving the LFO settings (works).
                    also, for stability between modes:  when moving from Mid to 1-8 or 9-10, reset all of the "active pot" flags 
                    To do: 
                    1)collision avoidance is not perfect (probably because of #2 below)
                    2) review some of the bugs, includding "level > 255" (review how we handle 0 = full volume and 255 = off for the DAC)
  2017-09-15   ozh  added "use Rate control 0" when the rate for LFO 1, 2 or 3 is above 60 (of 63)
                    corrected the sin table, which was only a half sin because the source (hi res) sin table was 512 elements.  Dest table is 256 elements.
                    this also corrected the inverted sin, which was the same as the sin before!
                    added exponential frequency conversion (it was linear before).  also, tweaked the range for an overall much better feel 
  2017-09-16   ozh  added "double saw" as wave #8 instead of inverted square (also added more writeProgram and loadProgram debugging
  2017-09-16   ozh  I debugged the writes (but I don't like the implementation)
                    Still working on the LFO modulation.  I may need to dial back the amount of LFO mod a bit.
  v 3.2
  2017-09-16   ozh  I forget what all I fixed, but it's better :)
  v 3.3
  2017-09-17   ozh  Begin to add display for LFO Distination on the last 4 rows
  v 3.5             Also, enforce a "minimum change" rule on the trims to avoid noise (jitter) while in 1-8 mode.  This seems to work
                    fixed wiring problem
                    corrected the orientation of the LFO waveforms.  That's working pretty smoothly at this point.
  
  2017-09-18   ozh  add an "implementation delay" so that if I go from 1-8 to 9-16, the destination display does not kick in.
  v 3.6             the MID display is working smoothly now
  
  2017-10-01   ozh  I debugged the "disappearing" hi-hat.  
  v 3.7             The problem was as follows:
                    1) both the 528 DAC and the AD75019 matrix switch are SPI components (both using the "putByte" routine) 
                    2) the mode switch was not stable enough so it toggled between a value of 18 and 19.
                    3) with each toggle, the software would "resend" all data to the matrix switch
                    4) at the same time, the LFO is constantly updating the 528 DAC

                    even though these two updates did not happen at the same time, there was some interaction between them.
                    The sound would disappear because the channel 15 (0-15) routing would get changed.
                    This is probably a timing/settling issue.  It could still happen.

  2017-10-02   ozh  Add waveform display
  v 3.8       
  2017-10-03   ozh  reorganized LFO faders to Rate1/Rate2/Rate3/Rate4  Dest1/Dest2/Dest3/Dest4             
  v 3.9                                       Trim1/Trim2/Trim3/Trim4  Wave1/Wave2/Wave3/Wave4 

  2017-10-06   ozh  added phase sync to LFOs where the Rate is synced.
  v 4.1

  2017-10-08   ozh  fixed (stopped) display of LFO destinations when not in "mid" mode 
  v 4.2

  2017-10-18   ozh  final version for the prototype hardware (v1.1 of controller, v1.5 of vca/mix)
  v 4.3

  2017-10-19   ozh  changes for new hardware (v1.3 of controller, v1.9 of vca/mix)
  v 5.0             change control lines for AD75019 from 23 (SDA1) and 22(SCL1)  to pins 4 and 5   (22,23 will be for external I2C)
                    skip software "descrambling" of VCA/Mix channels
                    

  2017-11-07 begin support for reading/writing on "public" ElectricDruid I2C bus.   
  v 5.1      First module: dual ADSR
                    8 faders (bytes) 
                    I2C slave address is 0x10 (only one of them at first)  
                
 2017-11-08    ozh  I have set up hardware to make the I2C connection from ED buss to dualADSR. 
                    When I test the hardware on the internal buss it works as follows:
                      Chg software routine to use Wire. instead of Wire1.  
                      Connect SCK/SIN to internal connector.
                      Use the new I2C address.
                      ReadFadersW1 works correctly to retrieve the ADSR data.
 2017-11-09    ozh  write/debug the "writeI2CSystemData()" routine.

                    When I switch to ED connector and Wire1, it just hangs at the first call to Wire1.

                    When I toggle pins 22 and 23 (clk & dat for ED I2C), 22 is a strong signal, 23 does not register on the meter 
                    (although you can "hear" it if route the output to an audio in on the synth).

                    Low Memory may be a factor, but fundamentally Wire1 is not working (hardware???).

2017-11-10     ozh  beginv 5.2
v 5.2

2017-11-11     ozh  memory storage is now working properly, but the I2C gets goofed up if 
v 5.2.1                   the envelope is not in an idle state when the the I2C comms occur
                    The ED I2C bus is still not working

2018-05-31      ozh It looks as if I might be successfully switching the pins for the I2C bus when I need to use the external Electric Druid I2C bus.
v 5.2.2             Internal I2C: 19 (SCL0) & 18 (SDA0)      
                    External I2C: 23 (SCL0) & 22 (SDA0)                
                    Note that I still have a (hardware?) issue in that the data line only goes to about 1.1v.  It should be 3.3v (the clock is 3.3v) 
2018-05-31      ozh this works except that the I2C traffic is flowing across the PIN 19/18 hardware (i.e. the pin switch is NOT working)
v 5.2.3             it successfully retrieves the fader values using DualEG firmware PTL30_fader_DualEGv2_5_1 (slave ID 0x68)

2018-06-10      ozh echo fader chase on ADSR @ 0x68
v 6                 note: with this version, I've started using GitHub for source control        
todo next:

           1) add a 'first time' flag for displaying all 4 destinations, even if they did not change                                         
  Known bugs:
        there seems to be some issue with the "mix" for 1-8 (there is a significant DC offset!!!).  there is distortion in the mix above mid way (hardware issue)
        There seems to be a bug in the levels where, when you have a modulated channel and you turn the modulation all the way down, it goes to full volume
           i.e. there is not a smooth transition from LFO level 5 to 0, because the trim is left in limbo (not at the scTrim level)
        I'm not sure, but there may be an issue with setting the routing when the volumes (esp for mixer 1-8) go to zero.
        the DAC output is only c. 0-1.5v AND it is inverted  (both side effects of the VCA/Mix board!)

  TODO: 
      display patch number as soon as we raise the write button
      I2C to other modules to manage presets (program change to 701 & total management to DDL and ADSRs)
                    
 ****************************************************
 
  Mods to ADAFruit library
  Date  Dev   Mod
  1) change to pins 22(SCL1) and 23 (SDA1) for use as an internal I2C bus(going to minimatrix & fader boards)
  N.B. - internal I2C is on pins 18&19, not 22, 23!!!

  https://forum.pjrc.com/threads/21680-New-I2C-library-for-Teensy3
  
  Interface  Devices     Pin Name      SCL    SDA
---------  -------  --------------  -----  -----
   Wire      All    I2C_PINS_16_17    16     17
   Wire      All    I2C_PINS_18_19    19     18  *
  Wire1       LC    I2C_PINS_22_23    22     23  

  //the above is not possible without the i2c_t3 library.  See
  // https://www.pjrc.com/teensy/td_libs_Wire.html
  // https://forum.pjrc.com/threads/21680-New-I2C-library-for-Teensy3
  // https://github.com/nox771/i2c_t3

  the plan is to use pins 19/18 for the internal I2C bus (Wire) and pins 22/23 (Wire1) for the Electric Druid bus
  
 ****************************************************/

#include <avr/pgmspace.h>
// TODO: remove next two lines if we get the i2c_t3 library working
// this is commented out in WireKinetis.h for memory considerations. see if we can add it manually to get Wire1 support
//#define WIRE_IMPLEMENT_WIRE1
//NOTE: WireKinetis.h is included in wire.h .  It has been updated to uncomment WIRE_IMPLEMENT_WIRE1
//NB: on the new HP touch laptop, WireKinetis.h has NOT been updated yet ozh 2018-05-31
#include <Wire.h>
//#include <i2c_t3.h>

#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

// only define this for the prototype hardware (ctrl v1.1 and vca/mix v1.5)
#ifndef SM_PROTOTYPE_HARDWARE
//  #define SM_PROTOTYPE_HARDWARE
#endif


#ifndef DEF_INTERNAL_EEPROM
//  #define DEF_INTERNAL_EEPROM
#endif

#ifndef DEF_I2C_EEPROM
  #define DEF_I2C_EEPROM
#endif

#ifndef DEF_DEBUG_I2C_EEPROM
  #define DEF_DEBUG_I2C_EEPROM
#endif

// this is the master control for cutting on/off the serial output
#ifndef DEF_SERIAL_OUT
  #define DEF_SERIAL_OUT
#endif

#ifndef DEF_DEBUG_LFO
//  #define DEF_DEBUG_LFO
#endif

#ifndef DEF_DEBUG_LFO_DISPLAY
//  #define DEF_DEBUG_LFO_DISPLAY
#endif


#ifndef DEF_DEBUG_LFO_VALUE
//  #define DEF_DEBUG_LFO_VALUE
#endif

#ifndef DEF_DBG_LFO_OVERFLOW
//  #define DEF_DBG_LFO_OVERFLOW
#endif


#ifndef DEF_DBG_WAVE_DISPLAY
//  #define DEF_DBG_WAVE_DISPLAY
#endif

#ifndef DEF_DEBUG_75019_WRITE
//  #define DEF_DEBUG_75019_WRITE
#endif

#ifndef OZH702_DBG_DAC_WRITES
//  #define OZH702_DBG_DAC_WRITES
#endif

#ifndef OZH702_DBG_TEENSY_DAC_OUT
//  #define OZH702_DBG_TEENSY_DAC_OUT
#endif

#ifndef OZHMS702_DBG_CHG_VAL
//  #define OZHMS702_DBG_CHG_VAL
#endif

#ifndef OZHMS702_DBG_LFO
//  #define OZHMS702_DBG_LFO
#endif

#ifndef DEF_VCA_BOARD_TEST
//    #define DEF_VCA_BOARD_TEST
#endif

#ifndef DEF_DEBUG_FADERS 
//  #define DEF_DEBUG_FADERS 
#endif

#ifndef DEF_DEBUG_FADERS2 
//  #define DEF_DEBUG_FADERS2 
#endif

#ifndef DEF_FADERBOARD_ACTIVE
//  #define DEF_FADERBOARD_ACTIVE
#endif

#ifndef DEF_FADERBOARD_TEST_ACTIVE
//  #define DEF_FADERBOARD_TEST_ACTIVE
#endif

#ifndef DEF_PRINT_FADERBOARD_TRACE
//  #define DEF_PRINT_FADERBOARD_TRACE 
  #ifndef DEF_PRINT_TRACE
  //  #define DEF_PRINT_TRACE
  #endif
#endif

// these defines are for the Electric Druid (external) I2C bus
#ifndef DEF_PRINT_FADERBOARD_TRACE_ED
  #define DEF_PRINT_FADERBOARD_TRACE_ED 
  #ifndef DEF_PRINT_TRACE_ED
  //  #define DEF_PRINT_TRACE_ED
  #endif
#endif

#ifndef DEF_DBG_R_W_ADSR
    #define DEF_DBG_R_W_ADSR
#endif
#ifndef DEF_MINIMATRIX_ACTIVE
  #define DEF_MINIMATRIX_ACTIVE
#endif
#ifndef DEF_MINIMATRIX_TEST_ACTIVE
//  #define DEF_MINIMATRIX_TEST_ACTIVE
    #ifndef DEF_PRINT_TRACE
//      #define DEF_PRINT_TRACE
    #endif
#endif
//control whether messages from Display activity are printed
#ifndef DEF_PRINT_MINIMATRIX_TRACE
//  #define DEF_PRINT_MINIMATRIX_TRACE 
#endif
#ifndef OZH_DBG_AD75019
//    #define OZH_DBG_AD75019
#endif
  
Adafruit_8x16minimatrix matrix0 = Adafruit_8x16minimatrix();
Adafruit_8x16minimatrix matrix1 = Adafruit_8x16minimatrix();

const int cFaderCount=16;  // half are "virtual"  (1-8 & 9-16)

const uint8_t i2c_addr_fader0=0x10;
const uint8_t i2c_addr_fader1=0x11;

// dual ADSR
#ifndef DUALADSR  
  #define DUALADSR
#endif
#ifndef DEBUG_DUALADSR 
  #define DEBUG_DUALADSR 
#endif

const uint8_t s=0x68;
const uint8_t i2c_addr_dualADSR0=0x68;

const int cADSRdataBytes=8;
uint8_t dualADSRdata0[cADSRdataBytes]; // local storage for one dualADSR

const int MIDIchannel = 2;    // MIDI channel on which to transmit
const int cSoftTakeoverRange = 10; // +/- this value allows a MIDI updated value to be panel controlled
const int editTolerance = 1;   // a pot change must be + or - this value before it is sent
const int scale7bitTO8bit = 2;   // multiplier to scale 7 bit value to 8 bit value (e.g. from MIDI to digitalPot)
const int scale7bitTO10bit = 8;   // multiplier to scale 7 bit value to 10 bit value (e.g. from MIDI to Clock values)
const int scale10bitTO8bit = 4;   // divisor to scale 10 bit value to 8 bit value (e.g. for digitalPot)
const int scale10bitTO7bit = 8;   // divisor to scale 10 bit value to 7 bit value (e.g. for MIDI)
const int scale10bitTO4bit = 64;   // divisor to scale 10 bit value to 4 bit value 
const int scale10bitTO3bit = 128;  // divisor to scale 10 bit value to 3 bit value 
const int scale4bitTO7bit = 8;  //  multiplier to scale 4 bit value to 7 bit value (e.g. from in channel # to MIDI value)
const int MIDIbaseCC = 81;     // MIDI Continuous Controller - first (base) number

const int MIDIccRESETALLnote = 1; //
const int MIDIccPortAOut1 = MIDIbaseCC + 0; //
const int MIDIccPortAOut8 = MIDIbaseCC + 7; //
const int MIDIccPortBOut8 = MIDIbaseCC + 15; //

// MVC = Model View Controller.  These values are the "model".  The views are a) the Panel b) any MIDI input c) output to the hardwar

// the model for the Fader boards
uint8_t FaderBoard0[cFaderCount];      // for Route
uint8_t prevFaderBoard0[cFaderCount];
uint8_t FaderBoard1[cFaderCount];      // for Trim
uint8_t prevFaderBoard1[cFaderCount];

uint8_t LFOFaderBoard0[cFaderCount];      // for LFOs
uint8_t prevLFOFaderBoard0[cFaderCount];
uint8_t LFOFaderBoard1[cFaderCount];      // for LFOs
uint8_t prevLFOFaderBoard1[cFaderCount];

// the model for the LEDs on the faders
uint16_t FaderBoard0LEDs=0;
uint16_t FaderBoard1LEDs=0;

struct SWITCH_CHANNEL {
  // out channel is implicit as the array index in an array of these structs
  uint8_t scInChannel; // 0-15 for a 16x8 matrix.  input channel (value) routed to this (index) output channel
  uint8_t scTrim;  // 0-255 for implementing "trim" on the output
};
struct LFO_CONTROL {
uint8_t LFO_Rate;   // "Route" 1   0-63
uint8_t LFO_Level;  // "Trim"  1   0-63
uint8_t LFO_Destination; // "Route" 2  0-15 (note: we need to display these to see where we have them routed.  use 4 horizontal (double) lines)       
uint8_t LFO_Wave;    // "Trim"  2 - encode 8 waves
};
const int cOutChannelCount=16;
const int cMaxInChannelValue=15;

#define LFO_COUNT 4
// below this threshold, the level is considered "off"
#define LFO_LEVEL_THRESHOLD 5 

//SWITCH_CHANNEL MidiInChannels[cOutChannelCount];  // array of 8 output channels from MIDI
//SWITCH_CHANNEL PanelInChannels[cOutChannelCount]; // array of 8 output channels from Panel

// Note: this is the storage area from (to) which patches are written (saved).   
//       changes to this format will ruin any existing patches
SWITCH_CHANNEL mvcChannels[cOutChannelCount];     // array of 16 output channels 
LFO_CONTROL mvcLFOs[LFO_COUNT]; // array of 4 paired channels, from both fader board 1 & fader board 2 

// in terms of space, this really does not work because mvcChannels, mvcLFOs and dummySpaceMaker are NOT in contiguous memory addresses!!!
//SWITCH_CHANNEL dummySpaceMaker[cOutChannelCount]; // take up space so we don't overwrite variables, since mvcLFOs may not be on a proper boundary
uint8_t prevDestination[LFO_COUNT]; // store the previous value of the destination for display purposes
uint8_t prevWave[LFO_COUNT]; // store the previous value of the wave for detecting a change

void printMVCValues(){
    // show what we got
    for(int x=0;x<cOutChannelCount;x++){
      Serial.print("mvcChannel#: ");
      Serial.println(x);
      Serial.println(mvcChannels[x].scInChannel);
      Serial.println(mvcChannels[x].scTrim);
    }  
    // show LFOs
    for(int x=0;x<LFO_COUNT;x++){
      Serial.print("LFO#: ");
      Serial.println(x);
      Serial.println(mvcLFOs[x].LFO_Rate);
      Serial.println(mvcLFOs[x].LFO_Level);
      Serial.println(mvcLFOs[x].LFO_Destination);
      Serial.println(mvcLFOs[x].LFO_Wave);
    } 
    showADSRvalues();
}
void showADSRvalues(){
      #ifdef DUALADSR
    
     Serial.print("ADSR1 values: ");
     for(int x=0;x<cADSRdataBytes;x++){
      Serial.print(dualADSRdata0[x]);
      if((3==x)||(7==x)){
        Serial.println(".");
        if(3==x)Serial.print("ADSR1 values: ");
      }else{
        Serial.print(" | ");
      }
    }   
    #endif
}
int mvcTeensyDACValue;
int mvcPrevTeensyDACValue;
const int ciDACSoftTakeoverRange=4;

#define MAX_ROUTES 16

// 64 position exponential lookup to be used as the divisor lookup 
const int ExponentialDivisor[64]= {4,5,6,7,8,10,11,12,13,13,14,15,16,18,19,20,22,23,25,27,30,32,34,36,38,40,43,45,48,50,55,60,65,70,75,80,85,90,95,100,110,120,130,140,150,160,170,180,195,210,225,240,260,280,300,320,352,384,416,448,480,512,544,576};

// this lookup is used to unscramble the prototype channel mapping
                                      //0,1,2,3,4,5,6,7,8,9,A,B,C,D,E,F    This is the index
const byte faderChToDacCh[MAX_ROUTES]={4,5,6,7,0,1,2,3,4,5,6,7,0,1,2,3};   //This is the mapped DAC channel (cuz original board was for channels 5-8)

const int cMode1to8=0;
const int cMode9to16=1;
const int cModeMid=2;

bool bPatchPitchPotActive=true;   // make it active
int iPitchTargetMillis=0;     //used for managing display of the patch

bool bMidModeActive=true;   // make it active
int iMidModeMillis=0;         //used for managing display of the LFO Dest (and back)

bool RoutesTakeoverFlag[MAX_ROUTES];
bool TrimsTakeoverFlag[MAX_ROUTES];
#define MAX_MIDFADERS 8
bool LFOsTakeoverFlag0[MAX_MIDFADERS];
bool LFOsTakeoverFlag1[MAX_MIDFADERS];

int RoutesEditedFlag=0;
int TrimsEditedFlag=0;
int LFOsEditedFlag=0;

// map out the bit masks
//                                               // index in LEDBits
const uint16_t ciLEDBit0 = 0b1;                  // 0
const uint16_t ciLEDBit1 = 0b10;                 // 1
const uint16_t ciLEDBit2 = 0b100;                // 2
const uint16_t ciLEDBit3 = 0b1000;               // 3
const uint16_t ciLEDBit4 = 0b10000;              // 4
const uint16_t ciLEDBit5 = 0b100000;             // 5
const uint16_t ciLEDBit6 = 0b1000000;            // 6
const uint16_t ciLEDBit7 = 0b10000000;           // 7

const uint16_t ciLEDBit8  = 0b100000000;         // 8
const uint16_t ciLEDBit9  = 0b1000000000;        // 9
const uint16_t ciLEDBit10 = 0b10000000000;       // 10
const uint16_t ciLEDBit11 = 0b100000000000;      // 11
const uint16_t ciLEDBit12 = 0b1000000000000;     // 12
const uint16_t ciLEDBit13 = 0b10000000000000;    // 13
const uint16_t ciLEDBit14 = 0b100000000000000;   // 14
const uint16_t ciLEDBit15 = 0b1000000000000000;  // 15
//                            FEDCBA9876543210
uint16_t LEDBits[MAX_ROUTES];
uint16_t prevX[MAX_ROUTES]; // store the previous value
void initLEDBits(){
  LEDBits[0]=ciLEDBit0;
  LEDBits[1]=ciLEDBit1;
  LEDBits[2]=ciLEDBit2;
  LEDBits[3]=ciLEDBit3;
  LEDBits[4]=ciLEDBit4;
  LEDBits[5]=ciLEDBit5;
  LEDBits[6]=ciLEDBit6;
  LEDBits[7]=ciLEDBit7;
  LEDBits[8]=ciLEDBit8;
  LEDBits[9]=ciLEDBit9;
  LEDBits[10]=ciLEDBit10;
  LEDBits[11]=ciLEDBit11;
  LEDBits[12]=ciLEDBit12;
  LEDBits[13]=ciLEDBit13;
  LEDBits[14]=ciLEDBit14;
  LEDBits[15]=ciLEDBit15;
}
void initModelRelatedVariables();
void updateLEDfromModel(int FaderMode,bool bReDisplay);
void UpdateTrimsView();

const int ciMiniMatrixDelay=1;
const int ciAD75019UpdateDelay=1;

int iLoopCount=0;
// data for LEDs - 8 pairs of bits.  the bits are: off/on  &  dim/bright
uint8_t iMSByte=0;
uint8_t iLSByte=0;  
uint8_t i2c_addr_fader; // local I2C address variable

uint8_t  writeBuffer[50];
      
const int ciLoopCountMax=1;  // outer loops to service before running inner loop for Fader LEDs
// these work for both the internal faders as well as the dualADSR!
const uint8_t cPointerByteWriteFaders = 0b00110000; 
const uint8_t cPointerByteReadFaders  = 0b00100000; 
const uint8_t cPointerByteWriteLED    = 0b00010000; 
/*
 Pointer Byte:
D7:D4    Mode Bits              D3:D0 - Address   Comments
0 0 0 0  Configuration Mode                       Unused at the moment
0 0 0 1  Write LED mode                           Always use Address 0 with 2 data bytes
0 0 1 0  Read ADC (fader) mode  0000 to 0111      Only 0-7 address for this module
0 0 1 1  Write ADC (fader) mode 0000 to 0111  Only 0-7 address for this module 8 data bytes

*/
int iMainLoopCount=0;
int cMaxMainLoopCount=65000;
// addresses of I2C devices
#define MiniMatrixI2CAddr1to8  0x70
#define MiniMatrixI2CAddr9to16 0x71
const int cSwitchShiftNum=6;   // previously 5
const int cSwitchLoHiBreak=8;  // for cSwitchShiftNum 5  value is 16
const int cSwitchMidBreak=14;  // for cSwitchShiftNum 5  value is 28

#define PATCH_IN_PIN 14    // A0 select the input channel for the fader potentiometer
#define MODE_SWITCH_PIN 16          // JP8 1-8 or 9-16
#define SAVE_LOAD_SWITCH_PIN 17     // JP9 momentary load or save
#define AD75019_PCLK_PIN 21
#ifdef SM_PROTOTYPE_HARDWARE
  #define AD75019_SCLK_PIN 22
  #define AD75019_SIN_PIN 23
#else
  #define AD75019_SCLK_PIN 5
  #define AD75019_SIN_PIN 6
#endif
#define TEENSY_DAC_PIN 26
#define LED_PIN 13          // LED on Teensy LC

// SPI pins for VCA/MIX boards
const int DAC_1to8=0;
const int DAC_9to16=1;
#define CS0 2 //CS for VCA/MIX board 1 (1-8)
#define CS1 3 //CS for VCA/MIX board 1 (9-16)
#define DACCLK_PIN 20 //clock for SPI for VCA/MIX 
#define DACDATA_PIN 0 //data pin for SPI for VCA/MIX 

void initDAC528(int dacBoardNumber);
void writeDAC528(int dacBoardNumber, int dacNumber, byte dacData);
void writeFaderLEDs(int i2c_addr_fader,uint8_t iMSByte,uint8_t iLSByte);
void writeFadersW1(uint8_t in_I2C_address,uint8_t in_Fader_Model[],uint8_t in_count);
void readFaders(uint8_t in_I2C_address,uint8_t in_Fader_Model[],uint8_t in_count);
void readFadersW1(uint8_t in_I2C_address,uint8_t in_Fader_Model[],uint8_t in_count,bool in_b_use_22_flag);
/* BEGIN - LFO code  */

/* support for displaying waveforms in a 6x8 matrix */

const int cSinArrayLen = 10;
const int cInvSinArrayLen = 10;
const int cRampArrayLen = 12;
const int cSawArrayLen = 12;

const int cQtrPulseArrayLen = 19;
const int cSquareQtrArrayLen = 19;
const int cThreeQtrArrayLen = 19;
const int cDblSawArrayLen = 21;
// note: all of these arrays are "upside down" because 0,0 is in the upper left corner on the panel.  adjust this as we process the arrays
const int sinX[] PROGMEM =    {0,0,1,2,3,4,4,5,6,7};
const int sinY[] PROGMEM =    {2,3,4,5,4,3,2,1,0,1};
const int invSinX[] PROGMEM = {0,0,1,2,3,4,4,5,6,7};
const int invSinY[] PROGMEM = {3,2,1,0,1,2,3,4,5,4};
const int sawX[] PROGMEM =  {0,0,0,0,0,0,1,2,3,4,5,6};
const int sawY[] PROGMEM =  {0,1,2,3,4,5,5,4,3,2,1,0};
const int rampX[] PROGMEM = {0,1,2,3,4,5,6,6,6,6,6,6};
const int rampY[] PROGMEM = {0,1,2,3,4,5,5,4,3,2,1,0};

const int qtrPulseX[] PROGMEM = {0,0,0,0,0,0,1,1,1,1,1,1,2,3,4,5,6,7};
const int qtrPulseY[] PROGMEM = {0,1,2,3,4,5,5,4,3,2,1,0,0,0,0,0,0,0};
const int squareX[] PROGMEM = {0,0,0,0,0,0,1,2,3,3,3,3,3,3,4,5,6,7};
const int squareY[] PROGMEM = {0,1,2,3,4,5,5,5,5,4,3,2,1,0,0,0,0,0};
const int threeQtrPulseX[] PROGMEM = {0,0,0,0,0,0,1,2,3,4,5,5,5,5,5,5,6,7};
const int threeQtrPulseY[] PROGMEM = {0,1,2,3,4,5,5,5,5,5,5,4,3,2,1,0,0,0};
const int dblSawX[] PROGMEM = {0,0,0,0,0,0,1,2,2,3,4,4,4,4,4,4,5,6,6,7,7};
const int dblSawY[] PROGMEM = {0,1,2,3,4,5,4,3,2,1,0,1,2,3,4,5,4,3,2,1,0};

//                    0 or 8    0-15      
void displayWave(int origX, int origY, const int waveX[], const int waveY[], int arrayLen)
{
  int wkX;
  int wkY;

  #ifdef DEF_DBG_WAVE_DISPLAY
      Serial.println("displayWave");  
  #endif
  // display destinations for the 4 LFOs on the LED minimatrix
  Adafruit_8x16minimatrix *pMatrix; // pointer to matrix, cuz it could be either one
  
  // determine which matrix will be updated (left or right)
  if(origX<8) { 
    pMatrix=&matrix0;
    // prep for I2C to 16x8 MiniMatrix LED
    pMatrix->begin(MiniMatrixI2CAddr1to8);  // pass in the address     
  }else{
    pMatrix=&matrix1; 
    // prep for I2C to 16x8 MiniMatrix LED
    pMatrix->begin(MiniMatrixI2CAddr9to16);  // pass in the address 
  }
  pMatrix->setRotation(0); 
  
  // erase the previous (entire section
  for(int x=0;x<8;x++){
    for(int y=0;y<6;y++){
       wkX=x;  // origX should always be 0 (or 8, which is virtual 0)
       wkY=y+origY;  // origY should be 0 or 6
       pMatrix->drawPixel(wkX,wkY,LED_OFF);  
    }
  }
  pMatrix->writeDisplay(); // make it so

  //now draw wave
  for(int z=0;z<arrayLen;z++){
       wkX=waveX[z];          // origX should always be 0 (or 8, which is virtual 0)
       wkY=(5-waveY[z])+origY; // "5-" to adjust for upsidedown waves (0-5)
       
       // X & Y not are backwards here, since we have a 0-15 destination (X = column) and a calculated row ( based on LFO # )   
       pMatrix->drawPixel(wkX,wkY,LED_ON);
  }
  pMatrix->writeDisplay();  // make it show
}

void showSin(){  // test only
    displayWave(0,0,&sinX[0],&sinY[0],cSinArrayLen);
}

void displayLFOWaveforms(){
  for(int iLFONum=0;iLFONum<LFO_COUNT;iLFONum++) {
    // select destination channel
    int iwkLFO_Wave=mvcLFOs[iLFONum].LFO_Wave;  // 3 wave bits
    int iwkArrayLen;
    int iOrigX=0;
    int iOrigY=0;
    
    if((iLFONum==1)||(iLFONum==3)) {iOrigX=8;}
    if(iLFONum>1) {iOrigY=6;}
  
    switch(iwkLFO_Wave) {
      case 0 : iwkArrayLen=cSinArrayLen ; 
               displayWave(iOrigX,iOrigY,&sinX[0],&sinY[0],iwkArrayLen);  
               break;   
      case 1 : iwkArrayLen=cInvSinArrayLen ;
               displayWave(iOrigX,iOrigY,&invSinX[0],&invSinY[0],iwkArrayLen);  
               break; 
      case 2 : iwkArrayLen=cSawArrayLen ; 
               displayWave(iOrigX,iOrigY,&sawX[0],&sawY[0],iwkArrayLen);  
               break;   
      case 3 : iwkArrayLen=cRampArrayLen ;
               displayWave(iOrigX,iOrigY,&rampX[0],&rampY[0],iwkArrayLen);  
               break;
      case 4 : iwkArrayLen=cQtrPulseArrayLen ;
               displayWave(iOrigX,iOrigY,&qtrPulseX[0],&qtrPulseY[0],iwkArrayLen);  
               break;    
      case 5 : iwkArrayLen=cSquareQtrArrayLen ;
               displayWave(iOrigX,iOrigY,&squareX[0],&squareY[0],iwkArrayLen);  
               break;   
      case 6 : iwkArrayLen=cThreeQtrArrayLen ;
               displayWave(iOrigX,iOrigY,&threeQtrPulseX[0],&threeQtrPulseY[0],iwkArrayLen);  
               break;   
      case 7 : iwkArrayLen=cDblSawArrayLen ;
               displayWave(iOrigX,iOrigY,&dblSawX[0],&dblSawY[0],iwkArrayLen);  
               break;  
    }
  }
}  
/* end matrix waveform display */

// need to convert this to uni-polar 8 bit ( uint8_t ) by 1) >>8 and 2) +127
// also this is a 512 array.  we need to change it to 256 by taking every other sample
const int16_t sinTable[] PROGMEM = {
  0, 402, 804, 1206, 1607, 2009, 2410, 2811, 3211, 3611, 4011, 4409, 4807, 5205, 5601, 5997, 6392,
  6786, 7179, 7571, 7961, 8351, 8739, 9126, 9511, 9895, 10278, 10659, 11038, 11416, 11792, 12166, 12539,
  12909, 13278, 13645, 14009, 14372, 14732, 15090, 15446, 15799, 16150, 16499, 16845, 17189, 17530, 17868, 18204,
  18537, 18867, 19194, 19519, 19840, 20159, 20474, 20787, 21096, 21402, 21705, 22004, 22301, 22594, 22883, 23169,
  23452, 23731, 24006, 24278, 24546, 24811, 25072, 25329, 25582, 25831, 26077, 26318, 26556, 26789, 27019, 27244,
  27466, 27683, 27896, 28105, 28309, 28510, 28706, 28897, 29085, 29268, 29446, 29621, 29790, 29955, 30116, 30272,
  30424, 30571, 30713, 30851, 30984, 31113, 31236, 31356, 31470, 31580, 31684, 31785, 31880, 31970, 32056, 32137,
  32213, 32284, 32350, 32412, 32468, 32520, 32567, 32609, 32646, 32678, 32705, 32727, 32744, 32757, 32764, 32767,
  32764, 32757, 32744, 32727, 32705, 32678, 32646, 32609, 32567, 32520, 32468, 32412, 32350, 32284, 32213, 32137,
  32056, 31970, 31880, 31785, 31684, 31580, 31470, 31356, 31236, 31113, 30984, 30851, 30713, 30571, 30424, 30272,
  30116, 29955, 29790, 29621, 29446, 29268, 29085, 28897, 28706, 28510, 28309, 28105, 27896, 27683, 27466, 27244,
  27019, 26789, 26556, 26318, 26077, 25831, 25582, 25329, 25072, 24811, 24546, 24278, 24006, 23731, 23452, 23169,
  22883, 22594, 22301, 22004, 21705, 21402, 21096, 20787, 20474, 20159, 19840, 19519, 19194, 18867, 18537, 18204,
  17868, 17530, 17189, 16845, 16499, 16150, 15799, 15446, 15090, 14732, 14372, 14009, 13645, 13278, 12909, 12539,
  12166, 11792, 11416, 11038, 10659, 10278, 9895, 9511, 9126, 8739, 8351, 7961, 7571, 7179, 6786, 6392,
  5997, 5601, 5205, 4807, 4409, 4011, 3611, 3211, 2811, 2410, 2009, 1607, 1206, 804, 402, 0,
  -402, -804, -1206, -1607, -2009, -2410, -2811, -3211, -3611, -4011, -4409, -4807, -5205, -5601, -5997, -6392,
  -6786, -7179, -7571, -7961, -8351, -8739, -9126, -9511, -9895, -10278, -10659, -11038, -11416, -11792, -12166, -12539,
  -12909, -13278, -13645, -14009, -14372, -14732, -15090, -15446, -15799, -16150, -16499, -16845, -17189, -17530, -17868, -18204,
  -18537, -18867, -19194, -19519, -19840, -20159, -20474, -20787, -21096, -21402, -21705, -22004, -22301, -22594, -22883, -23169,
  -23452, -23731, -24006, -24278, -24546, -24811, -25072, -25329, -25582, -25831, -26077, -26318, -26556, -26789, -27019, -27244,
  -27466, -27683, -27896, -28105, -28309, -28510, -28706, -28897, -29085, -29268, -29446, -29621, -29790, -29955, -30116, -30272,
  -30424, -30571, -30713, -30851, -30984, -31113, -31236, -31356, -31470, -31580, -31684, -31785, -31880, -31970, -32056, -32137,
  -32213, -32284, -32350, -32412, -32468, -32520, -32567, -32609, -32646, -32678, -32705, -32727, -32744, -32757, -32764, -32767,
  -32764, -32757, -32744, -32727, -32705, -32678, -32646, -32609, -32567, -32520, -32468, -32412, -32350, -32284, -32213, -32137,
  -32056, -31970, -31880, -31785, -31684, -31580, -31470, -31356, -31236, -31113, -30984, -30851, -30713, -30571, -30424, -30272,
  -30116, -29955, -29790, -29621, -29446, -29268, -29085, -28897, -28706, -28510, -28309, -28105, -27896, -27683, -27466, -27244,
  -27019, -26789, -26556, -26318, -26077, -25831, -25582, -25329, -25072, -24811, -24546, -24278, -24006, -23731, -23452, -23169,
  -22883, -22594, -22301, -22004, -21705, -21402, -21096, -20787, -20474, -20159, -19840, -19519, -19194, -18867, -18537, -18204,
  -17868, -17530, -17189, -16845, -16499, -16150, -15799, -15446, -15090, -14732, -14372, -14009, -13645, -13278, -12909, -12539,
  -12166, -11792, -11416, -11038, -10659, -10278, -9895, -9511, -9126, -8739, -8351, -7961, -7571, -7179, -6786, -6392,
  -5997, -5601, -5205, -4807, -4409, -4011, -3611, -3211, -2811, -2410, -2009, -1607, -1206, -804, -402,
};
uint8_t u8SinTable[256];
uint8_t u8InvSinTable[256];
uint8_t u8SawTable[256];
uint8_t u8RampTable[256];
uint8_t u8QuarterPulseTable[256];
uint8_t u8SquareTable[256];
uint8_t u8ThreeQtrPulseTable[256];
uint8_t u8DoubleSawTable[256];
uint8_t u8InvSquareTable[256];

uint8_t u8WaveTables[8][256];    // a place for all of them

float fWkSample;
uint8_t ui8WkSample;
uint8_t ui8PrevWkSample;

// https://www.pjrc.com/teensy/td_timing_IntervalTimer.html
#include "IntervalTimer.h"
// Create an IntervalTimer object 
IntervalTimer myTimer;

volatile uint8_t iLFOIndex[LFO_COUNT];
uint8_t iwkLFOIndex[LFO_COUNT];
uint8_t iwkPrevLFOIndex[LFO_COUNT];
volatile bool bserviceMyTimer0Flag[LFO_COUNT];
bool bwkserviceMyTimer0Flag[LFO_COUNT];
uint8_t iLFORate[LFO_COUNT]; // this will be used to decide whether to update the index

uint8_t iTimerLoopCounter=0;
volatile int iExpLFORate;
volatile int iISR_LFORate;

void TimerSetup() {   
    myTimer.priority(128); // 128 of 0-255 (higher # is lower priority)  // "Most other interrupts default to 128"
    // init fader board LEDs
    //showInterruptSetup();
    myTimer.begin(serviceMyTimer,75);  // LFO clock to run every 0.0000,075 seconds
}

void serviceMyTimer(void)
{
    // see https://www.pjrc.com/teensy/interrupts.html
    // recommendation: never use sei() within any interrupt service routine
    /* Timer 0 overflow */
    iTimerLoopCounter++; // increment & let it roll over
    // "sync" to the first LFO chnl if Rate is at c. the slowest rate
    for(int LFOnum=0;LFOnum<LFO_COUNT;LFOnum++) {
      if( (61<iLFORate[LFOnum]) && (LFOnum>0) ) {
        iISR_LFORate=iLFORate[0]; // "sync" to the first LFO chnl
        // phase sync
        iLFOIndex[LFOnum]=iLFOIndex[0]; // this will actually be 1 position out of sync, since iLFOIndex[0] has already been updated.
      }else{
        iISR_LFORate=iLFORate[LFOnum];
      }
      
      // use iLFORate to lookup the actual rate.  this changes the linear control to exponential
      iExpLFORate=ExponentialDivisor[ iISR_LFORate ];
      if(0==iTimerLoopCounter%iExpLFORate) {     //
        iLFOIndex[LFOnum]++;  // increment & let it roll over
        bserviceMyTimer0Flag[LFOnum]=true;
      }
    }
} 

void showInterruptSetup(){
      // back to all leds off
    writeFaderLEDs(i2c_addr_fader0,0,0); 
    writeFaderLEDs(i2c_addr_fader1,0,0); 

    FaderBoard0LEDs=03; // the bit layout is backwards.   03 = the two LSBs on.  That corresponds to fader 1 which is leftmost 
    writeFaderLEDs(i2c_addr_fader0,0,FaderBoard0LEDs&0xFF); 
    #ifdef DEF_SERIAL_OUT
      Serial.println("showInterruptSetup");
    #endif
}

void showInterrupt() {
    int i2c_addr_fader=i2c_addr_fader0;// fader submodule A0
    uint8_t mvcMSByte;
    uint8_t mvcLSByte;
    //int iChaseDelayMS=50;
      
    mvcMSByte=FaderBoard0LEDs>>8;     // MSB
    mvcLSByte=FaderBoard0LEDs&0xFF;   // LSB
    writeFaderLEDs(i2c_addr_fader,mvcMSByte,mvcLSByte);
    //delay(iChaseDelayMS); 
    FaderBoard0LEDs++;   // let it roll over
    #ifdef DEF_SERIAL_OUT
      Serial.println(FaderBoard0LEDs);
    #endif
}


void populateWavetables(){
  uint16_t iu16wkSample;
  // populate standard wavetables
  // saw & ramp
  for(int i=0;i<=255;i++){ 
     u8SawTable[i]=255-i; 
     u8RampTable[i]=i;
     if(i<128)  {
      u8DoubleSawTable[i]=512-(i+i); // double the value
     }else{
      u8DoubleSawTable[i]=512-(i+i-256); // start the ramp over     
     }
  }

  // various pulses
  for(int i=0;i<=255;i++){ 
    u8QuarterPulseTable[i]=0;  
    u8SquareTable[i]=0;       
    u8ThreeQtrPulseTable[i]=0;
    u8InvSquareTable[i]=0;
    if(i<128) {u8SquareTable[i]=255;}       
    if(i>127) {u8InvSquareTable[i]=255;}
    if(i<64)  {u8QuarterPulseTable[i]=255;}
    if(i<192) {u8ThreeQtrPulseTable[i]=255;}
  }

  // sin & inverted sin
  for(int i=0;i<=255;i++){ 
    iu16wkSample=sinTable[i+i]+32767;  // make it uni-polar  also, double the index to convert 512 array to 256 array
    iu16wkSample=iu16wkSample>>8;    // make it 8 bit
    u8SinTable[i]=iu16wkSample;      // put it in the table
    // populate the inverse table
    u8InvSinTable[255-i]=u8SinTable[i]; 
  }

  // now marshall them into an array of wavetables
  for(int x=0;x<8;x++){
      for(int i=0;i<=255;i++){ 
        if(0==x) { u8WaveTables[x][i] = u8SinTable[i]; }
        if(1==x) { u8WaveTables[x][i] = u8InvSinTable[i]; }
        if(2==x) { u8WaveTables[x][i] = u8SawTable[i]; }
        if(3==x) { u8WaveTables[x][i] = u8RampTable[i]; }
        if(4==x) { u8WaveTables[x][i] = u8QuarterPulseTable[i]; }
        if(5==x) { u8WaveTables[x][i] = u8SquareTable[i]; }
        if(6==x) { u8WaveTables[x][i] = u8ThreeQtrPulseTable[i]; }
        if(7==x) { u8WaveTables[x][i] = u8DoubleSawTable[i]; }
        //if(7==x) { u8WaveTables[x][i] = u8InvSquareTable[i]; }
     }
  }
}

void setupLFO() {
  populateWavetables();
  // init the level and destination because we test these fields in the trim update
  for(int iLFONum=0;iLFONum<LFO_COUNT;iLFONum++){
    mvcLFOs[iLFONum].LFO_Destination=0;
    prevDestination[iLFONum]=16;              // init the prevDestination for the display
    mvcLFOs[iLFONum].LFO_Level=0;
  }
  // init the working variables
  for(int x=0;x<LFO_COUNT;x++) { 
    iLFORate[x]=0; 
    iLFOIndex[x]=0;
    iwkPrevLFOIndex[x]=0;
    bwkserviceMyTimer0Flag[x]=false;
  }
}

/* END - LFO code  */
/* BEGIN - EEPROM patch storage support */
// I2C_EEPROM library (public domain)
// https://github.com/RobTillaart/Arduino/tree/master/libraries/I2C_EEPROM

// include the EEPROM library:
#ifdef DEF_INTERNAL_EEPROM
  #include <EEPROM.h>
#endif

const int MvcMinValue = 0;
const int MvcMaxValue = 1023;
const int maxPatches = 36;  // vs 12 for banks
const int maxInChannelNum = 15; // 0-15
const int patchDivisor = 29;  // technically 28.44 (i.e. 1024/28.44 = 36)
const int breakPoint = MvcMaxValue/maxPatches;
const int breakPointOffset = breakPoint/2; // avoid jitter by comparing to the middle of the range, not the edge

const int maxBanks = 1;     // vs 3 for banks
const int bankBreakPoint = MvcMaxValue/maxBanks;
//int wkPatchSize=sizeof(mvcChannels)+sizeof(mvcLFOs);
// TODO: true this up for addition of DUALADSR
const int PatchSize=sizeof(mvcChannels)+sizeof(mvcLFOs)+sizeof(mvcLFOs); // force it to 64 byte boundary

const int LEDOnDelay=20;
//used for managing display of the patch
int iTargetMillis=0;

// Teensy LC has only 128 bytes.  Use flash memory (program memory) to store patches
/* PROGMEM skip this for now.
SWITCH_CHANNEL patches[maxPatches][cOutChannelCount] PROGMEM;
*/

// logic is a bit different with a single switch which toggles up for write and down for load
/*
int WriteButtonPressedFlag = LOW; // pressed then released = 0 (i.e. commit write), released then pressed = 1; Teensy 2 analog input
int prevWriteButtonPressedFlag = LOW;
int LoadButtonPressedFlag = LOW;  // pressed then released = 0 (i.e. commit load),  released then pressed = 1; Teensy 2 analog input
int prevLoadButtonPressedFlag = LOW;

const int PgmWriteButtonInChannel = 7; // select the input pin for the Write switch
int PgmSelectValue = 0;
int PgmWriteButtonValue = 0; // variable to store the value coming from the switch

int PgmWriteButtonChannel = 7;
*/
void displayPatch(int iDelayMS,int iDisplayIfUnchangedflag,int iPatchNumber);

void displayPatch(int iDelayMS,int iDisplayIfUnchangedflag,int iPatchNumber) {
  // display Patch number on the LED minimatrix
  Adafruit_8x16minimatrix *pMatrix; // pointer to matrix, cuz it could be either one
  
  int wkPatchNumber=iPatchNumber+1;
  int wkOnes=wkPatchNumber%10;;
  int wkTens=wkPatchNumber/10;

  pMatrix=&matrix1; // point to the right matrix

  pMatrix->setTextSize(1);
  pMatrix->setTextWrap(false);  // we dont want text to wrap so it scrolls nicely
  pMatrix->setTextColor(LED_ON);
  pMatrix->setRotation(0);

  pMatrix->clear();
  pMatrix->setCursor(2,1);
  pMatrix->print(wkOnes);
  pMatrix->writeDisplay();

  pMatrix=&matrix0; // point to the left matrix
  
  pMatrix->setTextSize(1);
  pMatrix->setTextWrap(false);  // we dont want text to wrap so it scrolls nicely
  pMatrix->setTextColor(LED_ON);
  pMatrix->setRotation(0);

  pMatrix->clear();
  pMatrix->setCursor(2,1);
  
  if(wkTens>0) pMatrix->print(wkTens);
  
  pMatrix->writeDisplay();

  iTargetMillis=iDelayMS+millis();
  /* implement this in the loop, so avoid using the delay
  delay(iDelayMS);

  // now clear
  pMatrix=&matrix0;
  pMatrix->clear();
  pMatrix->writeDisplay();

  pMatrix=&matrix1;
  pMatrix->clear();
  pMatrix->writeDisplay();
  */
}
// display the LFO destination as an LED on one of the last 4 horizontal rows
void displayLFODestination(int iDisplayIfUnchangedflag, int iClearFlag) {
  
  // display destinations for the 4 LFOs on the LED minimatrix
  Adafruit_8x16minimatrix *pMatrix; // pointer to matrix, cuz it could be either one

  if(1==iClearFlag) {
    matrix0.begin(MiniMatrixI2CAddr1to8);
    matrix0.clear();
    matrix0.writeDisplay(); 
    matrix1.begin(MiniMatrixI2CAddr9to16);
    matrix1.clear();
    matrix1.writeDisplay();
  }
  for(int iLFONum=0;iLFONum<LFO_COUNT;iLFONum++) {
    // select destination channel
    int iwkLFO_Destination=mvcLFOs[iLFONum].LFO_Destination>>4; // 0-15
    int wkY;  // used to select the row
    int wkX;  // used to select the column
    bool bLEDchangedFlag0=false;
    bool bLEDchangedFlag1=false;
    
    if( (iwkLFO_Destination!=prevDestination[iLFONum]) || (1==iDisplayIfUnchangedflag) ) { // dest changed, update it
      wkY=iLFONum+12;  // use the last 4 lines of the display
      
      #ifdef DEF_DEBUG_LFO_DISPLAY
        Serial.print(" LFO_Destination changed: ");
        Serial.println(iwkLFO_Destination);
      #endif  
      
      // undisplay the prev dest (if it is legit - 0-15 )
      if(prevDestination[iLFONum]<16) {
        // point to the right matrix 
        if(prevDestination[iLFONum]<8) { // matrix 0
          wkX=prevDestination[iLFONum];    // 0-7
          pMatrix=&matrix0; 
          // prep for I2C to 16x8 MiniMatrix LED
          pMatrix->begin(MiniMatrixI2CAddr1to8);  // pass in the address 
        }else{              // matrix 1
          wkX=prevDestination[iLFONum]-8; // 0-7
          pMatrix=&matrix1; 
          // prep for I2C to 16x8 MiniMatrix LED
          pMatrix->begin(MiniMatrixI2CAddr9to16);  // pass in the address   
        }
        #ifdef DEF_DEBUG_LFO_DISPLAY
          Serial.print("turn off  Y: ");  Serial.print(wkY);
          Serial.print("  X: ");          Serial.println(wkX);
        #endif 
        pMatrix->setRotation(0); 
        if((1==iClearFlag)&&(0==iLFONum)){
          pMatrix->clear();
        }
        // X & Y not are backwards here, since we have a 0-15 destination (X = column) and a calculated row ( based on LFO # )   
        pMatrix->drawPixel(wkX,wkY,LED_OFF);  
        pMatrix->writeDisplay();
      }
      prevDestination[iLFONum]=iwkLFO_Destination;  // update prev
                    
      // now turn on new LED

      // point to the right matrix      
      if(iwkLFO_Destination<8) { // matrix 0
        wkX=iwkLFO_Destination;    // 0-7
        pMatrix=&matrix0; 
        // prep for I2C to 16x8 MiniMatrix LED
        pMatrix->begin(MiniMatrixI2CAddr1to8);  // pass in the address  
        bLEDchangedFlag0=true;     
      }else{                      // matrix 1
        wkX=iwkLFO_Destination-8;  // 0-7
        pMatrix=&matrix1; 
        // prep for I2C to 16x8 MiniMatrix LED
        pMatrix->begin(MiniMatrixI2CAddr9to16);  // pass in the address 
        bLEDchangedFlag1=true;     
      }
      #ifdef DEF_DEBUG_LFO_DISPLAY
        Serial.print("turn on  Y: ");  Serial.print(wkY);
        Serial.print("  X: ");          Serial.println(wkX);
      #endif 
      if((1==iClearFlag)&&(0==iLFONum)){
          pMatrix->clear();
      }
      pMatrix->setRotation(0); 
      pMatrix->drawPixel(wkX,wkY,LED_ON);
      pMatrix->writeDisplay();     
    }  
  }
}
const int ciI2C_EEPROM_DEVICE_ADDRESS=80;    // write patch data to 24C256 eeprom (I2C address 80 = 0x50) at the correct address
const int ciBatchSize=16;  // get/put data 16 bytes at a time
    
void writeI2CEEPROM(int writeAddress,int batchSize,uint8_t *pData){ // pointer to data to be read
    uint8_t PatchAddressHigh=writeAddress>>8;
    uint8_t PatchAddressLow=writeAddress&0xFF;    // mask lower byte

    #ifdef DEF_DEBUG_I2C_EEPROM
      Serial.print("write address: ");
      Serial.println(writeAddress);
      Serial.print("PatchAddressLow: ");
      Serial.println(PatchAddressLow);
      Serial.print("PatchAddressHigh: ");
      Serial.println(PatchAddressHigh);
     // Serial.print("pData address: ");
     // Serial.println((uint8_t *)pData);  does not want to print the pointer value ... deal with this later
    #endif

    // write patch data to 24C256 eeprom (I2C address 80 = 0x50) at the correct address

    Wire.beginTransmission(ciI2C_EEPROM_DEVICE_ADDRESS);
    Wire.send(PatchAddressHigh);   // address high byte  Note: the order of address is backwards compared to the PJRC example
    Wire.send(PatchAddressLow);    // address low byte         but it matches the datasheet (and works!)

    for(int x=0;x<batchSize;x++){
      //Wire.send(mvcChannels[x].scInChannel);
      //Wire.send(mvcChannels[x].scTrim);
      Wire.send(*pData);  // any more send starts writing

#ifdef DEF_DEBUG_I2C_EEPROM
      Serial.print("Save pData: ");
      Serial.println(*pData);
#endif
      pData++;            // next byte
    }
    Wire.endTransmission();
}

void readI2CEEPROM(int readAddress,int batchSize,uint8_t *pData){ // pointer to data to be read
    uint8_t PatchAddressHigh=readAddress>>8;
    uint8_t PatchAddressLow=readAddress&0xFF;    // mask lower byte

    #ifdef DEF_DEBUG_I2C_EEPROM
      Serial.print("read address: ");
      Serial.println(readAddress);
      Serial.print("PatchAddressLow: ");
      Serial.println(PatchAddressLow);
      Serial.print("PatchAddressHigh: ");
      Serial.println(PatchAddressHigh);
     // Serial.print("pData address: ");
     // Serial.println((uint8_t *)pData);  does not want to print the pointer value ... deal with this later
    #endif
    Wire.beginTransmission(ciI2C_EEPROM_DEVICE_ADDRESS);
    Wire.send(PatchAddressHigh);   // address high byte  Note: the order of address is backwards compared to the PJRC example
    Wire.send(PatchAddressLow);    // address low byte         but it matches the datasheet (and works!)
    Wire.endTransmission();

    Wire.requestFrom(ciI2C_EEPROM_DEVICE_ADDRESS, batchSize);
    #ifdef DEF_DEBUG_I2C_EEPROM
      Serial.print("Requested bytes: ");
      Serial.println(batchSize);
    #endif

    while(Wire.available()) {
      *pData = Wire.receive();
    #ifdef DEF_DEBUG_I2C_EEPROM
      Serial.print("Load pData: ");
      Serial.println(*pData);
    #endif
      pData++;
    }
    Wire.endTransmission();
}

void writeProgram(int PatchNumber){ // patch number = 0 to 35  (vs 0 to 11 for banks)
  int PatchAddress=PatchNumber*PatchSize;  // calc starting addr for patch
  uint8_t PatchAddressHigh=PatchAddress>>8;
  uint8_t PatchAddressLow=PatchAddress&0xFF;    // mask lower byte
  #ifdef DEF_DEBUG_I2C_EEPROM
      Serial.println("BEGIN: read I2C System Data");
  #endif
  readI2CSystemData(); // get all system data from the ElectricDruid I2C network (e.g. DUALADSR) 
  #ifdef DEF_DEBUG_I2C_EEPROM
      Serial.println("END: read I2C System Data");
  #endif

  #ifdef DEF_DEBUG_I2C_EEPROM
      Serial.print("PatchAddress: ");
      Serial.println(PatchAddress);
      Serial.print("Write Patch: ");
      Serial.println(PatchNumber); 
      Serial.print("Patch size: ");
      Serial.println(PatchSize);    
  #endif 
    digitalWrite(LED_PIN, HIGH);             // Na

  #ifdef DEF_I2C_EEPROM
    // write "PatchSize" bytes, from address "PatchAddress" in batches
    uint8_t *pData;
    int idx;
    //pData=&mvcChannels[0].scInChannel; //point to first byte in first struct
    
    for(int batchNum=0;(batchNum*ciBatchSize)<PatchSize;batchNum++){
      // The problem here is that mvcChannels and mvcLFOs do NOT appear to be in contiguous memory space!
      // this is very brittle!!!  we're assuming a batch size of 16.  We're also assuming that we only want to write mvcChannels then mvcLFOs  
      switch(batchNum){
        case 0 :
          pData=&mvcChannels[0].scInChannel; //point to first byte in the first struct
          break;
        case 1 :
          pData=&mvcChannels[8].scInChannel; //point to first byte in the first struct
          break;    
        case 2 :    
          pData=&mvcLFOs[0].LFO_Rate;
          break;
        // support for dualADSR     
        case 3 :   
          pData=&dualADSRdata0[0];  // note that these 8 bytes are less than the batch size of 16 (wasted space).
          break;
        default:
          pData=&mvcChannels[0].scInChannel; // dummy default ... just point to some real data.
      }
  #ifdef DEF_DEBUG_I2C_EEPROM
      Serial.print("mvcChannels address: ");
      Serial.println((long)&mvcChannels[0].scInChannel,DEC);
      Serial.print("mvcLFOs address: ");      
      Serial.println((long)&mvcLFOs[0].LFO_Rate,DEC);
      Serial.print("mvcDUALADSR address: ");
      Serial.println((long)&dualADSRdata0[0], DEC );
      Serial.print("mvcDUALADSR address array: ");
      Serial.println((long)dualADSRdata0, DEC );
      Serial.print("data pointer: ");
      Serial.println((long)pData, DEC );  
      Serial.print("PatchAddress in EEPROM: ");
      Serial.println(PatchAddress, DEC ); 
  #endif
      // calc the address for the data
      /*
      idx=batchNum*8;                      // this is fairly brittle.  It only works for the mvcChannels data.
      pData=&mvcChannels[idx].scInChannel; //point to first byte in struct
      */
      // TODO: enable writes once we get the I2C comms working
      //#ifndef DUALADSR 
      writeI2CEEPROM(PatchAddress,ciBatchSize,pData);
      //#endif
      // update local variables for the next batch
      PatchAddress+=ciBatchSize;
      /*
       * 
       *NOTE: pData gets incremented w/in the write routine!
       *  
      */
      // this does not seem to work!!!
      //pData+=ciBatchSize;        // increment the data by the batchsize
      //pData=(uint8_t *)pData+ciBatchSize;  // cast to be sure we're incrementing by a number of bytes
      delay(5);  // 5 ms write time for 24LC256!!!
    }
    #ifdef DEF_DEBUG_I2C_EEPROM
      printMVCValues(); // show what we got
    #endif
  #endif

  #ifdef DEF_INTERNAL_EEPROM
    EEPROM.put(PatchAddress, mvcChannels);
  #endif
    /* PROGMEM skip this for now. I'm not sure if this data disappears when we recomile & reload (I feel sure it does). 
    for(int x=0;x<cOutChannelCount;x++) {
      patches[PatchNumber][x]=mvcChannels[x];
    }
    */
    delay(LEDOnDelay);
    digitalWrite(LED_PIN, LOW);              // Noo
    delay(LEDOnDelay);
    digitalWrite(LED_PIN, HIGH);             // Na
    delay(LEDOnDelay);
    digitalWrite(LED_PIN, LOW);              // Noo

    // show which program we wrote
    displayPatch(500,1,PatchNumber);  // MS delay; display if unchanged - yes!
    // this will happen in the service routine for the timer
    //updateLEDfromModel(cMode1to8,true); // model,redisplay
    //updateLEDfromModel(cMode9to16,true);  
}

  // declare this here for use by loadProgram
  byte wkMIDICC=0;
  byte wkByte=0;
  float wkFloat=0;  // used to multiply
  
void loadProgram(int PatchNumber){ // patch number = 0 to 35  (vs 0 to 11 for banks)
    int PatchAddress=PatchNumber*PatchSize;  // calc starting addr for patch
      
  #ifdef DEF_DEBUG_I2C_EEPROM
      Serial.print("PatchAddress: ");
      Serial.println(PatchAddress);
      Serial.print("Load Patch: ");
      Serial.println(PatchNumber); 
      Serial.print("Patch Size: ");
      Serial.println(PatchSize); 
  #endif
    // signal the read on the LED 
    digitalWrite(LED_PIN, HIGH);             // Na
    
    // read "PatchSize" bytes, from address "PatchAddress"
    uint8_t *pData;
    int idx;
    //pData=&mvcChannels[0].scInChannel; //point to first byte in the first struct
    
  #ifdef DEF_I2C_EEPROM
    for(int batchNum=0;(batchNum*ciBatchSize)<PatchSize;batchNum++){
      // The problem here is that mvcChannels and mvcLFOs do NOT appear to be in contiguous memory space!
      // this is very brittle!!!  we're assuming a batch size of 16.  We're also assuming that we only want to write mvcChannels then mvcLFOs  
      switch(batchNum){
        case 0 :
          pData=&mvcChannels[0].scInChannel; //point to first byte in the first struct
          break;
        case 1 :
          pData=&mvcChannels[8].scInChannel; //point to first byte in the first struct
          break;    
        case 2 :    
          pData=&mvcLFOs[0].LFO_Rate;
          break; 
        // support for dualADSR     
        case 3 :    
          pData=&dualADSRdata0[0];  // note that these 8 bytes are less than the batch size of 16 (wasted space).
          break;
        default:
          pData=&mvcChannels[0].scInChannel; // dummy - just point to a real memory address
      }
  #ifdef DEF_DEBUG_I2C_EEPROM
      Serial.print("mvcChannels address: ");
      Serial.println((long)&mvcChannels[0].scInChannel,DEC);
      Serial.print("mvcLFOs address: ");      
      Serial.println((long)&mvcLFOs[0].LFO_Rate,DEC);
      Serial.print("mvcDUALADSR address: ");
      Serial.println((long)&dualADSRdata0[0], DEC );
      Serial.print("data pointer: ");
      Serial.println((long)pData, DEC ); 
      Serial.print("PatchAddress in EEPROM: ");
      Serial.println(PatchAddress, DEC ); 
  #endif  
      // calc the address for the data
      /*
      idx=batchNum*8;       // this is brittle.  it assumes on each index being 2 bytes and the batchsize being 16
      pData=&mvcChannels[idx].scInChannel; //point to first byte in struct
      */

      #ifdef DEBUG_DUALADSR 
        //Serial.println("Begin read I2C System Data" );  
        //readI2CSystemData();
        //Serial.println("End read I2C System Data" );  
      #endif
      // TODO: enable read once we get the I2C comms working
      //#ifndef DUALADSR 
      readI2CEEPROM(PatchAddress,ciBatchSize,pData);
      //#endif

      // update local variables for the next batch
      PatchAddress+=ciBatchSize;
      /*
       *NOTE: pData gets incremented w/in the write routine! 
      */
      //pData=(uint8_t *)pData+ciBatchSize;  // cast to be sure we're incrementing by a number of bytes
      
    }
    
    #ifdef DEF_DEBUG_I2C_EEPROM
      printMVCValues(); // show what we got
    #endif
    
  #endif
  #ifdef DEF_INTERNAL_EEPROM
    EEPROM.get(PatchAddress, mvcChannels); 
  #endif  
    delay(LEDOnDelay);
    digitalWrite(LED_PIN, LOW);              // Noo
    delay(LEDOnDelay);
    digitalWrite(LED_PIN, HIGH);             // Na
    delay(LEDOnDelay);
    digitalWrite(LED_PIN, LOW);              // Noo
    // I know the program values did not come from MIDI, but imagine they did - :)
    // MIDI is the "priority channel"

    initModelRelatedVariables();
    // update DAC and Matrix Switch from model
    // this happens below with updateLEDfromModel() and UpdateTrimsView()
    /*
    for(int i=0;i<cOutChannelCount;i++){
       // init the VCA/mixer to 0.  Update it in the UpdateTrimsView() below
       if(i<8){
          writeDAC528(DAC_1to8,faderChToDacCh[i],wkByte);
       }else{
          writeDAC528(DAC_9to16,faderChToDacCh[i],wkByte); // and the second board 
       }
    */ 
       /* test only, fake a MIDI CC message */
       /* rework this  when the time comes
       wkMIDICC=i+MIDIbaseCC;
       wkFloat=mvcChannels[i].scInChannel;  // use float for multiplication 
       wkFloat=(wkFloat*scale4bitTO7bit)+1; // for some reason we have to add 1 for this to work, otherwise an incoming value of 0 reflects as a 1 
       wkByte=wkFloat;  // make it a byte
       */
    /*
       // TODO - rework this
       //OnMIDIControlChange(2, wkMIDICC, wkByte); // update the Matrix Switch    
    }
    */

    // update the display (later in the loop)
    updateLEDfromModel(cMode1to8,true); // model,redisplay
    updateLEDfromModel(cMode9to16,true);
    UpdateTrimsView();
  #ifdef DUALADSR 
     writeI2CSystemData();
  #endif      
    // do the program display after model update
    // show which program we loaded
    displayPatch(500,1,PatchNumber);  // MS delay; display if unchanged - yes!
}

/* END - EEPROM patch storage support */

void initModelRelatedVariables(){
    // sync the variables to the scInChannel data
    for(int y=0;y<MAX_ROUTES;y++) {
      // while we have the index, reset these flags
      RoutesTakeoverFlag[y]=false;        
      TrimsTakeoverFlag[y]=false;
      if(y<MAX_MIDFADERS) { LFOsTakeoverFlag0[y]=false; LFOsTakeoverFlag1[y]=false; }
      prevX[y]=16;  // initialize the "previous X value" to a bogus value for miniMatrix purposes so a 0 will display
    } 
    for(int y=0;y<LFO_COUNT;y++){
      if(mvcLFOs[y].LFO_Wave > 7) {mvcLFOs[y].LFO_Wave=0;} // reset an out-of-range wave
      prevDestination[y]=mvcLFOs[y].LFO_Destination; 
      prevWave[y]=mvcLFOs[y].LFO_Wave; 
    }   
}
void writeFaderLEDs(int i2c_addr_fader,uint8_t iMSByte,uint8_t iLSByte){
    // update LED data
      // send LED data
      #ifdef DEF_PRINT_FADERBOARD_TRACE
      #ifdef DEF_SERIAL_OUT
      Serial.print ("Sending LED data: ");
      Serial.println (iLSByte);
      #endif
      #endif
      //begin

        // load the buffer 
        writeBuffer[0]=cPointerByteWriteLED;
        writeBuffer[1]=iMSByte;
        writeBuffer[2]=iLSByte; 

      //write
        Wire.beginTransmission(i2c_addr_fader);
        Wire.write(writeBuffer,3);
        Wire.endTransmission();
}
// eye candy on faders
void ledFaderChase() {

    int i2c_addr_fader=i2c_addr_fader0;// fader submodule A0
    uint8_t mvcMSByte;
    uint8_t mvcLSByte;
    int iChaseDelayMS=50;
    bool bFirstTimeFlag=true;

    // back to all leds off
    writeFaderLEDs(i2c_addr_fader0,0,0); 
    writeFaderLEDs(i2c_addr_fader1,0,0);  

    for(int z=0;z<3;z++){
      bFirstTimeFlag=true;
      FaderBoard0LEDs=3; // the bit layout is backwards.   03 = the two LSBs on.  That corresponds to fader 1 which is leftmost
      FaderBoard1LEDs=3;   
      for(int x=0;x<20;x++){
        if(x<10) { // first board
          i2c_addr_fader=i2c_addr_fader0;        
          mvcMSByte=FaderBoard0LEDs>>8;     // MSB
          mvcLSByte=FaderBoard0LEDs&0xFF;   // LSB
          writeFaderLEDs(i2c_addr_fader,mvcMSByte,mvcLSByte);
          // also write to first ADSR
          writeFaderLEDs(i2c_addr_dualADSR0,mvcMSByte,mvcLSByte);
          delay(iChaseDelayMS); 
          FaderBoard0LEDs=FaderBoard0LEDs<<2; 
          if(x<2) FaderBoard0LEDs=FaderBoard0LEDs|0x02; // add the ghost bit(s)
        }else{
          if(bFirstTimeFlag) { 
              bFirstTimeFlag=false;       
              writeFaderLEDs(i2c_addr_fader,0,0);
              i2c_addr_fader=i2c_addr_fader1;              // change to board 2
          } 
          mvcMSByte=FaderBoard1LEDs>>8;     // MSB
          mvcLSByte=FaderBoard1LEDs&0xFF;   // LSB
          writeFaderLEDs(i2c_addr_fader,mvcMSByte,mvcLSByte);
          // also write to first ADSR
          writeFaderLEDs(i2c_addr_dualADSR0,mvcMSByte,mvcLSByte);
          delay(iChaseDelayMS); 
          FaderBoard1LEDs=FaderBoard1LEDs<<2; 
          if(x<12) FaderBoard1LEDs=FaderBoard1LEDs|0x02; // add the ghost bit(s)
        }
      } // x
      writeFaderLEDs(i2c_addr_fader,0,0); // cleanup last ghost bit     
    } // z

    // back to all leds on bright
    writeFaderLEDs(i2c_addr_fader0,255,255); 
    writeFaderLEDs(i2c_addr_fader1,255,255);   
}

void setup() {
  analogReference(EXTERNAL); // we have an external 2.5v reference on the Matrix Switch controller 
  pinMode(PATCH_IN_PIN, INPUT);
  pinMode(MODE_SWITCH_PIN, INPUT);
  pinMode(SAVE_LOAD_SWITCH_PIN, INPUT);
  pinMode(AD75019_PCLK_PIN, OUTPUT); // to AD75019 PCLK
  pinMode(AD75019_SCLK_PIN, OUTPUT); // to AD75019 SCLK
  pinMode(AD75019_SIN_PIN, OUTPUT);  // to AD75019 SIN
  // wrong!   pinMode(TEENSY_DAC_PIN, OUTPUT);   // to Teensy DAC_9to16
  // instead, do this:     https://forum.pjrc.com/threads/40259-Teensy-LC-DAC-faulty-voltage
  analogWriteResolution(12); // 12 bit resolution
  digitalWrite(AD75019_PCLK_PIN, LOW); 
  
  // turn on LED before doing any "real" operations, to show that the Teensy has booted
  pinMode(LED_PIN, OUTPUT);            // Set digital pin 13 -> output
  digitalWrite(LED_PIN, HIGH);          // Pin 13 = 5 V, LED emits light
  delay(200);   

  // VCA/MIX board (8 channel version)
  pinMode(CS0, OUTPUT);
  pinMode(CS1, OUTPUT);
  pinMode(DACCLK_PIN, OUTPUT);
  pinMode(DACDATA_PIN, OUTPUT);


  // test code for pins 22 & 23
 /* 
  //pinMode(21, OUTPUT);
  pinMode(22, OUTPUT);
  pinMode(23, OUTPUT);
  
  for(int x=0; x< 10000;x++) {
  //  digitalWrite(21, HIGH);     
    digitalWrite(22, HIGH); 
    digitalWrite(23, HIGH);

    delay(1);
 //   digitalWrite(21, LOW);
    digitalWrite(22, LOW); 
    digitalWrite(23, LOW);    
    delay(1);
  }  
  */
  // end test code
  
  initDAC528(DAC_1to8);   // board 0
  initDAC528(DAC_9to16);  // board 1  

  initLEDBits();
  
  for(int y=0;y<cFaderCount;y++) {
    // for Route & Trim
    FaderBoard0[y]=0;
    prevFaderBoard0[y]=FaderBoard0[y];
    FaderBoard1[y]=0;
    prevFaderBoard1[y]=FaderBoard1[y];
    // for LFOs
    LFOFaderBoard0[y]=0;
    prevLFOFaderBoard0[y]=LFOFaderBoard0[y];
    LFOFaderBoard1[y]=0;
    prevLFOFaderBoard1[y]=LFOFaderBoard1[y];
  }

  // initialize to 0
  for(int y=0;y<MAX_ROUTES;y++){
      mvcChannels[y].scInChannel=0;
      mvcChannels[y].scTrim=0;
  }
  // now sync all working variables to the model data
  initModelRelatedVariables();

  // initialize to 0
  for(int y=0;y<MAX_ROUTES;y++){  // initModelRelatedVariables routine did not do this correctly for initial time
      RoutesTakeoverFlag[y]=true;        
      TrimsTakeoverFlag[y]=true;
      if(y<MAX_MIDFADERS) { LFOsTakeoverFlag0[y]=true; LFOsTakeoverFlag1[y]=true; }
  }
  
#ifdef DEF_SERIAL_OUT
  iPitchTargetMillis=millis()+250; // limit wait for serial to a quarter of a second (250 milliseconds)
  while ((!Serial)&&(millis()<iPitchTargetMillis));
  Serial.begin(115200);  // originally 9600
  Serial.println("16x16 Switch Matrix v5.2");
  Serial.print("Patchsize: ");
  Serial.println(PatchSize);
#endif

  pinMode(LED_PIN, OUTPUT);            // Set digital pin 13 -> output
  digitalWrite(LED_PIN, HIGH);          // Pin 13 = 5 V, LED emits light
  delay(200);                      // ..for 0.5 seconds
  digitalWrite(LED_PIN, LOW);           // Pin 13 = 0 V, LED no light
  delay(200);     
  digitalWrite(LED_PIN, HIGH);          // Pin 13 = 5 V, LED emits light
  delay(200);                      // ..for 0.5 seconds
  digitalWrite(LED_PIN, LOW);           // Pin 13 = 0 V, LED no light
  delay(200);     
  digitalWrite(LED_PIN, HIGH);          // Pin 13 = 5 V, LED emits light
  delay(500);                      // ..for 0.5 seconds
  digitalWrite(LED_PIN, LOW);           // Pin 13 = 0 V, LED no light
  delay(500);     
  // if we flash the lights, we don't need to wait
  //delay(3000);           // wait 3 seconds for fader board to boot up

#ifdef DEF_MINIMATRIX_ACTIVE
  // this works to drive the miniMatix 16x8 (note: does not matter which order these to go)
  matrix0.begin(MiniMatrixI2CAddr1to8);   // pass in the address
  matrix1.begin(MiniMatrixI2CAddr9to16);  // pass in the address
#else
  #ifdef DEF_FADERBOARD_ACTIVE 
    Wire.begin();        // this may be redundant to the matrix.begin(...) below but it does not prevent the matrix code from working
  #endif
#endif
  Wire.setClock(100000L); // I don't think that the LEDs will work well with 400000L

    matrix0.begin(MiniMatrixI2CAddr1to8);  // pass in the address 
    matrix0.clear();
    // write the changes we just made to the display
    matrix0.writeDisplay(); 

    matrix1.begin(MiniMatrixI2CAddr9to16);  // pass in the address
    matrix1.clear();
    // write the changes we just made to the display
    matrix1.writeDisplay();

    ledFaderChase(); // eye candy
    
    // LFO 
    setupLFO();
    // prepare interrupt servicing
    TimerSetup();
} // end setup()

const int ciMaxInputBit=8; // just to 1-8 for now
int iInputBitCntr=1;
int iFaderReadLoopCount=25;
int iModeSwitch=0;
int iPrevModeSwitch=0;

int iPatchNumber=0;
int iRawPatchNumber=1;
int iwkRawPatchNumber=1;
int iPrevPatchNumber=10;
int iSaveLoadSwitch=cSwitchMidBreak+2;
int iPrevSaveLoadSwitch=cSwitchMidBreak+2;
bool bSLSFirstTimeFlag=true;

const int cReadFaderInterval=3;

int iMillisNow;
int iFaderMode=cMode1to8;
int iPrevFaderMode=cMode1to8;

void readI2CSystemData() { // typically called just before a write program
      // ultimately this list of modules should be dynamic.  for development, hardcode the list
      // DualADSR is configured like the fader boards for I2C purposes

      readFadersW1(i2c_addr_dualADSR0,dualADSRdata0,cADSRdataBytes,false);  // route faders
    
}
void writeI2CSystemData() { // typically called just after a load program
      // ultimately this list of modules should be dynamic.  for development, hardcode the list
      // DualADSR is configured like the fader boards for I2C purposes

      writeFadersW1(i2c_addr_dualADSR0,dualADSRdata0,cADSRdataBytes);  // route faders
}
void loop() {
  // LFO interrupt code
  // to read a variable which the interrupt code writes, we
  // must temporarily disable interrupts, to be sure it will
  // not change while we are reading.  To minimize the time
  // with interrupts off, just quickly make a copy, and then
  // use the copy while allowing the interrupt to keep working.
  //Serial.print("new value for iRawPatchNumber: ");
  //Serial.println(iRawPatchNumber);  
  // this works to drive the rate from the Patch

  int iLFONum=0;
  for(iLFONum=0;iLFONum<LFO_COUNT;iLFONum++) {
    iLFORate[iLFONum]=(63-(mvcLFOs[iLFONum].LFO_Rate)); // chg 0-63 to 63-0 (note: 0 is legit, since we have an exponential lookup table)
  }

  noInterrupts();
  for(int x=0;x<LFO_COUNT;x++) {
    if(true==bserviceMyTimer0Flag[x]) {
      bserviceMyTimer0Flag[x]=false;  // reset flag
      iwkLFOIndex[x]=iLFOIndex[x];     // make a copy, for interrupt stability
    }
  }
  interrupts();
  bool bDestinationChangedFlag=false;
  for(iLFONum=0;iLFONum<LFO_COUNT;iLFONum++) {
      if(iwkPrevLFOIndex[iLFONum]!=iwkLFOIndex[iLFONum]) { // we got a change, service it
        iwkPrevLFOIndex[iLFONum]=iwkLFOIndex[iLFONum];
        #ifdef DEF_DEBUG_LFO
          Serial.print("new value for iLFORate: ");
          Serial.println(iLFORate[iLFONum]); 
        #endif 
        // select wave table
        int iLFO_Wave=mvcLFOs[iLFONum].LFO_Wave;  // 3 wave bits
    
        // select destination channel
        int iwkLFO_Destination=mvcLFOs[iLFONum].LFO_Destination>>4; // 0-15
        if(iwkLFO_Destination!=prevDestination[iLFONum]) {
          bDestinationChangedFlag=true;
          #ifdef DEF_DEBUG_LFO_DISPLAY
              Serial.print("LFO Destination Changed ");
              Serial.print(iLFONum);
          #endif
        }
        #ifdef DEF_DEBUG_LFO
          Serial.print("LFO_Destination: ");
          Serial.println(iwkLFO_Destination);
        #endif
        // and the LFO modulation level
        float fwkLFO_Level=mvcLFOs[iLFONum].LFO_Level;  // 0-255
        fwkLFO_Level=fwkLFO_Level/255;                  // 0-1.0
        #ifdef DEF_DEBUG_LFO
          Serial.print("Mod Level: ");
          Serial.println(fwkLFO_Level,DEC);
        #endif
        // get the current lfo sample value
        fWkSample=(u8WaveTables[iLFO_Wave][ iwkLFOIndex[iLFONum] ]); // get the current sample value
        fWkSample=128-fWkSample; // invert it and make it bi-polar (note: this manipulation has been verified via spreadsheet)
        // scale by LFO Mod Amount
        fWkSample=fWkSample*fwkLFO_Level;               // 0-1.0 multiplier
        // add in the initial amount
        if(mvcLFOs[iLFONum].LFO_Level > LFO_LEVEL_THRESHOLD) { // don't process if we don't have a level above the threshold
          fWkSample+=mvcChannels[iwkLFO_Destination].scTrim; // scTrim is an inverted amount (0=unity gain)
        }
        /*  don't need this.  it happens below with the scTrim service
         else{
          fWkSample=mvcChannels[iwkLFO_Destination].scTrim;  // else set it to the initial amount
        }
        */
       
        // be sure we're in the 0-255 range
        if (fWkSample>255) { 
            fWkSample=255;  
            #ifdef DEF_DBG_LFO_OVERFLOW
              Serial.println("LFO level overflow (> 255) "); 
            #endif
        }
        // with bi-polar modulation, this could happen
        if (fWkSample<0) { 
            fWkSample=0;  
            #ifdef DEF_DBG_LFO_OVERFLOW
              Serial.println("LFO level underflow (< 0) "); 
            #endif
        }
        ui8WkSample=(uint8_t)fWkSample;
      
        #ifdef DEF_DEBUG_LFO_VALUE 
          //if (15==iwkLFO_Destination) { // TODO: remove this IF after debugging
          //  Serial.print("new value for Channel 15 Volume: ");
          // note: the debugging showed that this code is doing exactly what it needs to do
            Serial.print("new value for Channel Volume: ");
            Serial.println(ui8WkSample);
          }
        #endif
        // update DAC
        if(ui8PrevWkSample!=ui8WkSample) {
          ui8PrevWkSample=ui8WkSample;
          // figure out which board & channel to use
          if(iwkLFO_Destination<8) {
            //           board    chnl(0-7)         data(0-255)
            #ifdef SM_PROTOTYPE_HARDWARE
              writeDAC528(DAC_1to8,faderChToDacCh[iwkLFO_Destination],ui8WkSample);
            #else
              writeDAC528(DAC_1to8,iwkLFO_Destination,ui8WkSample);
            #endif
          }else{
            //           board    chnl(0-7)         data(0-255)
            #ifdef SM_PROTOTYPE_HARDWARE
              writeDAC528(DAC_9to16,faderChToDacCh[iwkLFO_Destination-8],ui8WkSample);
            #else
              writeDAC528(DAC_9to16,(iwkLFO_Destination-8),ui8WkSample);
            #endif
          }
        }
        
        //showInterrupt();   // this works  
        //Serial.print("new value for iwkLFOIndex: ");
        //Serial.println(iwkLFOIndex[iLFONum);  
      }
  }
  // detect a destination change
  if( (true==bDestinationChangedFlag)&&(iFaderMode==cModeMid) ){
    displayLFODestination(1,0);  // display if unchanged & clear flag

    // test only!!!  TODO: remove this
    //showSin();
  }
  // END LFO interrupt code
  
#ifdef DEF_VCA_BOARD_TEST
  // hardware the first trim fader to the first board/first DAC for testing
  runVCAHardwareTest();
#endif
  

  
  //test the switch
  iModeSwitch=analogRead(MODE_SWITCH_PIN);
  iModeSwitch=iModeSwitch>>cSwitchShiftNum;  // just care about big picture (changed from 5 to 6 @ v3.7)

  // check transition & start clock
  bool processModeTransitionFlag=false;
  if(iPrevModeSwitch != iModeSwitch) {  // we have a transition
    if(0==iMidModeMillis) {  // see if timer is active
      iMidModeMillis=millis()+150; // debounce for 100 ms, in case we are going from 1-8 through MID mode to 9-16 (or vice verse)
    }else{
      if(millis()>iMidModeMillis) { // timer has completed
        iMidModeMillis=0;
        processModeTransitionFlag=true;
      } 
    }
  }

  if( (iPrevModeSwitch != iModeSwitch)&&(true==processModeTransitionFlag) ){ // be sure we have waited for the "debounce" period
       initModelRelatedVariables();  // reset the active flags, as if we loaded a new program.  this will cause "takeover" behavior.

       iPrevModeSwitch=iModeSwitch;
  #ifdef DEF_SERIAL_OUT
       Serial.println(iPrevModeSwitch);
  #endif
       iPrevFaderMode=iFaderMode;
       if(iPrevModeSwitch>cSwitchMidBreak)
       {
        iFaderMode=cModeMid;
        displayLFODestination(1,1);  // display if unchanged & clear flag
        displayLFOWaveforms();
  #ifdef DEF_SERIAL_OUT
        Serial.println("switch is mid");
  #endif
       }else{  
         // on transition from MID mode, trigger the redisplay of the routing detail
         if(cModeMid==iPrevFaderMode){ 
            if(0== iTargetMillis) { 
              iTargetMillis=millis();  // this will trigger redisplay
            }
         }
         if(iPrevModeSwitch>cSwitchLoHiBreak){
            iFaderMode=cMode1to8; // seems backwards but the upward switch position is labled 1-8
            //Serial.println("switch is high"); 
         }else{
            iFaderMode=cMode9to16;
            //Serial.println("switch is low");
         }  
       }
       // detect and deal with a change
       if(iPrevFaderMode!=iFaderMode) {
        if(cModeMid==iFaderMode){
          // DEBUG: this should have happened at any mode transition in initModelRelatedVariables()
          for(int x=0;x<MAX_MIDFADERS;x++) { LFOsTakeoverFlag0[x]=false; LFOsTakeoverFlag1[x]=false;}
        }else{
          // faders for cMode1to8 and cMode9to16
          int strt=0;  // assume cMode1to8
          if(cMode9to16==iFaderMode) strt=8;
          // DEBUG: this should have happened at any mode transition in initModelRelatedVariables()
          for(int x=strt;x<strt+8;x++){
              RoutesTakeoverFlag[x]=false; // reset the flag
              TrimsTakeoverFlag[x]=false;
          }
        }
       } // mode change
  }

  iPatchNumber=analogRead(PATCH_IN_PIN);
  iRawPatchNumber=iPatchNumber;
  wkFloat=iPatchNumber;
  wkFloat=wkFloat/patchDivisor;
  if(cModeMid==iFaderMode) { 
    // process patch knob as the controller for the DAC CV routed to channel 16 in
    int wkDacValue=iPatchNumber;
    iPatchNumber=analogRead(PATCH_IN_PIN);
    wkDacValue+=iPatchNumber;   // average the value for stability
    wkDacValue=wkDacValue>>1;   // divide by 2 
    if( ((wkDacValue>=mvcPrevTeensyDACValue-ciDACSoftTakeoverRange) && 
         (wkDacValue<=mvcPrevTeensyDACValue+ciDACSoftTakeoverRange) ) 
        || true==bPatchPitchPotActive) {
      // process a changed value
      bPatchPitchPotActive=true;
      iPitchTargetMillis=500+millis();  // set a 500 MS from now "deadline" for active
      #ifdef DEF_SERIAL_OUT
      // Serial.print("new DAC value: ");  Serial.println(wkDacValue,DEC);
      #endif
      mvcTeensyDACValue=wkDacValue;
      mvcPrevTeensyDACValue=mvcTeensyDACValue; // update prev
      wkDacValue=wkDacValue<<2;   //convert from 10 bits to 12 bits
      #ifdef OZH702_DBG_TEENSY_DAC_OUT
       Serial.print("output DAC value: ");  Serial.println(wkDacValue,DEC);
      #endif
      analogWrite(TEENSY_DAC_PIN,wkDacValue); 
    } 
  }else{ 
    bPatchPitchPotActive=false;  // no longer in Pitch mode
    iPatchNumber=wkFloat;  // divide down to 
    if(iPrevPatchNumber!=iPatchNumber){
      // process a change
      iPrevPatchNumber=iPatchNumber;
      #ifdef DEF_SERIAL_OUT
      Serial.print("New Patch Number: ");
      Serial.println(iPatchNumber);
      #endif
      displayPatch(500,1,iPatchNumber);
    }
  }
  iMillisNow=millis();
  // implement a loose timer in the loop for the Patch pot being an active Pitch control
  if(iPitchTargetMillis>0) {
    if(iMillisNow>iPitchTargetMillis){ // timer is up
      bPatchPitchPotActive=false;  // no longer active.  Must "takeover" to becomve active
      iPitchTargetMillis=0;
    }
  }
  // implement a loose timer in the loop for the display (remove "delay" in displayPatch()
  if(iTargetMillis>0) {
    if(iMillisNow>iTargetMillis){ // timer is up
      Adafruit_8x16minimatrix *pMatrix; // pointer to matrix, cuz it could be either one
 
      iTargetMillis=0;    // reset the target 

      //clear the displays
      pMatrix=&matrix0;
      //pMatrix->begin(MiniMatrixI2CAddr1to8);
      pMatrix->clear();
      pMatrix->writeDisplay();
    
      pMatrix=&matrix1;
      //pMatrix->begin(MiniMatrixI2CAddr9to16);
      pMatrix->clear();
      pMatrix->writeDisplay();
      
      updateLEDfromModel(cMode1to8,true); // model,redisplay
      updateLEDfromModel(cMode9to16,true); 
    } 
  }  
  
  // write/load
  iSaveLoadSwitch=analogRead(SAVE_LOAD_SWITCH_PIN);
  iSaveLoadSwitch=iSaveLoadSwitch>>cSwitchShiftNum;  // just care about big picture 
  if(true==bSLSFirstTimeFlag) {
    bSLSFirstTimeFlag=false;
    iPrevSaveLoadSwitch=iSaveLoadSwitch;
  }
  if(iPrevSaveLoadSwitch != iSaveLoadSwitch){
       #ifdef DEF_SERIAL_OUT
       Serial.println(iSaveLoadSwitch);
       #endif
       if(iSaveLoadSwitch>cSwitchMidBreak)
       {
        #ifdef DEF_SERIAL_OUT
        Serial.println("switch is mid");
        #endif
        // process from the previous state
        if(iPrevSaveLoadSwitch>cSwitchLoHiBreak){
          #ifdef DEF_SERIAL_OUT
          Serial.println("switch is Write"); 
          #endif
          // write patch
          if(cModeMid!=iFaderMode)  // if we are in Mid Mode the Patch knob is for Pitch, not for Patch number!!! 
            writeProgram(iPatchNumber);
        }else{
          #ifdef DEF_SERIAL_OUT
          Serial.println("switch is Load");
          #endif
          // load patch
          if(cModeMid!=iFaderMode)  // if we are in Mid Mode the Patch knob is for Pitch, not for Patch number!!! 
            loadProgram(iPatchNumber);
        }
       }
       delay(1);  // debounce
       iPrevSaveLoadSwitch=iSaveLoadSwitch;  
  }
  // test writing to AD75019
  // switchTestRoutine1(); // switch out 1 from inputs as follows:  0,1,0,2,0,3,0,4,0,5,0,6,0,7 (repeat)
  bool bFaderChangedFlag=false;
  bool bLFOFaderChangedFlag=false;
  int wkInt;
  int wkFaderValue0,wkPrevFaderValue0;
  int wkFaderValue1,wkPrevFaderValue1;
  digitalWrite(LED_PIN, LOW);  // Pin 13 = HIGH, LED emits light  
  // only read faders so often
  iFaderReadLoopCount++;
  if(iFaderReadLoopCount>cReadFaderInterval){
    iFaderReadLoopCount=0;
    // Route & Trim modes
    if ((cMode1to8==iFaderMode)||(cMode9to16==iFaderMode)){
      // process either 1-8 or 9-16 depending on Mode Switch
      int y;
      int maxy;
      if(cMode1to8==iFaderMode)    y=0; 
      if(cMode9to16==iFaderMode)   y=8; 
      maxy=(y+(cFaderCount/2));   
      for(;y<maxy;y++) {
        // save prev
        prevFaderBoard0[y]=FaderBoard0[y];
        prevFaderBoard1[y]=FaderBoard1[y];
      }
      // read faders
      readFaders(i2c_addr_fader0,FaderBoard0,cFaderCount/2);  // route faders
      readFaders(i2c_addr_fader1,FaderBoard1,cFaderCount/2);  // trim faders
    }else{ // cModeMid
      for(int y=0;y<MAX_MIDFADERS;y++) {
        // save prev
        prevLFOFaderBoard0[y]=LFOFaderBoard0[y];
        prevLFOFaderBoard1[y]=LFOFaderBoard1[y];
      }   
      //Serial.println("Read Mid Faders"); 
      // read faders
      readFaders(i2c_addr_fader0,LFOFaderBoard0,MAX_MIDFADERS);  // LFO faders
      readFaders(i2c_addr_fader1,LFOFaderBoard1,MAX_MIDFADERS);  // LFO faders  
    }
  }

  // see if anything changed
  // a value of 4 says divide by 2 to the 4th power (16) - only have 16 valid positions per fader
  // That's exceptionally bad, indicating a LOT of noise for some reason (we're crushing the noise with a sledge hammer)
  // TODO: determine if this noise problem has been solved (I think it has).  (v3.7 it appears to be fixed)
  const int cSoftTakeoverRangeRoutes = 0; // since the values are 0-15, require an exact match
  const int cSoftTakeoverRangeTrims = 10; // +/- this value allows a MIDI updated value to be panel controlled;  also manages toggling 1-8 & 9-16
  const int cSoftTakeoverRangeLFOs = 10; // +/- this value allows a MIDI updated value to be panel controlled;  also manages toggling 1-8 & 9-16
  const int cSoftTakeoverRangeLFORates = 3; // +/- this value allows a MIDI updated value to be panel controlled;  also manages toggling 1-8 & 9-16
  const int cSoftTakeoverRangeLFOWaves = 0; // since the values are 0-7, require an exact match
  const int cRoutesTolerance=3; 
  const int cTrimsTolerance=1;   // finest resoluation is 0-127
  const int cLFOsTolerance=2;    // finest resolution is 0-63
  int y;
  int maxy;

  if(cModeMid==iFaderMode){
        for(int y=0;y<MAX_MIDFADERS;y++) {
          wkFaderValue0 = LFOFaderBoard0[y]>>cLFOsTolerance;
          wkPrevFaderValue0 = prevLFOFaderBoard0[y]>>cLFOsTolerance;
          wkFaderValue1 = LFOFaderBoard1[y]>>cLFOsTolerance;
          wkPrevFaderValue1 = prevLFOFaderBoard1[y]>>cLFOsTolerance; 

          if( ( wkPrevFaderValue0!=wkFaderValue0 ) || 
              ( wkPrevFaderValue1!=wkFaderValue1 )   
          )
          {
              bLFOFaderChangedFlag=true;
          }
        }   
  }
  if((cMode1to8==iFaderMode)||(cMode9to16==iFaderMode)){
      if(cMode1to8==iFaderMode)    y=0; 
      if(cMode9to16==iFaderMode)   y=8; 
      maxy=(y+(cFaderCount/2));   
      for(;y<maxy;y++) {
        wkFaderValue0 = FaderBoard0[y]>>cRoutesTolerance;
        wkPrevFaderValue0 = prevFaderBoard0[y]>>cRoutesTolerance;
        wkFaderValue1 = FaderBoard1[y]>>cTrimsTolerance;
        wkPrevFaderValue1 = prevFaderBoard1[y]>>cTrimsTolerance;
    
        if( ( wkPrevFaderValue0!=wkFaderValue0 ) || 
            ( wkPrevFaderValue1!=wkFaderValue1 )   
          )
        {
            bFaderChangedFlag=true;
        }
      }
  }

  // service the change
  bool bChangedFlag=false;
  bool bChangedWaveFlag=false;
  if(true==bLFOFaderChangedFlag){   // LFO faders
    // update LFOs
    bChangedFlag=false; // reset flag

    // validate mode
    if(cModeMid==iFaderMode) {
        int wkLFO=0;
        const int cLFOFaderGroupOffset=4;
        
        // loop through faders on both LFO boards
        for(int y=0;y<LFO_COUNT;y++) {
          // LFOFaderBoard0
          {
              wkInt=LFOFaderBoard0[y];   // one of two  0/1/2/3 
              wkInt=wkInt>>2;            // 0-255 to 0-63
              /*
              Serial.print("LFO_Rate [");
              Serial.print(wkLFO);
              Serial.print("]: ");
              Serial.println(wkInt);
              */
              // manage takeover mode
              if((true==LFOsTakeoverFlag0[y]) || 
                 ( (wkInt>=mvcLFOs[wkLFO].LFO_Rate-cSoftTakeoverRangeLFORates) &&
                   (wkInt<=mvcLFOs[wkLFO].LFO_Rate+cSoftTakeoverRangeLFORates) )
                 )
              {
                LFOsTakeoverFlag0[y]=true; // set the takeover flag as true
                mvcLFOs[wkLFO].LFO_Rate=wkInt;            // assign to the model
                bChangedFlag=true;
              }
              
              //y++; // next fader 
                   
              wkInt=LFOFaderBoard0[y+cLFOFaderGroupOffset];   // two of two 4/5/6/7
              // manage takeover mode
              if((true==LFOsTakeoverFlag0[y+cLFOFaderGroupOffset]) || 
                 ( (wkInt>=mvcLFOs[wkLFO].LFO_Destination-cSoftTakeoverRangeLFOs) &&
                   (wkInt<=mvcLFOs[wkLFO].LFO_Destination+cSoftTakeoverRangeLFOs) )
                 )
              {
                LFOsTakeoverFlag0[y+cLFOFaderGroupOffset]=true; // set the takeover flag as true
                mvcLFOs[wkLFO].LFO_Destination=wkInt;            // assign to the model
    
                // don't need to update the view, it happens with the LFO code, but set flag anyway
                bChangedFlag=true;
              }
          } // end LFOFaderBoard0

          //y--; // backup to start at 0/2/4/6 on next board 
          // LFOFaderBoard1
          {
              wkInt=LFOFaderBoard1[y];   // one of two
              // manage takeover mode
              if((true==LFOsTakeoverFlag1[y]) || 
                 ( (wkInt>=mvcLFOs[wkLFO].LFO_Level-cSoftTakeoverRangeLFOs) &&
                   (wkInt<=mvcLFOs[wkLFO].LFO_Level+cSoftTakeoverRangeLFOs) )
                 )
              {
                LFOsTakeoverFlag1[y]=true; // set the takeover flag as true
                mvcLFOs[wkLFO].LFO_Level=wkInt;            // assign to the model
                bChangedFlag=true;
              }
              
              //y++; // next fader   
                  
              wkInt=LFOFaderBoard1[y+cLFOFaderGroupOffset];   // two of two
              wkInt=wkInt>>5;            // 0-255 to 0-7
              // manage takeover mode
              if((true==LFOsTakeoverFlag1[y+cLFOFaderGroupOffset]) || 
                 ( (wkInt>=mvcLFOs[wkLFO].LFO_Wave-cSoftTakeoverRangeLFOWaves) &&
                   (wkInt<=mvcLFOs[wkLFO].LFO_Wave+cSoftTakeoverRangeLFOWaves) )
                 )
              {
                LFOsTakeoverFlag1[y+cLFOFaderGroupOffset]=true; // set the takeover flag as true
                mvcLFOs[wkLFO].LFO_Wave=wkInt;            // assign to the model
    
                // don't need to update the view (except for the display), it happens with the LFO code, but set the flag anyway
                if(prevWave[wkLFO]!=wkInt){
                    bChangedWaveFlag=true;
                }

                prevWave[wkLFO]=wkInt;
              }
          } // end LFOFaderBoard 1
          wkLFO++;  // each lfo takes 2 faders (per board)
        }  // end "for y MAX_MIDFADERS" 
        if(true==bChangedWaveFlag) { displayLFOWaveforms();  }
    }
  }
  
  if(true==bFaderChangedFlag){  // Route/Trim faders
    digitalWrite(LED_PIN, HIGH);  // Pin 13 = HIGH, LED emits light 
    bFaderChangedFlag=false;

    if((cMode1to8==iFaderMode)||(cMode9to16==iFaderMode)){
      // update routes
      y=0;  // default - cMode1to8
      if(cMode9to16==iFaderMode)  y=8;
      maxy=(y+(cFaderCount/2)); 
      //Serial.print("UpdateRoutes.  maxy=");
      //Serial.println(maxy);

      for(;y<maxy;y++) {
        wkInt=255-FaderBoard0[y];    // invert the value (high fader = lower input channel # (i.e. on top) )
        wkInt=wkInt>>4;              // 0-15
        #ifdef DEF_PRINT_FADERBOARD_TRACE 
          Serial.print("Read fader: ");
          Serial.print(y);
          Serial.print(" wkInt=");
          Serial.println(wkInt);
        #endif
        // manage out-of-range error
        if(mvcChannels[y].scInChannel>cMaxInChannelValue) RoutesTakeoverFlag[y]=true;  // out-of-range - take over
        
        // manage takeover mode
        if((true==RoutesTakeoverFlag[y]) || 
           ( (wkInt>=mvcChannels[y].scInChannel-cSoftTakeoverRangeRoutes) && 
             (wkInt<=mvcChannels[y].scInChannel+cSoftTakeoverRangeRoutes) )
           )
        {
          RoutesTakeoverFlag[y]=true; // set the takeover flag as true
          mvcChannels[y].scInChannel=wkInt;  // save the value 0-15 
          bChangedFlag=true;
          /*
          Serial.print("Updt Routes.  y=");
          Serial.print(y);
          Serial.print("   maxy=");
          Serial.print(maxy);
          Serial.print(" wkInt=");
          Serial.println(wkInt);
          */
        }
      }
    }
    if (true==bChangedFlag) updateLEDfromModel(iFaderMode,false);
   
    // update trims
    bChangedFlag=false;

    if((cMode1to8==iFaderMode)||(cMode9to16==iFaderMode)){
        y=0; // default - cMode1to8
        if(cMode9to16==iFaderMode)   y=8; 
        maxy=(y+(cFaderCount/2));   
        for(;y<maxy;y++) {
          wkInt=255-FaderBoard1[y];    // invert the value (high fader = lower input channel # (i.e. on top) )
          // manage takeover mode
          if((true==TrimsTakeoverFlag[y]) || 
             ( (wkInt>=mvcChannels[y].scTrim-cSoftTakeoverRangeTrims) &&
               (wkInt<=mvcChannels[y].scTrim+cSoftTakeoverRangeTrims) )
             )
          {
            TrimsTakeoverFlag[y]=true; // set the takeover flag as true

            // enforce a "minimum change" rule on the trims, which are 0-255
            const int cMinChangeRangeTrims=2;
            if ( (wkInt>=mvcChannels[y].scTrim+cMinChangeRangeTrims) ||
                 (wkInt<=mvcChannels[y].scTrim-cMinChangeRangeTrims) ) {
                     mvcChannels[y].scTrim=wkInt;            // assign to the model
                     bChangedFlag=true;
            }
          }
        }
    }
    if (true==bChangedFlag)  UpdateTrimsView();
  }
}  // end loop
void readFaders(uint8_t in_I2C_address,uint8_t in_Fader_Model[],uint8_t in_count){
     // uint8_t writeBuffer[3];
    
      // load the buffer 
      //writeBuffer[0]=cPointerByteReadFaders;

      //write
      Wire.beginTransmission(in_I2C_address);
      Wire.send(cPointerByteReadFaders);
      //Wire.write(writeBuffer,1);
      Wire.endTransmission();
      
      // now read the cFaderCount (8) data bytes
      Wire.requestFrom(in_I2C_address, in_count);    // request 8 bytes from slave device and stop (true) after that

      int x=0;
      int maxx;
      if(cModeMid==iFaderMode)   x=0;
      if(cMode1to8==iFaderMode)  x=0; 
      if(cMode9to16==iFaderMode) x=8;
      maxx=(x+in_count);
      while(Wire.available())    // slave may send less than requested
      { 
          uint8_t faderValue = Wire.read();    // receive a byte as uint8_t
          if(x<maxx){ // be sure we're in range before we update
              in_Fader_Model[x++]=faderValue;
              #ifdef DEF_SERIAL_OUT
              #ifdef DEF_PRINT_FADERBOARD_TRACE
              Serial.println(faderValue);
              #endif
              #endif
          }else{
            #ifdef DEF_SERIAL_OUT
            #ifdef DEF_PRINT_FADERBOARD_TRACE
              Serial.println(" read overrun!");  
            #endif
            #endif         
          }
      }
      #ifdef DEF_SERIAL_OUT 
      #ifdef DEF_PRINT_FADERBOARD_TRACE
      Serial.println(" !!! ");  
      #endif
      #endif
}

// this version write to Wire1 (TODO: if we can get it to work)
// the original idea was to use Wire1 (vs Wire) for the Electric Druid I2C bus
// ozh - 2018May31 - attempt to just change the pins for Wire to alternate pins (22 & 23)
void readFadersW1(uint8_t in_I2C_address,uint8_t in_Fader_Model[],uint8_t in_count,bool in_b_use_22_flag){
     // uint8_t writeBuffer[3];
      #ifdef DEF_SERIAL_OUT
      #ifdef DEF_PRINT_FADERBOARD_TRACE_ED
      Serial.println("Enter readFadersW1");
      #endif
      #endif
      // this does not work yet - main pins for Wire are 19/18  the alt pins are 17/16, NOT 22/23.  those are default pins for Wire1
      if(true==in_b_use_22_flag){ // use the Electric Druid I2C network on pins 22 (SCL1) and 23 (SDA1)
        #ifdef DEF_SERIAL_OUT
        #ifdef DEF_PRINT_FADERBOARD_TRACE_ED
        Serial.println("  Use pins 22 (SCL1) & 23 (SDA1) ");
        #endif
        #endif
        Wire.setSCL(22);
        Wire.setSDA(23);
        Wire.begin(in_I2C_address);
      }
      // load the buffer 
      //writeBuffer[0]=cPointerByteReadFaders;

      //write
      Wire.beginTransmission(in_I2C_address);
      Wire.send(cPointerByteReadFaders);
      //Wire.write(writeBuffer,1);
      Wire.endTransmission();
  
      // now read the cFaderCount (8) data bytes
      Wire.requestFrom(in_I2C_address, in_count);    // request 8 bytes from slave device and stop (true) after that

      int x=0;
      int maxx;

      maxx=(x+in_count);
      #ifdef DEF_SERIAL_OUT
      #ifdef DEF_PRINT_FADERBOARD_TRACE_ED
      Serial.println("  Check Wire.available()");
      #endif
      #endif
      while(Wire.available())    // slave may send less than requested
      { 
          #ifdef DEF_SERIAL_OUT
          #ifdef DEF_PRINT_FADERBOARD_TRACE_ED
          Serial.println("  Wire.available() is TRUE");
          #endif
          #endif
          uint8_t faderValue = Wire.read();    // receive a byte as uint8_t
          if(x<maxx){ // be sure we're in range before we update
              in_Fader_Model[x++]=faderValue;
              #ifdef DEF_SERIAL_OUT
              #ifdef DEF_PRINT_FADERBOARD_TRACE_ED
              Serial.println(faderValue);
              #endif
              #endif
          }else{
            #ifdef DEF_SERIAL_OUT
            #ifdef DEF_PRINT_FADERBOARD_TRACE_ED
              Serial.println(" read overrun!");  
            #endif
            #endif         
          }
      }
      
    #ifdef DEF_SERIAL_OUT 
    #ifdef DEF_DBG_R_W_ADSR
      Serial.println(" Show values read !!!!! ");  
      showADSRvalues();
    #endif
    #endif
    if(true==in_b_use_22_flag){ // use the Electric Druid I2C network on pins 22 (SCL1) and 23 (SDA1)
      Wire.setSCL(19);  // always reset to the internal I2C network
      Wire.setSDA(18);
      Wire.begin();
    }
}

void writeFadersW1(uint8_t in_I2C_address,uint8_t in_Fader_Model[],uint8_t in_count){
      // send FADER data
      #ifdef DEF_DBG_R_W_ADSR
      #ifdef DEF_SERIAL_OUT
      Serial.println ("Sending DualADSR data ");
      showADSRvalues();
      #endif
      #endif
      //begin

        // load the buffer 
        writeBuffer[0]=cPointerByteWriteFaders;
        for(int x=0;x<in_count;x++) {
          writeBuffer[x+1]=in_Fader_Model[x];
        }

      //write
        Wire.beginTransmission(in_I2C_address);
        Wire.write(writeBuffer,in_count+1);
        Wire.endTransmission();

}
void updateAD75019View( uint16_t routes[] );

void updateLEDfromModel(int FaderMode,bool bReDisplayFlag){
  Adafruit_8x16minimatrix *pMatrix; // pointer to matrix, cuz it could be either one
  int wkX=0;
  int wkY=0;
  int y;
  int maxy;
  int wkLEDchangedFlag=0;
  int wkMiniMatrixI2CAddr;
  // default
  //if(cMode1to8==FaderMode) {    
    y=0;
    maxy=8;
    wkMiniMatrixI2CAddr=MiniMatrixI2CAddr1to8;
    pMatrix=&matrix0; // point to the right matrix 
  //}
  if(cMode9to16==FaderMode) {    
    y=8;
    maxy=16;
    wkMiniMatrixI2CAddr=MiniMatrixI2CAddr9to16; 
    pMatrix=&matrix1; // point to the right matrix 
  }

  // prep for I2C to 16x8 MiniMatrix LED
  pMatrix->begin(wkMiniMatrixI2CAddr);  // pass in the address 
  //pMatrix->clear();                   // don't clear all, do an intelligent on/off below
  pMatrix->setRotation(0); 
  // this assumes only one bit is set in each row of Routes  
  for(;y<maxy;y++) {
      wkX=mvcChannels[y].scInChannel; // should be 0-15
      wkY=y;  
      // out of range with mode 9-16!!!!
      if(cMode9to16==FaderMode) wkY=(y-8);
      // yes, the x & y are backwards.  the x & y are named for the outputs on the AD75019, not x & y coordinates
      if ((prevX[y]!=wkX)||(true==bReDisplayFlag)) // change from last time
      { 
        if(prevX[y]>15)  prevX[y]=15; // be sure this value is not out of range (it is initialized to 16)
        pMatrix->drawPixel(wkY,prevX[y],LED_OFF);  
        prevX[y]=wkX;    // update prev
        // now turn on new LED
        pMatrix->drawPixel(wkY,wkX,LED_ON);
        // only write display at the very end for all changes  
        //pMatrix->writeDisplay();  // write the changes we just made to the display
        //delay(ciMinipMatrixDelay);
        wkLEDchangedFlag=1;       // signal if we got a change to the LED matrx, only then should we update the switch  
      }      
  }

  // signal if we got a change to the LED matrx, only then should we update the switch 
  if ((1==wkLEDchangedFlag)||(true==bReDisplayFlag)) {
    #ifdef DEF_DEBUG_FADERS2 
    Serial.println(wkMiniMatrixI2CAddr);
    for(int z=0;z<16;z++) {
      Serial.print(z);
      Serial.print(" PrevX value: ");
      Serial.println(prevX[z]);
    }
    #endif
    // first update the AD75019 switches
    updateAD75019View(mvcChannels);

    // write the changes we just made to the display
    pMatrix->writeDisplay();  
    //delay(ciMiniMatrixDelay); 
    //delay(ciAD75019UpdateDelay); // delay to avoid back-to-back updates too quickly - this is more for the faders sake
    // todo: test to see if we really need this now!!!
  }
} // end updateLEDfromModel ()

// update the AD75019 View

// these constants are to adjust the settling and clock times
// these times can probably be shorter, since the AD75019 is on the board with the Teensy
// see datasheet for minimum times - probably 100 nS for line and 75 nS for clock
void delayNanoseconds(int delayCycles) {
  for(;delayCycles>0;delayCycles--) { int x; int y; x=y;}
}
// slowing down this update did not really seem to help
// reliability seems good now.  Most delays are taken out. 
const int lineSettleMicroTime = 1; //  4 of these
const int clockMicroTime = 1;      // 32 of these (max 20 - for AD75019
const int dataMicroTime = 1;       // 16 of these
const int nanoDelayCycles=10;
 
void putByte(byte data,int clockPin,int dataPin) {
  byte i = 8;
  byte mask;
  while(i > 0) {
    mask = 0x01 << (i - 1);      // get bitmask
    digitalWrite( clockPin, LOW);   // tick
    //delayMicroseconds(clockMicroTime);           //1
    if (data & mask){            // choose bit
      digitalWrite(dataPin, HIGH);// send 1
    }else{
      digitalWrite(dataPin, LOW); // send 0
    }
    //delayMicroseconds(dataMicroTime);            // data settle time
    delayNanoseconds(nanoDelayCycles);
    digitalWrite(clockPin, HIGH);   // tock
    //delayMicroseconds(clockMicroTime);           //2
    delayNanoseconds(nanoDelayCycles);
    --i;                         // move to lesser bit
  }
  //delayMicroseconds(lineSettleMicroTime);
}

void updateAD75019View( SWITCH_CHANNEL channels[]) {  
    //updateAD75019View is the "easy"  function to use for a     //single AD75019 

    int wkX=0;
    uint16_t iRouteBits; // bits representation
    
    uint8_t iHighByte;
    uint8_t iLowByte;
    
    digitalWrite(AD75019_PCLK_PIN,HIGH);
    delayMicroseconds(lineSettleMicroTime);  
    //for(int y=0;y<=MAX_ROUTES;y++){ 
    for(int y=MAX_ROUTES-1;y>=0;y--){ 
      wkX=mvcChannels[y].scInChannel;;
      // turn index into bit pattern
      iRouteBits=LEDBits[wkX];
      iHighByte=iRouteBits>>8;     // upper byte
      iLowByte=iRouteBits&0xFF;    // mask lower byte
      #ifdef DEF_DEBUG_75019_WRITE
        Serial.print("Route #: ");
        Serial.print(y);
        Serial.print("iHighByte: ");
        Serial.print(iHighByte);
        Serial.print("     iLowByte: ");
        Serial.println(iLowByte);
      #endif
      putByte(iHighByte,AD75019_SCLK_PIN,AD75019_SIN_PIN);
      putByte(iLowByte, AD75019_SCLK_PIN,AD75019_SIN_PIN);
    }
 
    digitalWrite(AD75019_PCLK_PIN, LOW);       // and load da data
    delayMicroseconds(lineSettleMicroTime); 
}

void UpdateTrimsView() {
  byte iByte;
  bool bLFODestinationFlag=false; // indication whether the LFO has this channel as a destination
  
  // note: having the LFO code in this routine violates encapsulation.  Not the best form.
  
  // update the VCA/Mix boards (i.e. max528 on those boards)
  // update all 16 trims
  
  for(int chnl=0;chnl<MAX_ROUTES;chnl++)
  {
    bLFODestinationFlag=false;  // reset for each output channel to be updated
    for(int iLFONum=0;iLFONum<LFO_COUNT;iLFONum++) {
      if(mvcLFOs[iLFONum].LFO_Destination==chnl) { 
        #ifdef OZHMS702_DBG_LFO
             //if(chnl>0) { // ignore chnl 0 for the moment
             Serial.print("LFO #: ");
             Serial.print(iLFONum);
             Serial.print("  level out: ");
             Serial.println(mvcLFOs[iLFONum].LFO_Level);
             //}
        #endif
        if(mvcLFOs[iLFONum].LFO_Level>LFO_LEVEL_THRESHOLD){
          bLFODestinationFlag=true;  // don't update this channel, the LFO will get it
        }
      }
    }
    iByte=mvcChannels[chnl].scTrim; // value is 0-255, already inverted
    if(false==bLFODestinationFlag) { // only update here if LFO will not be updating it
      if(chnl<8){
        #ifdef SM_PROTOTYPE_HARDWARE
          writeDAC528(DAC_1to8,faderChToDacCh[chnl],iByte);
        #else
          writeDAC528(DAC_1to8,chnl,iByte);       
        #endif
      }else{
        #ifdef SM_PROTOTYPE_HARDWARE
          writeDAC528(DAC_9to16,faderChToDacCh[chnl],iByte); // and the second board 
        #else
          writeDAC528(DAC_9to16,chnl-8,iByte); // and the second board        
        #endif
      }
    }
  }
}

/* Begin MAX528 Octal 8 bit SPI DAC Support */   
const int cPowerOfTwo[8] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};
void initDAC528(int dacBoardNumber) {
  byte CSpin;
  // note: data line is shared with LED Display, so CS4 must be on to write to MAX528 also use separate clock
  //       putByte is a bit banging routine from the LED code.  reuse it.
  // sec CSx based on board #
  if(DAC_1to8==dacBoardNumber)  CSpin=CS0;
  if(DAC_9to16==dacBoardNumber) CSpin=CS1;
  
  // set CSx low
  digitalWrite(CSpin,LOW);
  
  // ADDR byte 
  putByte(0x00,DACCLK_PIN,DACDATA_PIN);  // set buffer mode   
  // DATA byte
  putByte(0xFF,DACCLK_PIN,DACDATA_PIN);  // set all to full buffer mode
  
  // set CSx high (disable)
  digitalWrite(CSpin,HIGH);
}

void writeDAC528(int dacBoardNumber, int dacNumber, byte dacData) {
  byte wkDacNumber=0;
  byte CSpin;
  if(DAC_1to8==dacBoardNumber)  CSpin=CS0;
  if(DAC_9to16==dacBoardNumber) CSpin=CS1;
 
  wkDacNumber=cPowerOfTwo[dacNumber];  // look up the power of 2 to set the bits
  #if defined(OZH702_DBG_DAC_WRITES)
    //if(3==dacNumber){ // TODO: remove this after debugging
      Serial.print("DAC data: "); 
      Serial.print(dacData);
      Serial.print("   DAC #: "); 
      Serial.print(dacNumber);
      Serial.print("   wkDacNumber: ");
      Serial.println(wkDacNumber);
    //}
  #endif
  // set CSpin low
  digitalWrite(CSpin,LOW);  
  // ADDR byte 
  putByte(wkDacNumber,DACCLK_PIN,DACDATA_PIN);  // set dacNumber
  // DATA byte
  putByte(dacData,DACCLK_PIN,DACDATA_PIN);  // set all to full buffer mode
  // set CSpin high (disable)
  digitalWrite(CSpin,HIGH); 
}

/* End MAX528 Octal 8 bit SPI DAC Support */
// this does NOT work since the conversion to mvcChannels store the 0-15 representation
void switchTestRoutine1(){
   // test writing to AD75019

  iMainLoopCount++;
  if(cMaxMainLoopCount<iMainLoopCount){

    iMainLoopCount=0;
    // for testing, toggle between inputs 0 and 1
    if(ciLEDBit0==mvcChannels[0].scInChannel){
      iInputBitCntr++; //
      if (iInputBitCntr>=ciMaxInputBit) iInputBitCntr=1;
      mvcChannels[0].scInChannel=ciLEDBit1;     // toggle to 1
      // the following works
      mvcChannels[0].scInChannel=LEDBits[iInputBitCntr]; // toggle to a series of inputs 1-15 (not 0)
      RoutesEditedFlag=1;
      digitalWrite(LED_PIN, HIGH);  // Pin 13 = HIGH, LED emits light  
    }else{
      mvcChannels[0].scInChannel=ciLEDBit0;
      RoutesEditedFlag=1; 
      digitalWrite(LED_PIN, LOW);     
    }
  }
  if(0<RoutesEditedFlag){
    RoutesEditedFlag=0;
    updateLEDfromModel(iFaderMode,false);
  }
}


/* update LED data
      // send LED data
      #ifdef DEF_PRINT_FADERBOARD_TRACE
      #ifdef DEF_SERIAL_OUT
      Serial.print ("Sending LED data: ");
      Serial.println (iLSByte);
      #endif
      #endif
      //begin
        i2c_addr_fader=0x10;// fader submodule A0
        // load the buffer 
        writeBuffer[0]=cPointerByteWriteLED;
        writeBuffer[1]=iMSByte;
        writeBuffer[2]=iLSByte; 

      //write
        Wire.beginTransmission(i2c_addr_fader);
        Wire.write(writeBuffer,3);
        Wire.endTransmission();
 */

// hardware the first trim fader to the first board/first DAC for testing
void runVCAHardwareTest() {
#ifdef DEF_VCA_BOARD_TEST
  for(int data=0;data<255;data++)
  {
    for(int chnl=0;chnl<8;chnl++) {
      // note: VCA response is "backwards" (ground is full volume)
      //       so ... this is not a rise but a fall (AD envelopw) 
      writeDAC528(DAC_1to8,chnl,data);
      writeDAC528(DAC_9to16,chnl,data); // and the second board 
    }
  }
#endif
}   // end hardware test

