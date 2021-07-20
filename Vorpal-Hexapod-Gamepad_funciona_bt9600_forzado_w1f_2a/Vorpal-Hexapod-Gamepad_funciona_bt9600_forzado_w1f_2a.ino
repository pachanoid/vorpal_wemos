 // Copyright (C) 2017 Vorpal Robotics, LLC.

const char *Version = "#GV2r0";



int debugmode = 0;          // Set to 1 to get more debug messages. Warning: this may make Scratch unstable so don't leave it on.

#define DEBUG_SD  0         // Set this to 1 if you want debugging info about the SD card and record/play functions.
                            // This is very verbose and may affect scratch performance so only do it if you have to.
                            // You will likely see things running slower than normal in scratch record/play mode for example.
                            // It also takes up a lot of memory and you might get warnings from Arduino about too little
                            // RAM being left. But it seems to work for me even with that warning.

#define DEBUG_BUTTONS 0     // There is some manufacturer who is selling defective DPAD modules that look just like the
                            // ones sold by Vorpal Robotics, and they have a different set of output values for each
                            // button (and only use a small percentage of the range of values, which is why I consider
                            // them to be defective). If you get one of those, set this define to 1 and you'll get debug info sent
                            // to the serial monitor to help you figure out what values will work. After that you can
                            // modify the values in the decode_button() function below to suite your DPAD button module.

#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <SoftwareSerial.h>

//////////////////////////////////////////
// Gamepad layout, using variable names used in this code:
//
//                       W
// M0  M1  M2  M3
// M4  M5  M6  M7        F
// M8  M9  M10 M11     L   R
// M12 M13 M14 M15       B
//



SoftwareSerial BlueTooth(A5,A4);  // connect bluetooth module Tx=A5=Yellow wire Rx=A4=Green Wire
                                  // although not intuitive, the analog ports function just fine as digital ports for this purpose
                                  // and in order to keep the wiring simple for the 4x4 button matrix as well as the SPI lines needed
                                  // for the SD card, it was necessary to use analog ports for the bluetooth module.



// Matrix button definitions
// Top row
#define WALK_1 0
#define WALK_2 1
#define WALK_3 2
#define WALK_4 3
// Second row
#define DANCE_1 4
#define DANCE_2 5
#define DANCE_3 6
#define DANCE_4 7
// Third Row
#define FIGHT_1 8
#define FIGHT_2 9
#define FIGHT_3 10
#define FIGHT_4 11
// Bottom Row
#define REC_RECORD 12
#define REC_PLAY 13
#define REC_REWIND 14
#define REC_ERASE 15

// Pin definitions

#define DpadPin A1
#define VCCA1 A2    // we play a trick to power the dpad buttons, use adjacent unusued analog ports for power
#define GNDA1 A3    // Yes, you can make an analog port into a digital output!
#define SDCHIPSELECT 10   // chip select pin for the SD card reader

// definitions to decode the 4x4 button matrix

#define MATRIX_ROW_START 6
#define MATRIX_COL_START 2
#define MATRIX_NROW 4
#define MATRIX_NCOL 4

unsigned long suppressButtonsUntil = 0;      // default is not to suppress until we see serial data
File SDGamepadRecordFile;           // REC, holds the gamepad record/play file
const char SDGamepadRecordFileName[] = "REC";   // never changes
char SDScratchRecordFileName[4];    // three letters like W1f for walk mode 1 forward dpad, plus one more for end of string '\0'
File SDScratchRecordFile;

byte TrimMode = 0;
byte verbose = 0;
char ModeChars[] = {'W', 'D', 'F', 'R'};
char SubmodeChars[] = {'1', '2', '3', '4'};

char CurCmd = ModeChars[0];  // default is Walk mode
char CurSubCmd = SubmodeChars[0]; // default is primary submode
char CurDpad = 's'; // default is stop
unsigned int BeepFreq = 0;   // frequency of next beep command, 0 means no beep, should be range 50 to 2000 otherwise
unsigned int BeepDur = 0;    // duration of next beep command, 0 means no beep, should be in range 1 to 30000 otherwise
                             // although if you go too short, like less than 30, you'll hardly hear anything

void println() {
  Serial.println("");
}

