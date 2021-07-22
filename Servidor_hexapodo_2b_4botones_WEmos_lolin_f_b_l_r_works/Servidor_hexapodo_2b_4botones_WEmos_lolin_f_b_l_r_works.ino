/*********
  Rui Santos
  Complete project details at http://randomnerdtutorials.com  
*********/
/************************************
condicionar solo 1 boton a la vez en el servidor para que hexapodo vorpal avanze, retroceda, izquierda, derecha
LOLIN WEMOS
*/
#include <Arduino.h>
#include <SPI.h>

#include <Wire.h>  
#include "SSD1306.h"
 #include <SoftwareSerial.h>

SoftwareSerial BlueTooth(D5,D6);  // connect bluetooth module Tx=A5=Yellow wire Rx=A4=Green Wire A5 naranjo
 
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



// Load Wi-Fi library
#include <ESP8266WiFi.h>
//#include <RCSwitch.h>
//#include <IRremoteESP8266.h>
// the IP address for the shield:
IPAddress ip(192, 168, 1, 177); 
  IPAddress gateway(192,168,1,1);   
IPAddress subnet(255,255,255,0);   
 
//salida por softwareserial 
//#include <SoftwareSerial.h>
//SoftwareSerial BTserial(2, 4);
// Replace with your network credentials
const char* ssid     = "matrix666";
const char* password = "artificio639";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output5State = "off";
String output4State = "off";
String output3State = "off";
String output2State = "off";

// Assign output variables to GPIO pins
//const int output5 = 5; //gpio5 =D1
//const int output0 = 0; //gpio0 =D3

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0; 
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;



void setup() {
  
      Serial.begin(9600);
   BlueTooth.begin(9600);

  //BTserial.begin(9600);
  //ç Initialize the output variables as outputs
 // pinMode(output5, OUTPUT);
  //pinMode(output0, OUTPUT);
  // Set outputs to LOW
  //digitalWrite(output5, LOW);
  //digitalWrite(output0, LOW);


/**************+++
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a:\t");
  Serial.println(ssid); 
 */

  

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  WiFi.mode(WIFI_STA);
  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, password);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
   
  Serial.println(WiFi.localIP());
  server.begin();
//display.init();
//display.flipScreenVertically();
 //display.drawString(0, 0, "192, 168, 1, 177");
 // display.display();
  
}

void loop(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
//    cliente();
    currentTime = millis();
    previousTime = currentTime;
    while (client.connected() && currentTime - previousTime <= timeoutTime) { // loop while the client's connected
      currentTime = millis();         
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /5/on") >= 0) {
              Serial.println("GPIO 5 on");
              
               //BTserial.write("data1_to_bluetooth"); 
                output5State = "on";
              //digitalWrite(output5, HIGH);
            
            } else if (header.indexOf("GET /5/off") >= 0) {
              Serial.println("GPIO 5 off");
              output5State = "off";
             // digitalWrite(output5, LOW);
            } else if (header.indexOf("GET /4/on") >= 0) {
              Serial.println("GPIO 4 on");
              output4State = "on";
              //digitalWrite(output0, HIGH);
            } else if (header.indexOf("GET /4/off") >= 0) {
              Serial.println("GPIO 4 off");
              output4State = "off";
             // digitalWrite(output0, LOW);
            }
            else if (header.indexOf("GET /3/on") >= 0) {
              Serial.println("GPIO 3 on");
              output3State = "on";
              //digitalWrite(output0, HIGH);
            } else if (header.indexOf("GET /3/off") >= 0) {
              Serial.println("GPIO 3 off");
              output3State = "off";
              //digitalWrite(output0, LOW);
            }


             else if (header.indexOf("GET /2/on") >= 0) {
              Serial.println("GPIO 2 on");
              output2State = "on";
              //digitalWrite(output0, HIGH);
            } else if (header.indexOf("GET /2/off") >= 0) {
              Serial.println("GPIO 2 off");
              output2State = "off";
              //digitalWrite(output0, LOW);
            }





            
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>Hexapodo_Server</h1>");
            
            // Display current state, and ON/OFF buttons for GPIO 5  
            client.println("<p>GPIO 5 - State " + output5State + "</p>");
            // If the output5State is off, it displays the ON button       
            if (output5State=="off") {
              client.println("<p><a href=\"/5/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/5/off\"><button class=\"button button2\">OFF</button></a></p>");
            } 

//************************************boton 3 test *********************************************************************+
   client.println("<p>GPIO 3 - State " + output3State + "</p>");
            // If the output3State is off, it displays the ON button       
            if (output3State=="off") {
              client.println("<p><a href=\"/3/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/3/off\"><button class=\"button button2\">OFF</button></a></p>");
            } 
               

//******************************************************************************************************************************************************

//************************************boton 2 test *********************************************************************+
   client.println("<p>GPIO 2 - State " + output2State + "</p>");
            // If the output2State is off, it displays the ON button       
            if (output2State=="off") {
              client.println("<p><a href=\"/2/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/2/off\"><button class=\"button button2\">OFF</button></a></p>");
            } 
               

//******************************************************************************************************************************************************


               
            // Display current state, and ON/OFF buttons for GPIO 4  
            client.println("<p>GPIO 4 - State " + output4State + "</p>");
            // If the output4State is off, it displays the ON button       
            if (output4State=="off") {
              client.println("<p><a href=\"/4/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/4/off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
//desconectado();

    
  }


  if (output5State =="on"){
    avanza();
  }
  
  
 if (output3State =="on"){
    retrocede();
 }

 if (output2State =="on"){
    izquierda();
 }

 
 if (output4State =="on"){
    derecha();
 }


}
void avanza(){

    BlueTooth.print("V1"); // Vorpal hexapod radio protocol header version 1 (Byte 0 and 1)
    //forzando para que avanze w 1 f
    int eight =8;
    CurCmd =87;
    CurSubCmd=1;
    CurDpad=102;              // f in ascii
    
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


void retrocede(){

    BlueTooth.print("V1"); // Vorpal hexapod radio protocol header version 1 (Byte 0 and 1)
    //forzando para que avanze w 1 b
    int eight =8;
    CurCmd =87;
    CurSubCmd=1;
    CurDpad=98;                                 // b in ascii              
    
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

void izquierda(){

    BlueTooth.print("V1"); // Vorpal hexapod radio protocol header version 1 (Byte 0 and 1)
    //forzando para que avanze w 1 l
    int eight =8;
    CurCmd =87;
    CurSubCmd=1;
    CurDpad=108;              // l in ascii
    
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

void derecha(){

    BlueTooth.print("V1"); // Vorpal hexapod radio protocol header version 1 (Byte 0 and 1)
    //forzando para que avanze w 1 r
    int eight =8;
    CurCmd =87;
    CurSubCmd=1;
    CurDpad=114;                  //r in ascii
    
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
void para(){

    BlueTooth.print("V1"); // Vorpal hexapod radio protocol header version 1 (Byte 0 and 1)
    //forzando para que avanze w 1 s
    int eight =8;
    CurCmd =87;
    CurSubCmd=1;
    CurDpad=115;                  //s in ascii
    
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
/*
void cliente(){
  display.clear();
  display.drawString(1, 10, "New client");
  display.display();
}
void desconectado(){
  // clear the display
//  display.clear();
  //display.drawString(1, 10, "disconected");
  display.display();
}
  */
