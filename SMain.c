/*
 * File:   SMain.c
 * Author: Timothy Marchant
 *
 * Created on March 13, 2024, 11:58 PM
 * Formally this project was my first real attempt at a real embedded project with a pic microcontroller
 * I'm not expecting that the timer modules are that accurate for long periods of time,
 * I noticed when I try to match up with TimerMode, it seems like it slowly drifted faster than my phone's timer.
 * Also this project only uses 5 I/O pins and uses the 74hc165 and 74hc595 shift registers.
 */

//initialize functions.
unsigned char Shift_In(void);
void Shift_Out(unsigned char);
unsigned char WhichNumber(unsigned char);
void displayDigit(unsigned char, unsigned char);
void DisplayTime(void);
void DisplayTMR(void);
void DisplayNumber(unsigned int);
void ChangeTimedigit(_Bool, unsigned char, signed char[], _Bool);
_Bool ConfigureTime(_Bool);
void TimerButtonMenu(unsigned char);
void TimerMode(void);
void buttonmenu(unsigned char);
void main(void);
//include CONFIG file.
#include "CONFIG.h"
//32 Mhz
#define _XTAL_FREQ 32000000
//TRISA register bits.  Not using, but I figured I should leave these here so I know what goes to what.
#define RCLK_TRI TRISA5 
#define SER_TRI TRISA4 
#define SRCLK_TRI TRISA2 
#define Q7_TRI TRISA1
#define SHLD_TRI TRISA0
//LATA register bits.
#define RCLK_LATA LATA5 
#define SER_LATA LATA4 
#define SRCLK_LATA LATA2 
#define SHLD_LATA LATA0
//we can reuse the clock pin of the 74hc595 shift register.
//Since the 74hc595 only outputs when the RCLK pin is set high.
#define CLK_LATA LATA2
//read input from PortA.  I want to make this RA3, but when I tried doing so I had problems (even after turning off the MCRLE bit).
#define Q7_PORT PORTAbits.RA1
//I like having HIGH and LOW.
#define HIGH 1
#define LOW 0
//There are 3600 seconds per hour.
//86,400 seconds  per day
//RTCSeconds it the current time (or at least current time measured by the pic)
unsigned long RTCSeconds = 0;
unsigned int TMR0count = 0;
unsigned int TMR2count = 0;
//this is global since it makes certain things easier and also uses less overall program memory.
unsigned int TimerTime = 0;
//bit to determine if a button is being held down.
__bit Pressed=0;
//7-segment binary representation.
enum SegmentValues {
    /*the datasheet has segments that have a corresponding letter associated with it.
    For this project you can read the binary numbers like 0b A B C D E F G DP where DP=decimal point.
    if you see a 1 that means that segment is lit up, otherwise it's not used for that number.
    the last bit is the decimal point in the digit spot.  For the current application, it is unnessary to light up.*/
    Zero = 0b11111100,
    One = 0b01100000,
    Two = 0b11011010,
    Three = 0b11110010,
    Four = 0b01100110,
    Five = 0b10110110,
    Six = 0b10111110,
    Seven = 0b11100000,
    Eight = 0b11111110,
    Nine = 0b11100110,
};
//The delay caused by timer0 is 0.0005 seconds
//If the delay is 0.0005, then one second means TMR0count == 2000, 0.5 is TMR0count == 1000 and so on.
//the delay for timer2 is 0.001 seconds.  So for TMR2count==1000, the delay is one second.
//the interrupt manager only uses TMR0 and TMR2, I plan on using TMR1 for handling debouncing.
//Also when I can free up another pin use it to check if there is a input without polling 
void __interrupt() ISR(void) {
    if (TMR0IF == 1) {
        TMR0count++;
        TMR0 = 131;

        if (TMR0count == 2000) {
            TMR0count = 0;
            RTCSeconds++;
        }
        TMR0IF = 0;
    }
    if (TMR2IF == 1) {
        TMR2count++;
        TMR2 = 131;
        if (TMR2count == 1000) {
            TMR2count = 0;
            //decrement timer time.
            TimerTime--;
        }
        TMR2IF = 0;
    }
}
//this method reads in inputs from the 74hc165 shift register.
unsigned char Shift_In(void) {
    //latch pin hold low to sample.
    SHLD_LATA = LOW;
    //a delay is required for sampling from the latch pin.  The minimum time required for it is greater than the
    //time required to evaluate the next instructions according to the pic16f18313 datasheet.  
    __delay_us(10);
    //end sampling.
    SHLD_LATA = HIGH;
    unsigned char data = 0;
    for (signed char i = 7; i >= 0; i--) {
        //read Q7.
        if (Q7_PORT == HIGH) {
            //we need to write to the correct bit.  we're reading from the least significant bit, 
            //to most significant.  To do this we need to shift the binary string 10000000
            //i places to the right.
            data = data | (0b10000000 >> i);
        }
        //shift to the next bit.
        CLK_LATA = HIGH;
        CLK_LATA = LOW;
    }
    //set the Pressed bit high to make it so no more inputs can be read.
    if (data!=0&&!Pressed){
        Pressed=1;
    }
    //we don't want to keep taking in inputs while a button is still pressed.
    else if (data!=0&&Pressed){
        return 0;
    }
    //normally we would have to worry about debouncing, but it seems there's
    //a long enough delay without one?
    else {
        Pressed=0;
    }
    return data;
}
//this method shifts out data to the two 74hc595 shift registers.
void Shift_Out(unsigned char data) {

    for (signed char i = 7; i >= 0; i--) {
        unsigned char temp = data;
        //most significant to least significant bit.
        //Shift i bits to the left, then we need to put back where 
        //we had the original bit and then shift it 	//j bits to the right.
        //temp=(temp>>i)&0b00000001;
        temp = temp << i;
        temp = temp >> 7;
        if (temp==1) {
            SER_LATA = HIGH;
        } else {
            SER_LATA = LOW;
        }
        SRCLK_LATA = HIGH;
        SRCLK_LATA = LOW;
    }
}
//this just returns the binary representation of a set of segment values for a specific number.
unsigned char WhichNumber(unsigned char number) {
    switch (number) {
        case 0:
            return Zero;
        case 1:
            return One;
        case 2:
            return Two;
        case 3:
            return Three;
        case 4:
            return Four;
        case 5:
            return Five;
        case 6:
            return Six;
        case 7:
            return Seven;
        case 8:
            return Eight;
        case 9:
            return Nine;
    }
}
//Display a single digit.  Used in ConfigureTime.
void displayDigit(unsigned char number, unsigned char digit) {
    RCLK_LATA = 0;
    Shift_Out(number);
    Shift_Out(digit);
    RCLK_LATA = 1;
}

