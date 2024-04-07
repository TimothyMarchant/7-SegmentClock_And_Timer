/*
 * File:   SMain.c
 * Author: Timothy Marchant
 *
 * Created on March 13, 2024, 11:58 PM
 * Formally this project was my first real attempt at a real embedded project with a pic microcontroller
 * I'm not expecting that the timer modules are that accurate for long periods of time,
 * The little bit of testing I did do to check for drifting it's kinda hard to tell if there was any noticeable drift. 
 * Also this project only uses 5 I/O pins and uses the 74hc165 and 74hc595 shift registers for expanding I/O.
 * And a 7-segment display is being used.
 * As of right now I consider this project to be complete, I may in the future decide to maybe add an alarm setting?
 * Assuming I have enough space to work with (this version uses 84% of programmable memory).
 */

//initialize functions.
void Shift_In(void);
void Shift_Out(unsigned char);
unsigned char WhichNumber(unsigned char);
void displayDigit(unsigned char, unsigned char);
void DisplayTime(void);
void DisplayTMR(void);
void ChangeTimedigit(_Bool, unsigned char, signed char[], _Bool);
_Bool ConfigureTime(_Bool);
void TimerButtonMenu(void);
void TimerMode(void);
void buttonmenu(void);
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
//reads inputs from Q7 pin of 74hc165 shift register.
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
unsigned char TMR1count=0;
//this is global since it makes certain things easier and also uses less overall program memory.
unsigned int TimerTime = 0;
//bit to determine if a button is being held down.
__bit Pressed=0;
unsigned char data=0;
//7-segment binary representation.
enum SegmentValues {
    /*the datasheet has segments that have a corresponding letter associated with it.
    For this project you can read the binary numbers like 0b A B C D E F G DP where DP=decimal point.
    if you see a 1 that means that segment is lit up, otherwise it's not used for that number.
    the last bit is the decimal point in the digit spot.  For the current application, it is unnecessary to light up.*/
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
//These are here so that we know what to light up because the arrays hold the corresponding values.
const unsigned char numbers[10]={Zero,One,Two,Three,Four,Five,Six,Seven,Eight,Nine};
const unsigned char digits[4]={0b00010000,0b00100000,0b01000000,0b10000000};
//The delay caused by timer0 is 0.001 seconds
//If the delay is 0.001, then one second means TMR0count == 1000, 0.5 is TMR0count == 500 and so on.
//the delay for timer2 is 0.001 seconds.  So for TMR2count==1000, the delay is one second.
//the interrupt manager only uses TMR0 and TMR2, I plan on using TMR1 for handling debouncing.
//Also when I can free up another pin use it to check if there is a input without polling 
void __interrupt() ISR(void) {
    if (TMR0IF) {
        TMR0count++;
        TMR0 = 131;

        if (TMR0count == 1000) {
            TMR0count = 0;
            RTCSeconds++;
            //there are 86400 seconds in a day.
            if (RTCSeconds==86400){
                RTCSeconds=0;
            }
        }
        TMR0IF = 0;
    }
    if (TMR2IF) {
        TMR2count++;
        TMR2 = 131;
        if (TMR2count == 1000) {
            TMR2count = 0;
            //decrement timer time.
            TimerTime--;
        }
        TMR2IF = 0;
    }
    //for debouncing.  The delay is 0.050 seconds.  Which is more than enough for the required delay.
    if (TMR1IF){
        TMR1ON=0;
        TMR1=15536;
        TMR1IF=0;
    }
}
//this method reads in inputs from the 74hc165 shift register.
void Shift_In(void) {
    //while TMR1 is active that means we need to wait so that we don't deal with debouncing upon letting go of the button.
    if (TMR1ON==1){
        return;
    }
    //latch pin hold low to sample.
    SHLD_LATA = LOW;
    //a delay is required for sampling from the latch pin.  The minimum time required for it is greater than the
    //time required to evaluate the next instructions according to the pic16f18313 datasheet.  
    __delay_us(10);
    //end sampling.
    SHLD_LATA = HIGH;
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
        data=0;
    }
    //if the data=0 and Pressed=1, then we can set Pressed=0 and turn on the debouncing timer.
    else if (Pressed){
        Pressed=0;
        TMR1ON=1;
    }
}
//this method shifts out data to the two 74hc595 shift registers.
void Shift_Out(unsigned char data) {

    for (signed char i = 7; i >= 0; i--) {
        unsigned char temp = data;
        //most significant to least significant bit.
        //Shift i bits to the left, then we need to shift the left most digit to the rightmost digit.
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

//Display a single digit.  Used in ConfigureTime.
void displayDigit(unsigned char number, unsigned char digit) {
    RCLK_LATA = 0;
    Shift_Out(number);
    Shift_Out(digit);
    RCLK_LATA = 1;
}

//24 hour clock format; displays current time.
void DisplayTime(void) {
    //this loop runs four times, we start at the most significant digit and go to the right.
    //Since this method ends as soon as the loop ends we don't have to reset the digit variable.
    //we need time in minutes.
    unsigned int minutes = (unsigned int) (RTCSeconds / 60);
    //an hour ranges from 0 to 24.
    //Minutes only ranges from 0 to 1439 (as RTCSeconds always resets before 1440 minutes).  So we don't need % here.
    unsigned char hours = (unsigned char) (minutes / 60);
    minutes%=60;
    unsigned char temp = 10;
    for (signed char i = 3; i >= 0; i--) {
        RCLK_LATA = LOW;
        unsigned char number = 0;
        //for displaying hours
        if (i==1){
            temp=10;
        }
        if (i > 1) {
            number = ((hours) / temp);
        }
        //for displaying minutes
        else {
            number = (unsigned char) (minutes / temp);
        }
        number%=10;
        //shiftout the segments to be turned on.
        Shift_Out(numbers[number]);
        //select which digit to light up.
        Shift_Out(digits[i]);
        temp /= 10;
        RCLK_LATA = HIGH;
    }
}
//timer display.
//max time is 99 minutes and 59 seconds.  Or 99:59
void DisplayTMR(void) {
    //for the timer there are 5999 seconds in total.
    unsigned char minutes = (TimerTime / 60);
    unsigned char seconds = (TimerTime % 60);
    unsigned char temp = 10;
    for (signed char i = 3; i >= 0; i--) {
        RCLK_LATA = LOW;
        if (i == 1) {
            temp = 10;
        }
        unsigned char number = 0;
        //for displaying minutes.
        if (i > 1) {
            number = minutes;
        }            
        //for displaying seconds
        else {
            number = seconds;
        }
        number/=temp;
        number %= 10;
        Shift_Out(numbers[number]);
        Shift_Out(digits[i]);
        temp /= 10;
        RCLK_LATA = HIGH;
    }
}
//increment or decrement the currently selected number.
void ChangeTimedigit(_Bool isClock, unsigned char digit, signed char number[], _Bool isIncrement) {
    if (isIncrement) {
        number[digit]++;
        //We need this for when you increment the first digit (which can only 1 or 2 for 24hr clock)
        //So we do this check to make sure the second digit isn't greater than 4.  If it is just set it to 3.
        if (isClock&&digit==0&&number[0]==2&&number[1]>3){
            number[1]=3;
        }
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
        number[digit] %= 4;
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
    data=0;
    while (data != 0b00000001){
        Shift_In();
        //display the currently selected digit.
        displayDigit(numbers[number[currentdigit]], 0b10000000>>currentdigit);
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
                currentdigit = (currentdigit + 1)%4;
                break;
                //Move to the Left
            case 0b00010000:
                //modulo in c is not the same as it is in mathematics.  using % on negative numbers can have unintended consequences.
                currentdigit = (currentdigit - 1);
                if (currentdigit==-1){
                    currentdigit=3;
                }
                break;
                //exit cancel what we're doing.
            case 0b00000010:
                return 0;
                //we don't have a case for 0b00000001 since we need to do some more stuff outside of the switch statement.
        }
    } 
    //we do this here since we have to do this calculation in both if-else blocks.
    unsigned char temp1=number[0]*10 + number[1];
    unsigned char temp2=number[2]*10 + number[3];
    if (isClock) {
        RTCSeconds = ((unsigned long) temp1)*3600 + 60 * ((unsigned long) temp2);
    } else {
        TimerTime = ((unsigned int) temp1)*60 + ((unsigned int) temp2);
    }
    return 1;
}

void TimerButtonMenu(void) {
    Shift_In();
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
    //Start Timer 2.
    TMR2ON = 1;
    while (TimerTime != 0) {
        DisplayTMR();
        TimerButtonMenu();
    }
    //Turn off timer 2.
    TMR2ON = 0;
    unsigned int i=0;
    //this is to indicate that the timer has ended and is waiting for the user to acknowledge that it has ended.
    while (data!=0b00000001){
        Shift_In();
        if (i<1000){
            DisplayTMR();
        }
        else {
            displayDigit(0,0);
            if (i==2000){
                i=0;
            }
        }
        i++;
    }
}
//do something based on which button is pressed.  This method allows the user to set the time and use timer mode.
void buttonmenu(void) {
    Shift_In();
    //Set time button
    if (data == 0b10000000) {
        //Setup Clock.  If they exit do nothing, otherwise reset the timer register and reset TMR1count.
        //the one in the method call is so the method knows we're changing the clock's time.
        if (ConfigureTime(1)){
            TMR0 = 131;
            TMR1count=0;
        }
    }        
    //Set Timer Mode use seperate timer module so that we can keep the original time constantly updated.
    else if (data == 0b01000000) {
        //preload TMR2 with 131, that way we can guarantee that we get a 0.001 second delay.
        TMR2=131;
        TimerMode();
    }

}
void main(void) {
    //initialize our I/O pins and the TMR0 module.
    ANSELA = 0;
    //Q7 is configured as an input and everything else is an output.
    TRISA = 0b00001010;
    //we want an 8-bit timer.
    T016BIT = 0;
    //Clock source is FOSC/4 and prescaler is 64 and TMR0 is synchronized to FOSC/4
    T0CON1 = 0b01010110;
    //Setup timer 2.  But don't enable the module.
    //we only need to set the prescaler, and it's 64 in this case.
    T2CONbits.T2CKPS = 0b11;
    TMR2IF = 0;
    TMR2IE = 1;
    //An interrupt is caused when TMR2==PR2 according to the datasheet.
    //Set PR2 to 255.
    PR2 = 0xff;
    //preload TMR0.
    TMR0 = 131;
    GIE = 1;
    PEIE = 1;
    TMR0IE = 1;
    TMR0IF = 0;
    //Setup Timer1 module
    //clock source is FOSC/4 and prescaler is 8.
    T1CON=0b00110000;
    //we don't want this register to be used.  We don't need the timer gate functionality.
    T1GCON=0b00000000;
    TMR1=15536;
    TMR1IF=0;
    TMR1IE=1;
    
    //turn on TMR0.
    T0EN = 1;
    while (1) {
        //we poll for inputs.
        buttonmenu();
        DisplayTime();
    }
}

