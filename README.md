# DualEncoder
A DigiSpark with Two Quadrature Encoders via a DIO Expander.

Connecting one quad encoder directly to the DigiSpark has been done: The chip has 6 IO pins:
  P0 (available)
  P1 (already in use by LED, but could be used)
  P2 (Available)
  P3 USB+ (I wanted a USB device, so these pins are unavailable.)
  P4 USB-
  P5 RESET (unavailable - it resets the device. Not used, as I didn't want to have to use HVP.) 

The challenge was to add two quad encoders (each with an addition bushbutton) to the DigiSpark. Yes, I could have simply done this with a micro containing more DIO, but I used what I had lying around and I wanted to see if it cold be done. I've used the MCP23017 I/O expander: more overkill, but now by DigiSpark has 16 digital IO pins.
