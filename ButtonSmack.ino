// IMPORTANT!
// DOWNLOAD the MD_Parola library
// https://github.com/MajicDesigns/MD_Parola


#include <stdio.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
//#include "Parola_Fonts_data.h"
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4 // number of 8x8 panels connected together
#define MAX_ZONES   1 // Max number of zones to use (each grouping of panels can be split into virtual zones)
#define CLK_PIN    13
#define DATA_PIN   11
#define CS_PIN     10
MD_Parola Display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

int step_counter = 0;

int reset_led = 7;
int reset_button = A5;

const int leds_cnt = 5;
int p1_leds[leds_cnt] = {2,3,4,5,6};
int p1_buttons[leds_cnt] = {A0, A1, A2, A3, A4};

int action_speed = 2000;
int action_speed_min = 250;

bool playing = false;
int p1_score = 0;

void setup() {
  Serial.begin(9600);

  randomSeed(analogRead(A7));

  pinMode(reset_led, OUTPUT);
  pinMode(reset_button, INPUT);  
  for (int i = 0; leds_cnt > i; i++) {
    pinMode(p1_buttons[i], INPUT);
    pinMode(p1_leds[i], OUTPUT);
  }

  digitalWrite(reset_led, HIGH);

  Display.begin(MAX_ZONES);
  Display.setZone(0,0,3);
  Display.displayZoneText(0, "Hi Score", PA_CENTER, 100, 100, PA_SCROLL_LEFT, PA_SCROLL_LEFT);

  resetGame();
}

void resetGame() {
  endGame();
  startGame();
}

void startGame() {
  playing = true;
  p1_score = 0;
  for (int i = 0; i < leds_cnt; ++i) {
    digitalWrite(p1_leds[i], LOW);
  }
  step_counter = 0;
}

void endGame() {
  playing = false;
  
}

void loop() {
  if (digitalRead(reset_button) == HIGH) {
    resetGame();
  }
  if(playing) {
    if(p1_score < 100) {
      step_counter++;
      bool step_action = false;
      if (step_counter > action_speed) {
        step_counter = 0;
        step_action = true;
        action_speed = action_speed - round(action_speed/50);
        if (action_speed < action_speed_min) {
          action_speed = action_speed_min;
        }
      }

      if (step_action) {
        int pin_light = random(0,5);
        digitalWrite(p1_leds[pin_light], HIGH);
  
      }

      for (int i = 0; i < leds_cnt; ++i) {
        if (digitalRead(p1_buttons[i]) == HIGH) {
          if(digitalRead(p1_leds[i]) == HIGH){
            digitalWrite(p1_leds[i], LOW);
            p1_score++;
          } 
        }
      }

      if ( step_counter % 100 == 0){
        char Score1[80];
        sprintf(Score1, "%d", p1_score);
        char *chrdisp[] = {Score1};
  
        Display.displayZoneText(0, chrdisp[0], PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
      }
    } else {
      char Score1[80];
      sprintf(Score1, "%d", p1_score);
      char *chrdisp[] = {Score1};
      Display.displayZoneText(0, chrdisp[0], PA_CENTER, 100, 100, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
    }
  } else {
    //not playing
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
