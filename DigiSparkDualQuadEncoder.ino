// Ken McMullan, March 2023

#include <arduino.h>
#include <Wire.h>
#include "DigiJoystick.h" // output on joystick buttons for debug!

#define byteL 6 // buttons
#define byteH 7 // buttons

#define ExtAddr 0x20 // all address bits pulled low

// IOCON.BANK = 0 by default, so adddresses are sequential
// IPOL (0x02 and 0x03) remain at 0x00 (default) as we don't wish to invert teh logic
// INTCON (0x08 and 0x09) remain at 0x00 (default) as we wish to compare to previous
// DEFVAL (0x06 and 0x07) remain at 0x00 (default) as compare to previous doesn't care

#define regIODIRA  0x00 // IO direction
#define regIODIRB  0x01
#define regINTENA  0x04 // interrupt on change enable 
#define regINTENB  0x05
#define regGPPUA   0x0C // pullup resistors (1=pullup)
#define regGPPUB   0x0D
#define regINTFA   0x0E // interrupt flags
#define regINTFB   0x0F
#define regINTCAPA 0x10 // capture of GPIO at time of interrupt
#define regINTCAPB 0x11
#define regGPIOA   0x12 // GPIO port B value
#define regGPIOB   0x13
#define regIOCON   0x0A // IO configuration register
#define regIOCON   0x0B

#define maskPortB 0b11100111 // bits 4 and 3 are not input

#define pinLED 1

char jBuf[8]; // char jBuf[8] = { x, y, xrot, yrot, zrot, slider, buttonLowByte, buttonHighByte };
char eBuf[2]; // 16 bit I2C buffer

//const long InterDelay = 0;   // delay between circles
char tog = 0; // state of watchdog LED

void setReg(unsigned int DevAddr, byte Reg, byte Val)
{
// assumes IOCON.SEQOP is false, so address pointer automatically increments
  Wire.beginTransmission(DevAddr);  // Start I2C Transmission
  Wire.write(Reg);
  Wire.write(Val);
  Wire.endTransmission();
}

void ReqByteFrom(unsigned int DevAddr, byte Reg)
{
  Wire.beginTransmission(DevAddr);  // Start I2C Transmission
  Wire.write(Reg);
  Wire.endTransmission();
  Wire.requestFrom(ExtAddr, 1);    // Request 1 byte of data
}

void setup() {

  // initialize the ATTiny pins
  pinMode(pinLED, OUTPUT); // LED on Model A   

  // initialize the MCP23017
  Wire.begin();

  setReg(ExtAddr, regIODIRB, maskPortB); // port B inputs, except B3 and B4
  setReg(ExtAddr, regGPPUB, maskPortB); // portB pullups enabled, except B3 and B4
  setReg(ExtAddr, regINTENB, maskPortB); // portB interrupts, except B3 and B4

}

void loop() {

  ReqByteFrom(ExtAddr, regINTFB);

  if (Wire.available() == 1)    // Read requested bytes of data: the ADV value, MSbyte first
  {
    eBuf[0] = Wire.read();

    if (eBuf[0] != 0) {

      ReqByteFrom(ExtAddr, regINTCAPB);

      if (Wire.available() == 1)    // Read requested bytes of data: the ADV value, MSbyte first
      {
        eBuf[1] = Wire.read();

        jBuf[byteL] = eBuf[0]; // move byte into joystick buffer
        jBuf[byteH] = eBuf[1]; // move byte into joystick buffer
        DigiJoystick.setValues(jBuf); // output the joystick buffer if it's been updated

      } // INTCAPB available
    } // INTFB <> 0
  } // INTFB available

  if (tog == 0) {tog = 1;} else {tog  = 0;}
  digitalWrite(pinLED, tog);

  DigiJoystick.delay(500); // delay while keeping joystick active

}