void CheckifValidSeconds(void) {
    //there are 86400 seconds in a day.
    if (RTCSeconds == 86400) {
        RTCSeconds = 0;
    }
}
//24 hour clock format; displays current time.
void DisplayTime(void) {
    //this loop runs four times, we start at the most significant digit and go to the right.
    //Since this method ends as soon as the loop ends we don't have to reset the digit variable.
    //we need time in minutes.
    unsigned int minutes = (unsigned int) (RTCSeconds / 60);
    //an hour ranges from 0 to 24.
    unsigned int hours = (minutes / 60) % 24;
    minutes %= 60;
    unsigned char digit = 0b10000000;
    unsigned int temp = 1000;
    for (signed char i = 3; i >= 0; i--) {
        RCLK_LATA = LOW;
        //for displaying hours
        unsigned char number = 0;
        if (i > 1) {
            //Shift_Out(WhichNumber((unsigned char) (((100*hours)/temp)%10)));
            number = (unsigned char) (((100 * hours) / temp) % 10);
        }            //for displaying minutes
        else {
            //Shift_Out(WhichNumber((unsigned char) ((minutes/temp)%10)));
            number = (unsigned char) ((minutes / temp) % 10);
        }
        Shift_Out(WhichNumber(number));
        //while not necessary to do this, if you want to use the colon, we need to use another output on the 74hc595 chip.
        //alterantively you can just wire the actual display so that it's always on.
        Shift_Out(digit);
        temp /= 10;
        RCLK_LATA = HIGH;
        digit = digit >> 1;
    }
}
//timer display.
//max time is 99 minutes and 59 seconds.  Or 99:59
void DisplayTMR(void) {
    //for the timer there are 5999 seconds in total.
    unsigned char minutes = (TimerTime / 60);
    unsigned char seconds = (TimerTime % 60);
    unsigned char digit = 0b10000000;
    unsigned char temp = 10;
    for (signed char i = 3; i >= 0; i--) {
        RCLK_LATA = LOW;
        if (i == 1) {
            temp = 10;
        }
        unsigned char number = 0;
        //for displaying minutes.
        if (i > 1) {
            number = minutes / temp;
        }            
        //for displaying seconds
        else {
            number = seconds / temp;
        }
        number %= 10;
        Shift_Out(WhichNumber(number));
        Shift_Out(digit);
        temp /= 10;
        RCLK_LATA = HIGH;
        digit = digit >> 1;
    }
}
//generic displaying of a number.  Not called, but left here in case it's needed.
void DisplayNumber(unsigned int number) {
    //this loop runs for times, we start at the most significant digit and go to the right.
    //Since this method ends as soon as the loop ends we don't have to reset the digit variable.
    unsigned char digit = 0b10000000;
    for (signed char i = 3; i >= 0; i--) {
        RCLK_LATA = LOW;
        int j = 1;
        //if you're wondering if it would be easier to declare j outside of the loop and divide at the end of each
        //iteration, it actually takes up an extra 30 bytes of flash storage, I'm not joking.
        for (unsigned char l = 0; l < i; l++) {
            j *= 10;
        }
        //shift out the bits in the shift register.  We use the mod operator so that we know which number to pick
        //when we call the whichnumber method.
        Shift_Out(WhichNumber((unsigned char) ((number / j) % 10)));
        Shift_Out(digit);
        digit = digit >> 1;
        RCLK_LATA = HIGH;

    }
}
//increment or decrement the currently selected number.
void ChangeTimedigit(_Bool isClock, unsigned char digit, signed char number[], _Bool isIncrement) {
    if (isIncrement) {
        number[digit]++;
    } else {
        //modulo in c apparently is not the same as it is in mathematics.
        //so when we reach 0 we can't decrement any further.
        if (number[digit]==0){
            return;
        }
        number[digit]--;
    }
    //do necessary math to make sure you don't exceed max values for certain digits.
    if (digit == 0 && isClock) {
        number[digit] %= 3;
    } else if (digit == 1 && number[0] == 2 && isClock) {
        number[digit] %= 5;
    } else if (digit == 2) {
        number[digit] %= 6;
    } else {
        number[digit] %= 10;
    }
    
}
//isClock means, if it referring to the actual clock mode, if it's one then it's in clock mode. 
//if it's a timer it's zero.
_Bool ConfigureTime(_Bool isClock) {
    signed char number[4] = {0};
    signed char currentdigit = 0;
    unsigned char data=0b00000000;
    while (data != 0b00000001){
        data=Shift_In();
        //display the currently selected digit.
        displayDigit(WhichNumber(number[currentdigit]), 0b10000000>>currentdigit);
        switch (data) {
                //increment
            case 0b10000000:
                ChangeTimedigit(isClock, currentdigit, number, 1);
                break;
                //decrement
            case 0b01000000:
                ChangeTimedigit(isClock, currentdigit, number, 0);
                break;
                //Move to the right
            case 0b00100000:
                currentdigit = (currentdigit + 1) % 4;
                break;
                //Move to the Left
            case 0b00010000:
                currentdigit = (currentdigit - 1) % 4;
                break;
                //exit cancel what we're doing.
            case 0b00000010:
                return 0;
                //we don't have a case for 0b00000001 since we need to do some more stuff outside of the switch statement.
        }
    } 
    if (isClock) {
        RTCSeconds = ((unsigned long) number[0]*10 + number[1])*3600 + 60 * ((unsigned long) number[2]*10 + number[3]);
    } else {
        TimerTime = ((unsigned long) number[0]*10 + number[1])*60 + ((unsigned long) number[2]*10 + number[3]);
    }
    return 1;
}

