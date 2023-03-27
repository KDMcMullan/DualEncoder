// Ken McMullan, March 2023

#include <arduino.h>
#include <Wire.h>
#include "DigiJoystick.h" // output on joystick buttons for debug!

#define byteB 6 // buttons

#define ExtAddr 0x20 // all address bits pulled low

// IOCON.BANK = 0 by default, so adddresses are sequential
#define regIODIRA  0x00 // IO direction
#define regIODIRB  0x01
#define regIPOLA   0x02 // GPIO polatity (1=invert)
#define regIPOLB   0x03
#define regINTENA  0x04 // interrupt on change enable 
#define regINTENB  0x05
#define regINTCONA 0x08 // interrupt control (0=last value) 
#define regINTCONB 0x09
#define regGPPUA   0x0C // pullup resistors (1=pullup)
#define regGPPUB   0x0D
#define regGPIOA   0x12 // GPIO port B value
#define regGPIOB   0x13

#define regIOCON   0x13 // GPIO port B value

#define maskPortB 0b11100111 // bits 4 and 3 are not input

#define pinLED 1

char jBuf[8]; // char jBuf[8] = { x, y, xrot, yrot, zrot, slider, buttonLowByte, buttonHighByte };
char eBuf[2]; // 16 bit I2C buffer

//const long InterDelay = 0;   // delay between circles
char tog = 0; // state of watchdog LED

void setup() {

  // initialize the ATTiny pins
  pinMode(pinLED, OUTPUT); // LED on Model A   

  // initialize the MCP23017
  Wire.begin();

  Wire.beginTransmission(ExtAddr);  // Start I2C Transmission
  Wire.write(regIODIRB);
  Wire.write(maskPortB); // port B inputs, except B3 and B4
  Wire.endTransmission();
  Wire.beginTransmission(ExtAddr);  // Start I2C Transmission
  Wire.write(regGPPUB);
  Wire.write(maskPortB); // portB pullups enabled, except B3 and B4
  Wire.endTransmission();
  Wire.beginTransmission(ExtAddr);  // Start I2C Transmission
  Wire.write(regINTENB);
  Wire.write(maskPortB); // portB interrupts, except B3 and B4
  Wire.endTransmission();
  Wire.beginTransmission(ExtAddr);  // Start I2C Transmission
  Wire.write(regINTCONB);
  Wire.write(maskPortB); // portB value compared against last
  Wire.endTransmission();

}

void loop() {

  Wire.beginTransmission(ExtAddr);  // Start I2C Transmission
  Wire.write(regGPIOB);
  Wire.endTransmission();

  Wire.requestFrom(ExtAddr, 1);    // Request 1 byte of data

  if (Wire.available() == 1)    // Read requested bytes of data: the ADV value, MSbyte first
  {
    eBuf[0] = Wire.read(); // MS byte
//    eBuf[1] = Wire.read(); // LS byte

  }

  if (eBuf[0] != eBuf[1]) {
    jBuf[byteB] = eBuf[0]; // move byte into joystick buffer
    eBuf[1] = eBuf[0];
    DigiJoystick.setValues(jBuf); // output the joystick buffer if it's been updated
  }  

  DigiJoystick.delay(500); // delay while keeping joystick active

  if (tog == 0) {tog = 1;} else {tog  = 0;}
  digitalWrite(pinLED, tog);

}
