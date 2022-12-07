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
#define LED_GREEN PB1
#define LED_YELLOW PB2
#define LED_BLUE PB3
#define LED_RED PB4
#define LED_START PB5
#define BUTTON_GREEN PC0 //PCINT8
#define BUTTON_YELLOW PC1 //PCINT9
#define BUTTON_BLUE PC2 //PCINT10
#define BUTTON_RED PC3 //PCINT11
#define BUTTON_START PC4 //PCINT12

//Define flags
#define powerOn_MenuF (1<<0) //1
#define startGame_F (1<<1) //2
#define viewScores_F (1<<2) //4
#define game_F (1<<3) //8
#define endGame_F (1<<4) //16
#define correct_F (1<<5) //32

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
void viewScores();
void endGame();
void ring();

ISR(PCINT1_vect){
    switch (flags) //Analyze interrupt cause
    {
        case powerOn_MenuF:
            if(!(PINC & (1<<BUTTON_START)))
            {
                PORTB = 0;
                PCMSK1 = 0;
                flags = startGame_F;
                break;
            }
            if(!(PINC & (1<<BUTTON_RED)))
            {
                PORTB = 0;
                PCMSK1 = 0;
                flags = viewScores_F;
                break;
            }
            break;
        case game_F:
            //Clear stuff
            TCCR1B &= ~(1<<CS12); //Timer1 detached
            TIMSK1 &= ~(1<<OCIE1A); //Disable interrupting off timer
            PCMSK1 = 0; //Disable pin interrupts
            PORTB = 0; //Turn off LEDs
            ledMask = 0;
            flags = endGame_F;
            break;
        case correct_F:
            break;
        case viewScores_F:
            PORTB = 0;
            flags = powerOn_MenuF;
            break;
        default:
            break;
    }
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
        case game_F:
            //Clear stuff
            PCMSK1 = 0; //Disable pin interrupts
            PORTB = 0; //Turn off LEDs
            //Check correctness
            flags = endGame_F;
            break;
        case correct_F:
            PCMSK1 = 0; //Disable pin interrupts
            PORTB = 0;
            break;
        case viewScores_F:
            break;
        default:
            break;
    }
}

int main(int argc, char** argv) {
    
    //Timer0, Timer1 setup
    TCCR0B |= (1<<CS00); //Used for random number generation seeding
    TCCR1B |= (1<<WGM12);//Timer1 on CTC mode
    OCR1A = 7812; //2s window for input during game
    
    //Pull-ups for all buttons
    PORTC = 0b00111111; //Pins 6,7 DNE
    
    //Set LED pins & Buzzer Pin as outputs (Initialize as off)
    PORTB &= !(1<<LED_RED) & !(1<<LED_BLUE) & !(1<<LED_YELLOW)
            & !(1<<LED_GREEN) & !(1<<LED_START);
    DDRB |= (1<<BUZZER) | (1<<LED_RED) | (1<<LED_BLUE)
            | (1<<LED_YELLOW) | (1<<LED_GREEN) | (1<<LED_START);
    
    //Interrupt from buttons:
    PCICR |= (1 << PCIE1);
    
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
            viewScores();
            break;
        case game_F:
            game();
            break;
        case endGame_F:
            endGame();
            break;
        case correct_F:
            game();
            break;
        default:
            flags = powerOn_MenuF;
            break;
    }
}

void powerOn_Menu(){
    PCMSK1 |= (1<<PCINT12) | (1<<PCINT11);
    PORTB |= (1<<LED_START) | (1<<LED_RED);
    cli();
    fprintf(&lcd_str, "\x1b\x80White: Start\x1b\xC0Red: Scoreboard");
    sei();
}

void startGame(){
    currentScore = 0;
    PORTB = 0;
    
    
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
    TCCR1B |= (1<<CS12); //Timer1 attached, prescale by 256
    TIMSK1 |= (1<<OCIE1A); //Enable interrupting off timer
    flags = game_F;
}

void game(){
    flags = game_F;
    sei();
    //Initialize stuff
    fprintf(&lcd_str, "\x1b\x87%d", currentScore);
    uint8_t difficulty;
    uint8_t randInt; //Buffer for random number
    if(currentScore>10){
        difficulty = 2;
        if(currentScore>20){
            difficulty = 3;
            if(currentScore>30){
                difficulty = 4;
            }
        }
    }
    else{difficulty = 1;}
    
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
    TCNT1 = 0x00;
    //Logic & doing things
    PORTB |= ledMask; //Light up LEDs
    
    PCMSK1 = (~ledMask)>>1 & 0b00001111; //If wrong button pressed
    while( ((ledMask>>1) != ((~PINC) & 0b00001111)) && (flags != endGame_F) )
    {
        sei();
    };
    PCMSK1 = 0;
    PORTB = 0;
    ledMask = 0; //If not end, correct answer
    if(flags != endGame_F){
        currentScore++;
        fprintf(&lcd_str, "\x1b\x87%d", currentScore);
        ring();
        flags = correct_F;
        _delay_ms(1000);
    }
}

void endGame(){
    PORTB = 0;
    TCCR1B &= ~(1<<CS12); //Timer1 detached
    TIMSK1 &= ~(1<<OCIE1A); //Enable interrupting off timer
    uint16_t tempHighScore = readHighScore();
    if(currentScore>tempHighScore)
    {
        updateHighScore(currentScore);
        tempHighScore = readHighScore();
    }
    fprintf(&lcd_str, "\x1b\x01Game Over.");
    _delay_ms(2000);
    fprintf(&lcd_str, "\x1b\x01Score: %d\x1b\xC0High Score: %d", 
            currentScore, tempHighScore);
    _delay_ms(4000);
    flags = powerOn_MenuF;
}

void viewScores(){
    PCMSK1 |= (1<<PCINT8);
    uint16_t tempHighScore = readHighScore();
    fprintf(&lcd_str, "\x1b\x80Session: %-6d \x1b\xC0HighScore: %-5d", currentScore, tempHighScore);
    PORTB |= (1<<LED_GREEN);
}

void ring(){
    for(int i = 0; i<20; i++)
    {
       PORTB ^= (1<<PB0);
        _delay_ms(10);
    }
}