void TimerButtonMenu(unsigned char data) {
    //pause by
    if (data == 0b10000000) {
        TMR2ON = !TMR2ON;
    }        //configure the current time
    else if (data == 0b01000000) {
        ConfigureTime(0);
    }        //exit timer mode.  Just set the timer to 0.
    else if (data == 0b00000010) {
        TimerTime = 0;
    }
}

void TimerMode(void) {
    //setup start time
    //if not 1, end the method we're exiting.
    if (!ConfigureTime(0)) {
        return;
    }
    //Setup timer2 module here.
    TMR2ON = 1;
    while (TimerTime != 0) {
        DisplayTMR();
        TimerButtonMenu(Shift_In());
    }
    TMR2ON = 0;
}
//do something based on which button is pressed.  This method allows the user to set the time and use timer mode.
void buttonmenu(unsigned char data) {
    //Set time button
    if (data == 0b10000000) {
        displayDigit(Zero,0b10000000);
        //disable timer interrupt so that we can reset the current time on the clock.
        ConfigureTime(1);
        TMR0 = 131;
    }        
    //Set Timer Mode use seperate timer module so that we can keep the original time constantly updated.
    else if (data == 0b01000000) {
        TMR2=131;
        TimerMode();
    }

}

void main(void) {
    //initialize our I/O pins and the TMR0 module.
    ANSELA = 0;
    PORTA = 0;
    //Q7 is configured as an input and everything else is an output.
    TRISA = 0b00001010;
    T0EN = 1;
    T016BIT = 0;
    T0CON1 = 0b01110110;
    //Setup timer 2.  But don't enable the module.
    T2CONbits.T2CKPS = 0b11;
    TMR2IF = 0;
    TMR2IE = 1;
    //An interrupt is caused when TMR2==PR2 according to the datasheet.
    //preload TMR2 with 131 since that gives the same delay as TMR0.
    TMR2 = 131;
    PR2 = 0xff;
    TMR0 = 131;
    GIE = 1;
    PEIE = 1;
    TMR0IE = 1;
    TMR0IF = 0;
    while (1) {
        //since we don't have any extra pins we have to poll in order to check inputs.
        //when I find the time, I will try to use RA3 as an input and use only 3 other pins for the shift registers.
        buttonmenu(Shift_In());
        DisplayTime();
    }
}