int priorMatrix = -1;
long curMatrixStartTime = 0;
int longClick = 0;  // this will be set to 1 if the last matrix button pressed was held a long time
int priorLongClick = 0; // used to track whether we should beep to indicate new longclick detected.

#define LONGCLICKMILLIS 500
#define VERYLONGCLICKMILLIS 1000

// find out which button is pressed, the first one found is returned
int scanmatrix() {
  // we will energize row lines then read column lines

  // first set all rows to high impedance mode
  for (int row = 0; row < MATRIX_NROW; row++) {
    pinMode(MATRIX_ROW_START+row, INPUT);
  }
  // set all columns to pullup inputs
  for (int col = 0; col < MATRIX_NCOL; col++) {
    pinMode(MATRIX_COL_START+col, INPUT_PULLUP);
  } 

  // read each row/column combo until we find one that is active
  for (int row = 0; row < MATRIX_NROW; row++) {
    // set only the row we're looking at output low
    pinMode(MATRIX_ROW_START+row, OUTPUT);
    digitalWrite(MATRIX_ROW_START+row, LOW);
    
    for (int col = 0; col < MATRIX_NCOL; col++) {
      delayMicroseconds(100);
      if (digitalRead(MATRIX_COL_START+col) == LOW) {
        // we found the first pushed button
        if (row < 3) {
          CurCmd = ModeChars[row];
          CurSubCmd = SubmodeChars[col];
        }
        int curmatrix = row*MATRIX_NROW+col;
        int clicktime = millis() - curMatrixStartTime;
        if (curmatrix != priorMatrix) {
          curMatrixStartTime = millis();
          priorMatrix = curmatrix;
          longClick = priorLongClick = 0;
        } else if (clicktime > LONGCLICKMILLIS) {
          // User has been holding down the same button continuously for a long time
          if (clicktime > VERYLONGCLICKMILLIS) {
            longClick = 2;
          } else {
            longClick = 1;
          }
        }
        return curmatrix;
      }
    }
    pinMode(MATRIX_ROW_START+row, INPUT);  // set back to high impedance
    //delay(1);
  }

  // if we get here no buttons were pushed, return -1 and
  // clear out the timers used for long click detection.
  priorMatrix = -1;
  curMatrixStartTime = 0;
  return -1;
}

// This decodes which button is pressed on the DPAD

char decode_button(int b) {

#if DEPADDEBUG
  Serial.print("DPAD: "); Serial.println(b);
#endif

// If your DPAD is doing the wrong things for each button, and you didn't

  if (b < 100) {
     return 'b';  // backward (bottom button)
  } else if (b < 200) {
     return 'l';  // left 
  } else if (b < 400) {
    return 'r';   // right
  } else if (b < 600) {
    return 'f';  // forward (top of diamond)
  } else if (b < 850) {
    return 'w';  // weapon (very top button) In the documentation this button is called "Special"
                 // but a long time ago we called it "weapon" because it was used in some other
                 // robot projects that were fighting robots. The code still uses "w" since "s" means stop.
  } else {
    return 's';  // stop (no button is pressed)
  }
}

//////////////////////////////////////////////////////////
// BEEP FREQUENCIES
//////////////////////////////////////////////////////////
#define BF_ERROR 100
#define BF_RECORD_CHIRP 1500
#define BF_PLAY_CHIRP 1000
#define BF_PAUSE_CHIRP 2000
#define BF_NOTIFY  500
#define BF_ERASE   2000
#define BF_REWIND  700

//////////////////////////////////////////////////////////
// BEEP DURATIONS
//////////////////////////////////////////////////////////
#define BD_CHIRP 10
#define BD_SHORT 100
#define BD_MED   200
#define BD_LONG  500
#define BD_VERYLONG 2000

//////////////////////////////////////////////////////////
//   RECORD/PLAY FEATURES
/////////////////////////////////////////////////////////

// States for the Scratch Record/play function
// These can't be the same state variable as used for the gamepad REC/PLAY states because you can have
// a scratch recording playing from a long tap button at the same time you've got
// the gamepad REC button working.
#define SREC_STOPPED    0
#define SREC_RECORDING  1
#define SREC_PLAYING    2

// States for the Gamepad record/play function
#define GREC_STOPPED     0
#define GREC_RECORDING   1
#define GREC_PLAYING     2
#define GREC_PAUSED      3
#define GREC_REWINDING   4
#define GREC_ERASING     5

