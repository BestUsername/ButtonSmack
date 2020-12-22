// IMPORTANT!
// DOWNLOAD the MD_Parola library
// https://github.com/MajicDesigns/MD_Parola
#include <avr/eeprom.h>

#include <stdio.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
//#include "Parola_Fonts_data.h"
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4 // number of 8x8 panels connected together
#define MAX_ZONES   2 // Max number of zones to use (each grouping of panels can be split into virtual zones)

#define DATA_PIN   11
#define CS_PIN     10
#define CLK_PIN    13

MD_Parola Display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

int reset_led = 7;
int reset_button = A0;
int centre_button = A1; // used to clear EEPROM on startup

const int leds_cnt = 5;
int p1_leds[leds_cnt] = {2,3,4,5,6};
int p1_buttons[leds_cnt] = {A5, A4, A3, A2, A1};

unsigned long p1_led_times[leds_cnt];
unsigned long led_activation_length = 2000; 

unsigned long activation_timestamp = 0;
unsigned long activation_min = 100;
unsigned long activation_max = 600;

bool playing = false;
int p1_score = 0;

uint16_t high_score = 0;
uint16_t* high_score_address = 0;

bool resetHeld = false;
unsigned long gameStartTime;
const unsigned long gameLength = 30000;

const int scoreBufferLength = 4;
char scoreBuffer[scoreBufferLength];
const int timerBufferLength = 3;
char timerBuffer[timerBufferLength];

void setup() {
  Serial.begin(9600);
  Serial.println("Setup System");

  randomSeed(analogRead(A7));

  // setup up pin modes for buttons and LEDs
  pinMode(reset_led, OUTPUT);
  pinMode(reset_button, INPUT);  
  for (int i = 0; leds_cnt > i; i++) {
    pinMode(p1_buttons[i], INPUT);
    pinMode(p1_leds[i], OUTPUT);
  }

  // hold centre button while turning on to reset the Hi Score
  if (digitalRead(centre_button) == HIGH) {
    Serial.println("CENTRE BUTTON HIGH");
    setHiScore(0);
  } else {
    Serial.println("CENTRE BUTTON LOW");
    // load high score from EEPROM
    high_score = eeprom_read_word(high_score_address);
  }

  // starting state
  idle();
}

/* Get a LED index that we can light up.
 *  Avoid LEDS that are already lit, as well as LEDS where the associated button is pressed.
 *  If nothing suitable can be found, return -1.
 */
int getRandomUnlitUnpressedIndex() {
  int result = -1; // result of -1 means all lights are lit
  
  size_t unlit_cnt = 0;
  int unlit_leds[leds_cnt];
  for (int i = 0; i < leds_cnt; ++i) {
    if(digitalRead(p1_leds[i]) == LOW && digitalRead(p1_buttons[i]) == LOW){
      unlit_leds[unlit_cnt] = i;
      ++unlit_cnt;
    }
  }
  if (unlit_cnt > 0) {
    result = random(0,unlit_cnt-1);
  }
  return result;
}

void idle() {
  playing = false;
  // high score display
  Display.begin(MAX_ZONES);
  Display.setZone(0,0,2);
  Display.setZone(1,3,3);
  snprintf(scoreBuffer, scoreBufferLength, "%d", high_score);
  Display.displayZoneText(1, "Hi", PA_LEFT, 0, 0, PA_PRINT);
  Display.displayZoneText(0, scoreBuffer, PA_CENTER, 0, 0, PA_PRINT);
  lowLight();
  digitalWrite(reset_led, HIGH);
}

void lowLight() {
  for (int i = 0; i < leds_cnt; ++i) {
    digitalWrite(p1_leds[i], LOW);
  }
}

void startGame() {
  Serial.println("Start State");

  Display.begin(2);
  Display.setZone(0,0,1);
  Display.setZone(1,2,3);
  
  lowLight();
  digitalWrite(reset_led, LOW);
 
  p1_score = 0;
  playing = true;
  gameStartTime = millis();
  activation_timestamp = gameStartTime + activation_max;
}

void endGame() {
  Serial.println("End Game");
  playing = false;

  if (p1_score > high_score) {
    setHiScore(p1_score);
  } else {
    
  }
  idle();
}

void setHiScore(int score) {
  Serial.println("Writing High Score");
  high_score = score;
  eeprom_write_word(high_score_address, high_score);
}

void loop() {
  unsigned long currentTime = millis();
  // main game logic
  if(playing) {
    // update timer
    unsigned long counter = currentTime - gameStartTime;
    if (counter <= gameLength) { // prevent going negative on unsigned, while also checking if the game has ended
      counter = gameLength - counter;

      // see about lighting up a button
      if (activation_timestamp <= currentTime) {
        int pin_light = getRandomUnlitUnpressedIndex();
        if (pin_light >= 0) {
          digitalWrite(p1_leds[pin_light], HIGH);
          p1_led_times[pin_light] = currentTime;
          // set up next timer
          activation_timestamp = currentTime + random(activation_min, activation_max);
        } else {
          // alter timer for faster next attempt
          activation_timestamp = currentTime + activation_min;
        }
      }

      // check button presses
      for (int i = 0; i < leds_cnt; ++i) {
        if (digitalRead(p1_buttons[i]) == HIGH && digitalRead(p1_leds[i]) == HIGH) {
            digitalWrite(p1_leds[i], LOW);
            p1_score++;
        } else if (digitalRead(p1_leds[i]) == HIGH && currentTime - p1_led_times[i] >= led_activation_length) {
          // light timed out. remove it and reduce points
          digitalWrite(p1_leds[i], LOW);
          p1_score--;
          if (p1_score < 0) {
            p1_score = 0;
          }
        }
      }

      // update displays
        // timer
        snprintf(timerBuffer, timerBufferLength, "%d", counter/1000);
        Display.displayZoneText(1, timerBuffer, PA_CENTER, 0, 0, PA_PRINT);
  
        // score
        snprintf(scoreBuffer, scoreBufferLength, "%d", p1_score);
        Display.displayZoneText(0, scoreBuffer, PA_RIGHT, 0, 0, PA_PRINT);
    } else { // game timer finished
      endGame();
    }
  } else {
    // only reset when releasing the button to prevent multiple calls while held down.
    if (resetHeld == true && digitalRead(reset_button) == LOW) {
      resetHeld = false;
      startGame();
    } else if (digitalRead(reset_button) == HIGH && !resetHeld) {
      resetHeld = true;
    }
    // TODO idle animation
  }

  // regardless if playing or not, update the display windows
  if (Display.displayAnimate()) {
    for (uint8_t i=0; i<MAX_ZONES; i++) {
      if (Display.getZoneStatus(i)) {
        // do something with the parameters for the animation then reset it
        Display.displayReset(i);
      }
    }
  }
}
