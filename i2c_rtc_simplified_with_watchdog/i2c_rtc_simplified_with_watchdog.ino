/* 
 * This file is part of the Arduino i2c orja distribution (https://github.com/rainisto/arduino_i2c_orja)
 * Copyright (c) 2015-2022 Jonni Rainisto (rainisto@iki.fi)
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <Wire.h>
#include <AltSoftSerial.h>
#include <NeoSWSerial.h>
#include <avr/wdt.h>

NeoSWSerial serial2(3,4);    // RX, TX
AltSoftSerial serial3;       // 8, 9

enum { 
  I2C_CMD_PING_BOOT = 0xfd,
  I2C_CMD_PING_QUERY = 0xfe
};
extern const byte supportedI2Ccmd[] = { 
  I2C_CMD_PING_BOOT, I2C_CMD_PING_QUERY
};

enum { 
  I2C_MSG_ARGS_MAX = 3,
  I2C_RESP_LEN_MAX = 3
};
#define TWI_FREQ_SETTING         400000L       // 400KHz for I2C
#define CPU_FREQ                 16000000L     // 16MHz
#define SLAVE_ADDRESS            0x2e
byte zero = 0x00;                              //workaround for issue #527
#define MAX_REGISTER             0x7F
byte previousRequest=0x0;

byte writeMask=0x80;
byte writeAddress=0x0;
byte writeHigh=0x0;
byte writeLow=0x0;
int writeOrigin=0;
int roPinValue=0;

int okPending=0;

int readQueue[MAX_REGISTER];

int argsCnt = 0;                        // how many arguments were passed with given command
int requestedCmd = -1;                  // which command was requested (if any)

byte i2cArgs[I2C_MSG_ARGS_MAX];         // array to store args received from master
int i2cArgsLen = 0;                     // how many args passed by master to given command

uint8_t i2cResponse[I2C_RESP_LEN_MAX];  // array to store response
int i2cResponseLen = 0;                 // response length

void setup()
{
  delay(1000); 
  // TWBR = ((CPU_FREQ / TWI_FREQ_SETTING) - 16) / 2;
  Wire.begin(SLAVE_ADDRESS);
  int i;

  // Initialize readQueue
  for (i=0; i <= MAX_REGISTER; i++) {
    readQueue[i]=0;
  }
  // config pins
  pinMode(5, INPUT_PULLUP);
  pinMode(6, INPUT_PULLUP);
  pinMode(7, INPUT_PULLUP);

  // Initialize serial ports
  if (digitalRead(5)) {
    Serial.begin(9600);
  } else {
    Serial.begin(115200);
  }
  Serial.println(F("\nI2C Orja v1.0 (serial1)"));
  if (digitalRead(6)) {
    serial2.begin(9600);
  } else {
    serial2.begin(38400);
  }
  serial2.println(F("\nI2C Orja (serial2)"));
  if (digitalRead(7)) {
    serial3.begin(115200);
  } else {
    serial3.begin(9600);
  }
  serial3.println(F("\nI2C Orja (serial3)"));

  Wire.onRequest(requestEvent); // register event handlers
  Wire.onReceive(receiveEvent); // register event handlers
  wdt_enable(WDTO_8S);          // enable watchdog
}

byte getVal(char c)
{
   if(c >= '0' && c <= '9') return (byte)(c - '0');
   else return (byte)(c-'A'+10);
}

void requestEvent(){
  Wire.write(i2cResponse, i2cResponseLen);
}

void receiveEvent(int howMany)
{
  int cmdRcvd = -1;
  int argIndex = -1; 
  argsCnt = 0;

  if (Wire.available()) {
    cmdRcvd = Wire.read();                       // receive first byte - command assumed
    while (Wire.available()) {                   // receive rest of tramsmission from master assuming arguments to the command
      if (argIndex < I2C_MSG_ARGS_MAX){
        argIndex++;
        i2cArgs[argIndex] = Wire.read();
      } else {
        Serial.println(F("Too many arguments")); // only printing errors to main serial
        return;
      }
      argsCnt = argIndex+1;  
    }
  } else {
    Serial.println(F("empty request"));
    return;
  }
  // validating command is supported by slave
  int fcnt = -1;
  //for (int i = 0; i < sizeof(supportedI2Ccmd); i++) { // cast to avoid compiler warning
  for (int i = 0; i < (int)sizeof(supportedI2Ccmd); i++) {
    if (supportedI2Ccmd[i] == cmdRcvd) {
      fcnt = i;
    }
  }
  if (cmdRcvd >= 0 && cmdRcvd <= MAX_REGISTER) fcnt = cmdRcvd;

  if (fcnt<0){
    Serial.print(F("command not supported:"));
    Serial.println(cmdRcvd);
    return;
  }
  requestedCmd = cmdRcvd;
  // now main loop code should pick up a command to execute and prepare required response when master waits before requesting response
}

// buffer to each serial port
String inData;
String inData2;
String inData3;

void loop()
{
  wdt_reset(); // prevent watchdog from rebooting
  int hits=0;

  // if heatpump asks that do I need to query anything
  if (requestedCmd == I2C_CMD_PING_BOOT || requestedCmd == I2C_CMD_PING_QUERY) {
    int i;
    // Iterate over all registers and any of the serials have raised interest to specific register value, then
    // append it to i2cResponse (so we ask pump to return those values).
    for (i=0; i <= MAX_REGISTER; i++) {
      if (readQueue[i]>0) {
        i2cResponseLen = 0;
        i2cResponseLen++;
        i2cResponse[i2cResponseLen -1] = i;
        hits=1;
        okPending=readQueue[i]; // mark okPending to indicate that we are waiting for response for found register number
        break;
      }  
    }
    // if ATW has triggered writeAdress and there is no read requests on the queue, then we can ask for write
    // a value to specific register address
    if (writeAddress && hits==0) {
      okPending=writeOrigin; // informs which serial interface is waiting OK message for the write command
      hits=1;
      i2cResponseLen = 0;
      i2cResponseLen++;
      i2cResponse[i2cResponseLen -1] = writeAddress;
      i2cResponseLen++;
      i2cResponse[i2cResponseLen -1] = writeLow;
      i2cResponseLen++;
      i2cResponse[i2cResponseLen -1] = writeHigh;
      // clear write request afterwards
      writeAddress=0x0;
      writeLow=0x0;
      writeHigh=0x0;
      writeOrigin=0;
    }
    if (hits==0) {
      // when there is no more read or write commands pending then print OK to serial consoles that
      // were issuing commnds (resolved by bitfield in okPenging so &1, &2 and &4)
      i2cResponseLen = 0;
      i2cResponseLen++;
      i2cResponse[i2cResponseLen -1] = 0xFF; // 0xFF == I don't need any more data currently
      if (okPending) {
        if (okPending&1) Serial.println(F("OK\n"));
        if (okPending&2) serial2.println(F("OK\n"));
        if (okPending&4) serial3.println(F("OK\n"));
        okPending=0;
      }
    }
    requestedCmd = -1;   // set requestd cmd to -1 disabling processing in next loop
  } else if (requestedCmd >= 0 && requestedCmd <= MAX_REGISTER){
    // log the requested function is unsupported (e.g. by writing to serial port or soft serial)
    // requestedCmd is registerNumber+"="+value(high+low) bytes
    // If will check which serial terminal was interested about the received register value
    // and then prints it if there is a match
    if (readQueue[requestedCmd]&1) {
      if (requestedCmd<16) Serial.print(F("0"));
      Serial.print(requestedCmd, HEX); 
      Serial.print(F("="));
      if (i2cArgs[1]<16) Serial.print(F("0"));
      Serial.print(i2cArgs[1], HEX);
      if (i2cArgs[0]<16) Serial.print(F("0"));
      Serial.println(i2cArgs[0], HEX);
    }
    if (readQueue[requestedCmd]&2) {
      if (requestedCmd<16) serial2.print(F("0"));
      serial2.print(requestedCmd, HEX); 
      serial2.print(F("="));
      if (i2cArgs[1]<16) serial2.print(F("0"));
      serial2.print(i2cArgs[1], HEX);
      if (i2cArgs[0]<16) serial2.print(F("0"));
      serial2.println(i2cArgs[0], HEX);
    }
    if (readQueue[requestedCmd]&4) {
      if (requestedCmd<16) serial3.print(F("0"));
      serial3.print(requestedCmd, HEX); 
      serial3.print(F("="));
      if (i2cArgs[1]<16) serial3.print(F("0"));
      serial3.print(i2cArgs[1], HEX);
      if (i2cArgs[0]<16) serial3.print(F("0"));
      serial3.println(i2cArgs[0], HEX);
    }
    readQueue[requestedCmd]=0;
    requestedCmd = -1;   // set requestd cmd to -1 disabling processing in next loop
  }

  // iterative code for each serial interface
  handle_serial(Serial , inData , 1);
  handle_serial(serial2, inData2, 2);
  handle_serial(serial3, inData3, 4);

}

void handle_serial(Stream &serialport, String &input_data, int bit_id ) {
  while (serialport.available() > 0)
  {
    char recieved = serialport.read();
    if (input_data.length() > 0 && (recieved == 8 || recieved == 127)) {
      input_data[input_data.length()]=0;
    } else {
      input_data += recieved;
    }
    serialport.print(recieved); // local echo
    // Process message when new line character is recieved
    if (recieved == '\n' && input_data.length()<2) {
      input_data="";
      recieved=0;
    }
    if (recieved == '\r' || recieved == '\n' || input_data.length() > 25)
    {
      if (recieved == '\r') serialport.println(); // remove if it bothers
      input_data.toUpperCase();
      if (input_data.substring(0,3).equalsIgnoreCase("ATI")) {
        // Fake that we are ThermIQ device
        serialport.println(F("ThermIQ2 v.4.30 0011"));
        serialport.println(F("OK\n"));
      } else if (input_data.substring(0,3).equalsIgnoreCase("ATR")) {
        int pituus = input_data.substring(3).length();
        byte start = getVal(input_data[4]) + (getVal(input_data[3]) << 4);
        if (start > MAX_REGISTER) { serialport.println(F("NO\n")); input_data = ""; return; }   // error handling
        readQueue[start]=readQueue[start]|bit_id;
        if (pituus > 4) {
          byte untill = getVal(input_data[6]) + (getVal(input_data[5]) << 4);
          if (untill > MAX_REGISTER) { serialport.println(F("NO\n")); input_data = ""; return; } // error handling
          int i;
          for (i=start+1; i <= untill; i++) {
            readQueue[i]=readQueue[i]|bit_id;
          }
          if (pituus > 8) {
            start = getVal(input_data[8]) + (getVal(input_data[7]) << 4);
            if (start > MAX_REGISTER) { serialport.println(F("NO\n")); input_data = ""; return; } // error handling
            untill = getVal(input_data[10]) + (getVal(input_data[9]) << 4);
            if (untill > MAX_REGISTER) { serialport.println(F("NO\n")); input_data = ""; return; } // error handling
            int i;
            for (i=start; i <= untill; i++) {
              readQueue[i]=readQueue[i]|bit_id;
            }
          }
        }
      } else if (input_data.substring(0,3).equalsIgnoreCase("ATW")&&(analogRead(A6) > 100)) {
        // atwxxhhll               Write hhll to register xx
        int pituus = input_data.substring(3).length();
        if (pituus > 6) {
          writeAddress = getVal(input_data[4]) + (getVal(input_data[3]) << 4);
          if (writeAddress < writeMask) writeAddress = writeAddress + writeMask;
          writeHigh = getVal(input_data[6]) + (getVal(input_data[5]) << 4);
          writeLow = getVal(input_data[8]) + (getVal(input_data[7]) << 4);
          writeOrigin = bit_id;
        }
      } else if (input_data.substring(0,3).equalsIgnoreCase("ATB")) {
        serialport.println(F("Not supported in this emulation"));
        serialport.println(F("NO\n"));
      } else if (input_data.substring(0,3).equalsIgnoreCase("ATP")) {
        serialport.println(F("Protect pin state"));
        roPinValue=analogRead(A6);
        serialport.println(roPinValue);
        serialport.println(F("OK\n"));
      } else if (input_data.substring(0,4).equalsIgnoreCase("ATL?")) {
        serialport.println(F("1"));
        serialport.println(F("OK"));
      } else if (input_data.substring(0,4).equalsIgnoreCase("ATTG")) {
        serialport.println(F("NO\n"));
      } else if (input_data.substring(0,4).equalsIgnoreCase("ATTS")) {
        serialport.println(F("NO\n"));
      } else {
        serialport.print(F("Received unknown command: "));
        serialport.println(input_data);
        serialport.println(F("NO\n"));
      }
      input_data = ""; // Clear recieved buffer
    }
  }
}