#define REC_MAXLEN (40000)      // we will arbitrarily limit recording time to about 1 hour (forty thousand deci-seconds)
#define REC_FRAMEMILLIS 100     // time between data frames when recording/playing

int GRecState = GREC_STOPPED;  // gamepad recording state
int SRecState = SREC_STOPPED; // scratch recording state
unsigned long GRecNextEventTime = 0; // next time to record or play a gamepad record/play event 
unsigned long SRecNextEventTime = 0; // next time to play a scratch recording event

unsigned long NextTransmitTime = 0;  // next time to send a command to the robot
char PlayLoopMode = 0;

void setBeep(int f, int d) {
  // schedule a beep to go out with the next transmission
  // this is not quite right as there can only be one beep per transmission
  // right now so if two different subsystems wanted to beep at the same time
  // whichever one is scheduled last would win. 
  // But because 10 transmits go out per second this seems sufficient, and it's simple
  BeepFreq = f;
  BeepDur = d;
}

// Open a scratch recording file.
// Closes any prior recording file first. This may be the same file, if so then this
// effectively just rewinds it.
// Returns 1 if the open worked, otherwise 0
//
int openScratchRecordFile(char cmd, char subcmd, char dpad) {
  if (SDScratchRecordFile) {  // if one is already open, close it
    SDScratchRecordFile.close();
  }

  SDScratchRecordFileName[0] = cmd;
  SDScratchRecordFileName[1] = subcmd;
  SDScratchRecordFileName[2] = dpad;
  SDScratchRecordFileName[3] = '\0';
 
  SDScratchRecordFile = SD.open(SDScratchRecordFileName, FILE_WRITE);
  if (SDScratchRecordFile) {
    SDScratchRecordFile.seek(0);
    return 1;
  } else {
    setBeep(100, 100); // error tone
    Serial.print("#SDOF:"); Serial.println(SDScratchRecordFileName); // SDOF means "SD Open Failed"
    return 0;
  }
}


long LastRecChirp;  // keeps track of the last time we sent a "recording is happening" reminder chirp to the user

void removeScratchRecordFile(char cmd, char subcmd, char dpad) {
  SDScratchRecordFile.close();  // it's safe to do this even if the file is not open

  SDScratchRecordFileName[0] = cmd;
  SDScratchRecordFileName[1] = subcmd;
  SDScratchRecordFileName[2] = dpad;
  SDScratchRecordFileName[3] = '\0';
  SD.remove(SDScratchRecordFileName);
}



int sendbeep(int noheader) {

#if DEBUG_SD
      if (BeepFreq != 0) {
        Serial.print("#BTBEEP="); Serial.print("B+"); Serial.print(BeepFreq); Serial.print("+"); Serial.println(BeepDur);
      }
#endif

    unsigned int beepfreqhigh = highByte(BeepFreq);
    unsigned int beepfreqlow = lowByte(BeepFreq);
    if (!noheader) {
      BlueTooth.print("B");
    }
    BlueTooth.write(beepfreqhigh);
    BlueTooth.write(beepfreqlow);

    unsigned int beepdurhigh = highByte(BeepDur);
    unsigned int beepdurlow = lowByte(BeepDur);
    BlueTooth.write(beepdurhigh);
    BlueTooth.write(beepdurlow);

    // return checksum info
    if (noheader) {
      return beepfreqhigh+beepfreqlow+beepdurhigh+beepdurlow;
    } else {
      return 'B'+beepfreqhigh+beepfreqlow+beepdurhigh+beepdurlow;
    }

}

