/* 
 * File:   Final_main.c
 * Authors: Emily Chong, Zachary Der, Ryan Dutchyn, Ryan Engman
 *
 * Created on December 1, 2022, 12:37 PM
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util/delay.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include "defines.h"
#include "hd44780.h"
#include "lcd.h"

//Define pins
#define BUZZER PB0
#define LED_RED PB4
#define LED_BLUE PB3
#define LED_YELLOW PB2
#define LED_GREEN PB1
#define LED_START PB5
#define BUTTON_START PC4 //PCINT12
#define BUTTON_RED PC3 //PCINT11
#define BUTTON_BLUE PC2 //PCINT10
#define BUTTON_YELLOW PC1 //PCINT9
#define BUTTON_GREEN PC0 //PCINT8

//Define flags
#define powerOn_MenuF (1<<0) //1
#define startGame_F (1<<1) //2
#define viewScores_F (1<<2) //4
#define endGame_F (1<<3) //8

//Variables
uint16_t EEMEM highScoreEEPROM; //EEPROM high score
uint8_t flags = 0b00000000;
volatile uint8_t ledMask = 0; //For game
volatile uint16_t currentScore = 0; //Buffer for session score

//LCD bitstream
FILE lcd_str = FDEV_SETUP_STREAM(lcd_putchar, NULL, _FDEV_SETUP_WRITE);

//Prototypes
uint16_t readHighScore();
void updateHighScore(uint16_t value);
void handleFlags();
void powerOn_Menu();
void startGame();
void game();
void endGame();

ISR(PCINT1_vect, ISR_NOBLOCK){
    switch (flags) //Analyze interrupt cause
    {
        case powerOn_MenuF:
            if(!(PINC & (1<<BUTTON_START)))
            {
                PCMSK1 = 0;
                flags = startGame_F;
            }
            if(!(PINC & (1<<BUTTON_RED)))
            {
                PCMSK1 = 0;
                flags = viewScores_F;
            }
            break;
        case startGame_F:
            while((ledMask && 0b00011110)>>1 != ((!PORTC) && 0b00001111));
            ledMask = 0;
            currentScore++;
            game();
            break;
        case viewScores_F:
            break;
        default:
            break;
    }
    handleFlags();
}

ISR(TIMER1_COMPA_vect){
    switch (flags) //Analyze interrupt cause
    {
        case powerOn_MenuF:
            if(!(PINC & (1<<BUTTON_START)))
            {
                PCMSK1 = 0;
                flags = startGame_F;
            }
            if(!(PINC & (1<<BUTTON_RED)))
            {
                PCMSK1 = 0;
                flags = viewScores_F;
            }
            break;
        case startGame_F:
            TCCR1B &= !(1<<CS12); //Timer1 detached
            ledMask = 0;
            flags = endGame_F;
            break;
        case viewScores_F:
            break;
        default:
            break;
    }
    handleFlags();
}

int main(int argc, char** argv) {
    
    TCCR0B |= (1<<CS00); //Used for random number generation seeding
    TCCR1B |= (1<<WGM12);//Timer1 on CTC mode
    OCR1A = 7812; //2s window for input during game
    
    uint16_t highScore = readHighScore(); //Stores EEPROM in SRAM
    
    //Pull-ups for all
    PORTC = 0xFF;
    
    //Set LED pins & Buzzer Pin as outputs (Initialize as off)
    PORTB &= !(1<<LED_RED) & !(1<<LED_BLUE) & !(1<<LED_YELLOW)
            & !(1<<LED_GREEN) & !(1<<LED_START);
    DDRB |= (1<<BUZZER) | (1<<LED_RED) | (1<<LED_BLUE)
            | (1<<LED_YELLOW) | (1<<LED_GREEN) | (1<<LED_START);
    
    //Interrupt from buttons:

    
    lcd_init();
    
    sei();
    //Initial State - Startup Menu
    flags = powerOn_MenuF;
    while(1){
        handleFlags();
    }
    return (EXIT_SUCCESS);
}

uint16_t readHighScore(){
    return eeprom_read_word(&highScoreEEPROM);
}

void updateHighScore(uint16_t value){
    eeprom_update_word(&highScoreEEPROM, value);
}

void handleFlags(){
    switch (flags)
    {
        case powerOn_MenuF:
            powerOn_Menu();
            break;
        case startGame_F:
            startGame();
            break;
        case viewScores_F:
            break;
        case endGame_F:
            endGame();
            break;
        default:
            break;
    }
}

void powerOn_Menu(){
    PCMSK1 |= (1<<PCINT12) | (1<<PCINT11);
    
    fprintf(&lcd_str, "\x1b\x01");
    //Blink display start controls
    cli();
    fprintf(&lcd_str, "\x1b\x80Start");
    sei();
    PORTB ^= (1<<LED_START); //On
    _delay_ms(250);
    PORTB ^= (1<<LED_START); //Off
    _delay_ms(250);
    PORTB ^= (1<<LED_START); //On
    _delay_ms(250);
    PORTB ^= (1<<LED_START); //Off
    _delay_ms(250);
    PORTB ^= (1<<LED_START); //On
    _delay_ms(250);
    PORTB ^= (1<<LED_START); //Off
    _delay_ms(250);

    //Blink display score display 
    cli();
    fprintf(&lcd_str, "\x1b\x80""Display Scores");
    sei();
    PORTB ^= (1<<LED_RED); //On
    _delay_ms(250);
    PORTB ^= (1<<LED_RED); //Off
    _delay_ms(250);
    PORTB ^= (1<<LED_RED); //On
    _delay_ms(250);
    PORTB ^= (1<<LED_RED); //Off
    _delay_ms(250);
    PORTB ^= (1<<LED_RED); //On
    _delay_ms(250);
    PORTB ^= (1<<LED_RED); //Off
    _delay_ms(250);
}

void startGame(){
    currentScore = 0;
    PORTB = 0;
    TIMSK1 |= (1<<OCIE1A); //Enable interrupting off timer
    PCMSK1 |= (1<<PCINT11) | (1<<PCINT10) | (1<<PCINT9) | (1<<PCINT8);
    
    cli();
    fprintf(&lcd_str, "\x1b\x01");
    fprintf(&lcd_str, "Starting...");
    _delay_ms(1000);
    fprintf(&lcd_str, "\x1b\x01""3");
    _delay_ms(1000);
    fprintf(&lcd_str, "\x1b\x01""2");
    _delay_ms(1000);
    fprintf(&lcd_str, "\x1b\x01""1");
    _delay_ms(1000);
    fprintf(&lcd_str, "\x1b\x01Score: ");
    sei();
    game();
}

void game(){
    cli();
    fprintf(&lcd_str, "\x1b\x87%d", currentScore);
    sei();
    uint8_t difficulty;
    uint8_t randInt; //Buffer for random number
    if(currentScore>10){
        if(currentScore>20){
            if(currentScore>30){
                difficulty = 4;
            }
            difficulty = 3;
        }
        difficulty = 2;
    }
    else{difficulty = 1;}
    while(1){
        TCNT1 = 0;
        for(uint8_t i = 0; i<difficulty; i++)
        {
            srand(TCNT0);
            randInt = rand();
            switch (randInt%4){
                case 0:
                    ledMask |= (1<<LED_RED);
                    break;
                case 1:
                    ledMask |= (1<<LED_BLUE);
                    break;
                case 2:
                    ledMask |= (1<<LED_YELLOW);
                    break;
                case 3:
                    ledMask |= (1<<LED_GREEN);
                    break;
                default:
                    break;
            }
        }
        PORTB |= ledMask; //Light up LEDs
        TCCR1B |= (1<<CS12); //Timer1 attached, Prescale by 256
        while(!(TIFR1 & (1<<OCF1A))); //While window is open
    }
}

void endGame(){
    fprintf(&lcd_str, "\x1b\x01Game Over.");
    _delay_ms(2000);
    flags = powerOn_MenuF;
}