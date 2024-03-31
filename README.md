This project involves the PIC16F18313 microcontroller and uses its timer modules to keep time.
This specific PIC only has 6 I/O pins, but I used the SN74HC165N and SN74HC595N shift registers for expanding I/O.
I left the C file with the required code and config file I used for the microcontroller.
I should also mention that you probably can use these exact same files for the PIC16F18323 since they use the same datasheet (although you may have to use a different CONFIG file).

The actual supplies I ended up using were the following (excludes wires).

* 1 PIC16f18313

* 1 7-segment display (I used this one https://www.mouser.com/ProductDetail/485-1001)

* 4 PN2222TF transistors (NPN)

* 3 0.1 uF capacitors (two for the shift register, one for the minimun circuitry for the PIC)

* 5 22k OHM resistors (4 are used with the transistors and one is used for the colon of the 7-segment display).

* 8 10k OHM resistors

* 10 10k/4.7k ohm resistors (either value works, for the buttons I use 4.7k because that's what I had out).

* 6 buttons.

* 2 SN74HC595N Shift Registers

* 1 SN74HC165N Shift Register