void SendNextRecordedFrame(File file, char *filename, int loop) {
        int length = file.read(); // first byte is the number of bytes in the frame to send
        
#if DEBUG_SD
        Serial.print("#L"); Serial.println(length);
#endif
        if (length <= 0 || file.available() < length+1) { // we are at the end of the file
          if (loop) {
            // rewind to start of file and just keep going in this state to loop
            file.seek(0);
            length = file.read();
            if (file.available() < length+1) { // something's wrong, the file is damaged, return
              return;
            }
            Serial.println("#LOOP");
          } else { // not looping, stop playing
            if (GRecState == GREC_PLAYING) {
              GRecState = GREC_STOPPED;
              SDGamepadRecordFile.close();
            }
            if (SRecState == SREC_PLAYING) {
              SRecState = SREC_STOPPED;
              SDScratchRecordFile.close();
            }
            file.close(); // it's ok to close the file twice
            return;
          }
        }
        // if we get here, there is a full frame to send to the robot

        // one complication: if we're playing a scratch recording off a long click button, and also
        // we're in gamepad record mode, then we need to save the current scratch playing records to the gamepad record file
        if (SRecState == SREC_PLAYING && GRecState == GREC_RECORDING) {
          // in this case we know the file we're reading from must be a scratch recording that's now playing
          // so write its length into the gamepad recording file.
          //SDGamepadRecordFile.write(length);
#if DEBUG_SD
          Serial.print("#SC>GR L="); Serial.println(length);
#endif
        }
        BlueTooth.print("V1");
        BlueTooth.write(length+5);  // include 5 more bytes for beep
#if DEBUG_SD
        Serial.print("#V1 L="); Serial.println(length+5);
#endif
        {
            int checksum = length+5;  // the length byte is included in the checksum
            for (int i = 0; i < length && file.available()>0; i++) {
              int c = file.read();
              checksum += c;
              BlueTooth.write(c);
              if (SRecState == SREC_PLAYING && GRecState == GREC_RECORDING) {
                // in this case we know the file we're reading from must be a scratch recording that's now playing
                // and we're also recording using the gamepad record feature, so we need to write this byte into the gamepad
                // recording file
                //SDGamepadRecordFile.write(c);
#if DEBUG_SD
                Serial.print("#SC>GR@");  Serial.print(SDScratchRecordFile.position()); Serial.print(":"); Serial.println(SDGamepadRecordFile.position()); // scratch data written to gamepad recording file
#endif
              }
#if DEBUG_SD
              Serial.print("#"); Serial.write(c); Serial.print("("); Serial.print(c); Serial.println(")");
#endif
            }
            checksum += sendbeep(0);
            setBeep(0,0); // clear the beep since we've already sent it out
            checksum = (checksum % 256);
            BlueTooth.write(checksum);
#if DEBUG_SD
            Serial.print("#CS"); Serial.println(checksum);
#endif
        }

#if DEBUG_SD
        Serial.print("#P:"); Serial.print(filename); Serial.println(file.position());
#endif
}

