
/*
 * The intent is to develop a flexible keyboard / mouse / joystick emulator
 * for the ATTiny85, whose inputs are a from a bank of quadrature encoders. In
 * this example, the micro is connected to a pair of encoders, each having its
 * own pushbutton. This equates to 6 digital inputs; well beyond the
 * capability of the micro. As such, an I2C connected MCP23017 IO expander has
 * been used.
 * 
 * 14 April 2023 v0.93
 * Added the promised sensitivity control, and doubled the potential
 * sensitivity.
 * The inputs are defaulted to only 25% as sensitive as they could be, with
 * the option to go to 50% or 100%.
 * Both quad encoders now work and are output to the two bytes of the joystick
 * buttons.
 *
 * 12 April 2023 v0.92
 * Added a change history.
 * Reworked to use a state machine. This means we don't wait for or
 * inadvertently ignore any I2C data. We only delay when absolutely idle.
 * Implemented logic to decode the encoder and now have an internal byte which
 * increments or decrements depending on the direction the knob is turned.
 * This byte is output (guess where) on the joystick buttons. Even thought it
 * could be made more sensitive, the strategy is arguably already too
 * effective: the knob has 20 detention points as it rotates, but each of
 * those equates to 2 increments (potentially 4). I will implement a less
 * senstive version.
 *
 * 31 March 2023 v0.91
 * Now also grabs the PORTB interrupts and puts them out on the joystick
 * buttons.
 *
 * 24 March 2023 v0.9
 * Grabs PORTB from the GPIO expander and deposits it in the joystick input
 * buttons, as a handy debug output. Hardware is working fine.
 * 
 * Ken McMullan, 2023
 * 
 * ATtiny P5 (RESET) is not used, P4 and P3 are USB comms, P1 is the onboard
 * LED. P2 and P0 are SCL and SDA, connected to the I2C interface of the GPIO
 * extender.
 * 
 * 4.7Kohm pull-up resistors are used on the I2C bus.
 * 
 * Port B Port B of the GPIO externder is used as follows:
 *  B0 encoder 1 button
 *  B1 encoder 1 input B
 *  B2 encoder 1 input A
 *  B5 encoder 2 input B
 *  B6 encoder 2 input A
 *  B7 encoder 2 button
 */
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

#define enc1A 0x04 // encoder 1 input A is bit 2
#define enc1B 0x02 // encoder 1 input B is bit 1
#define enc1S 0x01 // encoder 1 button input is bit 0
#define enc2A 0x40 // encoder 2 input A is bit 6
#define enc2B 0x20 // encoder 2 input B is bit 5
#define enc2S 0x80 // encoder 2 button input is bit 7

/* "sens" defines the sensitivity of the input. Bit 3 allows interrupts on
 * rising input A, bit 2 allows interrupt on falling input A, bit 1 allows
 * interrupt on rising input B and bit 0 allows interrupt on fallsing input B.
 *   8 = reacts only to rising edges on input A (25% = 20 hits per revolution)
 *  12 = reacts to rising and falling edges only on input A (50% = 40 hits)
 *  15 = reacts to rising and falling edges on input A and input B (100%)
 * Technically, 2, 4 and 8 are the same sensitivity as 1, for example, but the
 * above are the only settings for most needs.
 * Don't set it to zero: that would be silly.
 */
#define sens   0b1000 // we only care about bits 3..0
//#define sens   0b1100 // we only care about bits 3..0
//#define sens   0b1111 // we only care about bits 3..0

#define sensAR 0b1000 // A rising interrupt allowed
#define sensAF 0b0100 // A falling interrupt allowed
#define sensBR 0b0010 // B rising interrupt allowed
#define sensBF 0b0001 // B falling interrupt allowed

#define pinLED 1

char jBuf[8]; // char jBuf[8] = { x, y, xrot, yrot, zrot, slider, buttonLowByte, buttonHighByte };
char eBuf[2]; // 16 bit I2C buffer

void setReg(unsigned int DevAddr, byte Reg, byte Val)
{
// assumes IOCON.SEQOP is false, so address pointer automatically increments
  Wire.beginTransmission(DevAddr);  // Start I2C Transmission
  Wire.write(Reg);
  Wire.write(Val);
  Wire.endTransmission();
} // SetReg

void ReqByteFrom(unsigned int DevAddr, byte Reg)
{
  Wire.beginTransmission(DevAddr);  // Start I2C Transmission
  Wire.write(Reg);
  Wire.endTransmission();
  Wire.requestFrom(ExtAddr, 1);    // Request 1 byte of data
} // ReqByteFrom

