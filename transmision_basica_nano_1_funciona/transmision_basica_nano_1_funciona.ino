 #include <SoftwareSerial.h>

SoftwareSerial BlueTooth(A5,A4);  // connect bluetooth module Tx=A5=Yellow wire Rx=A4=Green Wire A5 naranjo
 
#define REC_FRAMEMILLIS 100     // time between data frames when recording/playing 
 
 char CurCmd;
 char CurSubCmd;
 char CurDpad;
unsigned int BeepFreq = 0;   // frequency of next beep command, 0 means no beep, should be range 50 to 2000 otherwise
unsigned int BeepDur = 0;    // duration of next beep command, 0 means no beep, should be in range 1 to 30000 otherwise
                             // although if you go too short, like less than 30, you'll hardly hear anything

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



int sendbeep(int noheader) {

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





void setup() {
  Serial.begin(9600);
   BlueTooth.begin(9600);

}

void loop() {
   
    BlueTooth.print("V1"); // Vorpal hexapod radio protocol header version 1 (Byte 0 and 1)
    //forzando para que avanze w 1 f
    int eight =8;
    CurCmd =87;
    CurSubCmd=1;
    CurDpad=102;
    
    BlueTooth.write(eight);             //Byte 2
    BlueTooth.write(CurCmd);            //Byte 3
    BlueTooth.write(CurSubCmd);         //
    BlueTooth.write(CurDpad);
 //BlueTooth.write(850);

    unsigned int checksum = sendbeep(0);

    checksum += eight+CurCmd+CurSubCmd+CurDpad;
    checksum = (checksum % 256);
    BlueTooth.write(checksum);
    //BlueTooth.flush();
    
    setBeep(0,0); // clear the current beep because it's been sent now
    
      NextTransmitTime = millis() + REC_FRAMEMILLIS;
      delay(10);  //menos milisegundos taldea
  
}