void RecordPlayHandler() {
  // We save records consisting of a length byte followed by the data bytes.
  // This allows easy transmission by just putting the "V1" header out followed by the
  // length and data, followed by the checksum to bluetooth.

  // first let's see if scratch is playing a command right now from a SD card button file
  if (SRecState == SREC_PLAYING) {
     if (!SDScratchRecordFile) {
      // something is wrong, the file is not open
      Serial.print("#E:"); Serial.println(SDScratchRecordFileName); // "Scratch Play Error"
      SRecState = SREC_STOPPED;
      return;
     }

     if (millis() > NextTransmitTime) {
          NextTransmitTime = millis() + REC_FRAMEMILLIS;
          SendNextRecordedFrame(SDScratchRecordFile, SDScratchRecordFileName, 1); // always loop scratch recordings
     }

     return;  // in this case we don't need to continue with the rest of the code because we've taken care of both
              // scratch playing and gamepad recording from a scratch playback
  }

  // If we get here, then Scratch was not recording or playing anything
  // The code below handles the gamepad manual recording functions triggered from the "R" line of buttons
  
  switch (GRecState) {
    case GREC_STOPPED:
      // make sure file is closed and flushed
      SDGamepadRecordFile.close();  // it's ok to call this even if its already closed, I checked the SD card code
      break;
      
    /////////////////////////////////////////////////////////////  
    case GREC_PAUSED:
      // nothing to do!
#if DEBUG_SD
      Serial.println("#PS");
#endif
      break;

    ////////////////////////////////////////////////////////////
    case GREC_PLAYING: {
        if (!SDGamepadRecordFile) {
          // We don't seem to have an opened file, something is wrong
          Serial.println("#SDNOP"); // "SD Not Open"
          GRecState = GREC_STOPPED;
          return;
        }
        // chirp once every second to remind user they are playing a recording
        long t = millis();
        if ((t-LastRecChirp) >= 1000) {
           setBeep(BF_RECORD_CHIRP, BD_CHIRP);
           LastRecChirp = t;
        }
        
        if (millis() < GRecNextEventTime) {
          // it's not time to send the next record yet
          return;
        }

        // If we get here, it's time to transmit the next record over bluetooth
        GRecNextEventTime = millis() + REC_FRAMEMILLIS;
#if DEBUG_SD
        Serial.print("#P:"); Serial.print(SDGamepadRecordFileName);Serial.print("@");Serial.print((long)SDGamepadRecordFile.position()); println();
#endif
        SendNextRecordedFrame(SDGamepadRecordFile, (char *)SDGamepadRecordFileName, PlayLoopMode);
    } 
      break;

    /////////////////////////////////////////////////////////
    case GREC_RECORDING:
      if (!SDGamepadRecordFile) {
          // something is hideously wrong
          GRecState = GREC_STOPPED;
          return;
      }
      // chirp once every second to remind user they are recording
      if ((millis()-LastRecChirp) >= 1000) {
         setBeep(BF_RECORD_CHIRP, BD_CHIRP);
         LastRecChirp = millis();
      }

      if (millis() < GRecNextEventTime) {
        // it's not time to record a frame yet
        return;
      }
#if DEBUG_SD
      Serial.print("REC@"); Serial.println(SDGamepadRecordFile.position());
#endif
      GRecNextEventTime = millis() + REC_FRAMEMILLIS;
      { // local variables require a scope
        int three = 3;  // yeah this is weird but trust me
        SDGamepadRecordFile.write(three);
        SDGamepadRecordFile.write(CurCmd);
        SDGamepadRecordFile.write(CurSubCmd);
        SDGamepadRecordFile.write(CurDpad);
      }
      // we don't record headers or checksums. The SD card is considered a reliable device.
#if  DEBUG_SD
      Serial.print("#L="); Serial.println(SDGamepadRecordFile.size());
#endif
      
      break;

    ////////////////////////////////////////////////////////////
    case GREC_REWINDING:
      GRecNextEventTime = 0;
      GRecState = GREC_STOPPED;
      SDGamepadRecordFile.flush();  // it's safe to do all these operations without checking if the file is open
      SDGamepadRecordFile.seek(0);       

#if DEBUG_SD
      Serial.println("#RW");
#endif
      break;

    //////////////////////////////////////////////////////////
    default:
      Serial.print("#E2:");      // "Record State Error"
      Serial.print(GRecState); println();
      break;
  }
}

void setup() {
  // right away, see if we're supposed to be in trim mode
  if (scanmatrix() == WALK_1) {
    Serial.println("#trim");
    TrimMode = 1;
  }
  // make a characteristic flashing pattern to indicate the gamepad code is loaded.
  pinMode(13, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(13, !digitalRead(13));
    delay(400);
  }
  // after this point you can't flash the led on pin 13 because we're using it for SD card

  BlueTooth.begin(9600);
  Serial.begin(9600);
  Serial.println(Version);

  pinMode(A0, OUTPUT);  // extra ground for additional FTDI port if needed
  digitalWrite(A0, LOW);
  pinMode(VCCA1, OUTPUT);
  pinMode(GNDA1, OUTPUT);
  digitalWrite(GNDA1, LOW);
  digitalWrite(VCCA1, HIGH);
  pinMode(SDCHIPSELECT, OUTPUT);
  digitalWrite(SDCHIPSELECT, HIGH); // chip select for SD card
  
  if (!SD.begin(SDCHIPSELECT)) {
    Serial.println("#SDBF");    // SD Begin Failed
    return;
  }
}

int priormatrix = -1;
long curmatrixstarttime = 0;  // used to detect long tap for play button and erase button

  // Scratch integration: If anything appears on the serial input,
  // it's probably coming from a scratch program, so simply send it
  // out to the robot, unless it appears to be a command to start
  // recording onto a gamepad button

// The following are states for the scratch state machine

#define SCR_WAITING_FOR_HEADER  0
#define SCR_WAITING_FOR_HEX_1   1
#define SCR_WAITING_FOR_REC_1   2
#define SCR_WAITING_FOR_LENGTH  3
#define SCR_HEX_TRANSFER        4
#define SCR_REC_COMMAND         5