byte Decode(unsigned int INTF, unsigned int INTCAP, byte EncA, byte EncB, byte Sw)
{
  byte ret = 0; // return value
  if (INTF & EncA) { // A changed
    if (INTCAP & EncA) { // ...and is now high
      if (sens & sensAR) { // ...and rising A interrupt is enabled
        if (INTCAP & EncB) { // B is high
          ret = 2; // clockwise
        } else { // B is low
          ret = 1; // counter-clockwise
        }
      } // rising A interrupt
    } else { // A changed and is now low 
      if (sens & sensAF) { // falling A interrupt is enabled
        if (INTCAP & EncB) { // B is high
          ret = 1; // counter-clockwise
        } else { // B is low
          ret = 2; // clockwise
        }
      } // falling A interrupt  
    } // A changed to high 
  } // A changed

  else if (INTF & EncB) { // B changed
    if (INTCAP & EncB) { // ...and is now high
      if (sens & sensBR) { // ...and rising B interrupt is enabled
        if (INTCAP & EncA) { // A is high
          ret = 1; // counter-clockwise
        } else { // A is low
          ret = 2; // clockwise
        }
      } // rising B interrupt
    } else { // B changed and is now low 
      if (sens & sensBF) { // falling B interrupt is enabled
        if (INTCAP & EncA) { // A is high
          ret = 2; // clockwise
        } else { // A is low
          ret = 1; // counter-clockwise
        }
      } // falling B interrupt  
    } // B changed to high 
  } // B changed

// else if switch buttton changed.... code goes here.... ret = 4

  return ret;
} // Decode

void setup() {

  // initialize the ATTiny pins
  pinMode(pinLED, OUTPUT); // LED on Model A   

  // initialize the MCP23017
  Wire.begin();

  setReg(ExtAddr, regIODIRB, maskPortB); // port B inputs, except B3 and B4
  setReg(ExtAddr, regGPPUB, maskPortB); // portB pullups enabled, except B3 and B4
  setReg(ExtAddr, regINTENB, maskPortB); // portB interrupts, except B3 and B4

}

byte cnt1 = 0;
byte cnt2 = 0;
char tog = 0;      // watchdog LED
byte watchdog = 0; // watchdog counter
int state = 0;     // execution state
byte input = 0;    // input from controller

void loop() {

  switch (state)
  {
    case 0: // request regINTFB
      ReqByteFrom(ExtAddr, regINTFB);
      state = 1;
      break;

    case 1: // read regINTFB / request regINTCAPB
      if (Wire.available() == 1) // is INFB available?
      {
        eBuf[0] = Wire.read(); // read INTFB
        if (eBuf[0] == 0) {    // does it indicate an interrupt?
          state = 4;           // no - go to pause
        } else {               // yes - go to read INTCAP
          ReqByteFrom(ExtAddr, regINTCAPB);
          state = 2;          
        }
      }
      break;

    case 2: // read INTCAPB
      if (Wire.available() == 1) {
        eBuf[1] = Wire.read(); 
        state = 3;
      }
      break;

    case 3: // decode and act on the digital inputs
      if (eBuf[0] & (enc1A | enc1B | enc1S)) { // encoder 1 (or button) changed
        input = Decode(eBuf[0], eBuf[1], enc1A, enc1B, enc1S);        
        if (input & 2) {
          cnt1 += 1;
        } else if (input & 1) {
          cnt1 -= 1;
        }
      }
      else if (eBuf[0] & (enc2A | enc2B | enc2S)) { // encoder 2 (or button) changed
        input = Decode(eBuf[0], eBuf[1], enc2A, enc2B, enc2S);
        if (input & 2) {
          cnt2 += 1;
        } else if (input & 1) {
          cnt2 -= 1;
        }
      }

      jBuf[byteL] = cnt1; // move byte into joystick buffer
      jBuf[byteH] = cnt2; // move byte into joystick buffer
      DigiJoystick.setValues(jBuf); // output the joystick buffer if it's been updated

      state = 4;

      break;

    case 4: // pause and toggle watchdog
      DigiJoystick.delay(10); // delay while keeping joystick active
      watchdog +=1;
      if ((watchdog & 0x20) == 0x20) { // toggle LED every 32 cycles of 10ms
        if (tog == 0) {tog = 1;} else {tog  = 0;}
        digitalWrite(pinLED, tog);
      }
      state = 0;
      break;

  } // switch
} // loop