#define SCR_MAX_COMMAND_LENGTH  80    // we never expect scratch to send more than 16 commands at a time and max command len is 5 bytes.
                                      // 16 commands is enough to send a move individually to every servo, plus a beep, plus a sensor
                                      // request, plus a couple more besides that. At 80 bytes the packet would require about 20 millisec
                                      // to send over bluetooth, which allows plenty of time for the hexapod to send back the sensor
                                      // data before the next transmission. Although the bluetooth hardware is full duplex, the
                                      // softwareserial library currently is not, so we can't be both reading and writing the bluetooth
                                      // module at the same time.



int ScratchState = SCR_WAITING_FOR_HEADER;
int ScratchXmitBytes = 0; // how many packet bytes did we get so far
int ScratchLength = 0;    // length received in packet
char ScratchSDFileName[3]; // the name of a scratch recording file which is a matrix/dpad combo like: W1b for "walk mode 1 backward dpad"

int handleSerialInput() {

  int dataread = 0;
  
 
  return dataread;
}


void loop() {
  int matrix = scanmatrix();
  CurDpad = decode_button(analogRead(DpadPin));
  
  if (debugmode && matrix != -1) {
    Serial.print("#MA:LC=");Serial.print(longClick); Serial.print("m:"); Serial.println(matrix);
  }
  
  if (debugmode && CurDpad != 's') {
    Serial.print("#DP:"); Serial.println(CurDpad);
  }

  if (TrimMode) {
    // special mode where we just transmit the DPAD buttons or the buttons on the record line in a special way.
    //if (CurDpad != 's') { Serial.print("#TRIM:"); Serial.println(CurDpad); }
    
    delay(200);
    return;
  }

  if (priormatrix != matrix) {  // the matrix button pressed has changed from the prior loop iteration
    curmatrixstarttime = millis();  // used to detect long tap
    if (matrix != -1) {  // -1 means nothing was pressed
      // short beep for button press feedback
      setBeep(200,50);
    }
  }

  if (longClick && !priorLongClick) {  // we have detected a long click with no prior long click
    if (matrix < REC_RECORD) {  // don't beep if it's a record button
      setBeep(2000,100); // high pitch beep tells user they are in long click mode
#if DEBUG_BUTTONS
      Serial.println("#LCL"); // long click
#endif
    }
    priorLongClick = longClick;  // keep track of whether we are already long clicking
  }

  int serialinput = handleSerialInput();  // this would be commands coming from scratch over the hardware serial port
                                          // the return code is how many bytes of serial input were handled
#if DEBUG_SD
  if (serialinput && debugmode) {
    Serial.print("#SINP="); Serial.println(serialinput);
  }
#endif

  

  RecordPlayHandler();  // handle the record/play mode

  if (millis() > NextTransmitTime  && GRecState != GREC_PLAYING && SRecState != SREC_PLAYING ) { // don't transmit joystick controls during replay mode!

    // Packet consists of:
    // Byte 0: The letter "V" is used as a header. (Vorpal)
    // Byte 1: The letter "1" which is the version number of the protocol. In the future there may be different gamepad and
    //         robot protocols and this would allow some interoperability between robots and gamepads of different version.
    // Byte 2: L, the length of the data payload in the packet. Right now it is always 8. This number is
    //          only the payload bytes, it does not include the "V", the length, or the checksum
    // Bytes 3 through 3+L-1  The actual data.
    // Byte 3+L The base 256 checksum. The sum of all the payload bytes plus the L byte modulo 256.
    //
    // Right now the 
    //

    
    BlueTooth.print("V1"); // Vorpal hexapod radio protocol header version 1
    int eight=8;
    //forzando para que avanze w 1 f
    CurCmd =87;
    CurSubCmd=1;
    CurDpad=102;
    
    BlueTooth.write(eight);
    BlueTooth.write(CurCmd);
    BlueTooth.write(CurSubCmd);
    BlueTooth.write(CurDpad);
 //BlueTooth.write(850);

    unsigned int checksum = sendbeep(0);

    checksum += eight+CurCmd+CurSubCmd+CurDpad;
    checksum = (checksum % 256);
    BlueTooth.write(checksum);
    //BlueTooth.flush();
    
    setBeep(0,0); // clear the current beep because it's been sent now
    
    NextTransmitTime = millis() + REC_FRAMEMILLIS;
  }
}
