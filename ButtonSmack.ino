/* 
 *  -=IMPORTANT LIBRARY INFORMATION=-
 *  MD_Parola:
 *    get from library management (https://github.com/MajicDesigns/MD_Parola)
 *  MD_MAX72XX:
 *    get from library management
 *  StateMachine:
 *    if you get it from library management, patch according to https://github.com/jrullan/StateMachine/pull/8/files/fc4ca5e9861a15853804068d87880fedaea50da8
 *    or get from the git repo from the link
 *  LinkedList (StateMachine dependency):
 *    if you get it from library management, patch according to https://github.com/ivanseidel/LinkedList/issues/33#issuecomment-585125344
 *    or get from the git repo from the link
 */

#include <avr/eeprom.h>

#include <stdio.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <StateMachine.h>

// Some additional helper functions to check free SRAM and reset the device
#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__
 int freememory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else  // __arm__
  return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}
void(* resetFunc) (void) = 0;//declare reset function at address 0


// Start program
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4 // number of 8x8 panels connected together
#define MAX_ZONES   2 // Max number of zones to use (each grouping of panels can be split into virtual zones)

#define DATA_PIN   11
#define CS_PIN     10
#define CLK_PIN    13

MD_Parola g_display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

int g_reset_led = 7;
int g_reset_button = A0;
int g_centre_button = A1; // used to clear EEPROM on startup

const int g_leds_cnt = 5;
int g_leds[g_leds_cnt] = {2,3,4,5,6};
int g_buttons[g_leds_cnt] = {A5, A4, A3, A2, A1};

unsigned long g_led_times[g_leds_cnt];
unsigned long g_led_activation_length = 2000; 

unsigned long g_activation_timestamp = 0;
unsigned long g_activation_min = 100;
unsigned long g_activation_max = 600;

int g_score = 0;

uint16_t g_high_score = 0;
uint16_t* g_high_score_address = 0;

bool g_reset_held = false;
unsigned long g_state_start_time;
const unsigned long g_game_length = 30000; //30s
const unsigned long g_pregame_length = 4000; //4s (3-1,go)

const int g_score_buffer_length = 4;
char g_score_buffer[g_score_buffer_length];
const int g_timer_buffer_length = 3;
char g_timer_buffer[g_timer_buffer_length];

StateMachine g_machine = StateMachine();
State* g_state_idle = g_machine.addState(&s_idle); // being first makes it the default state
State* g_state_pregame = g_machine.addState(&s_pregame);
State* g_state_play = g_machine.addState(&s_play);
State* g_state_end = g_machine.addState(&s_end);

/* Get a LED index that we can light up.
 *  Avoid LEDS that are already lit, as well as LEDS where the associated button is pressed.
 *  If nothing suitable can be found, return -1.
 */
int getRandomUnlitUnpressedIndex() {
  int result = -1; // result of -1 means all lights are lit
  
  size_t unlit_cnt = 0;
  int unlit_leds[g_leds_cnt];
  for (int i = 0; i < g_leds_cnt; ++i) {
    if(digitalRead(g_leds[i]) == LOW && digitalRead(g_buttons[i]) == LOW){
      unlit_leds[unlit_cnt] = i;
      ++unlit_cnt;
    }
  }
  if (unlit_cnt > 0) {
    result = unlit_leds[random(0,unlit_cnt)];
  }
  return result;
}

void lowLight() {
  for (int i = 0; i < g_leds_cnt; ++i) {
    digitalWrite(g_leds[i], LOW);
  }
}

void setHiScore(int score) {
  Serial.println(F("Writing High Score"));
  g_high_score = score;
  eeprom_write_word(g_high_score_address, g_high_score);
}

void updateDisplays() {
  // regardless if playing or not, update the display windows
  if (g_display.displayAnimate()) {
    for (uint8_t i=0; i<MAX_ZONES; i++) {
      if (g_display.getZoneStatus(i)) {
        // do something with the parameters for the animation then reset it
        g_display.displayReset(i);
      }
    }
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println(F("Setup System"));
  randomSeed(analogRead(A7));

  // setup up pin modes for buttons and LEDs
  pinMode(g_reset_led, OUTPUT);
  pinMode(g_reset_button, INPUT);  
  for (int i = 0; g_leds_cnt > i; i++) {
    pinMode(g_buttons[i], INPUT);
    pinMode(g_leds[i], OUTPUT);
  }

  // hold centre button while turning on to reset the Hi Score
  if (digitalRead(g_centre_button) == HIGH) {
    Serial.println(F("Resetting High Score"));
    setHiScore(0);
  } else {
    // load high score from EEPROM
    Serial.println(F("Loading High Score from EEPROM"));
    g_high_score = eeprom_read_word(g_high_score_address);
  }
}

void loop() {
  g_machine.run();
  updateDisplays();
  //Serial.println(freememory());
}

void s_idle() {
  if(g_machine.executeOnce){
    Serial.println(F("State Idle"));
    g_reset_held = false; // shouldn't need this, but JIC
    
    // high score display
    g_display.begin(2);
    g_display.setZone(0,0,2);
    g_display.setZone(1,3,3);
    snprintf(g_score_buffer, g_score_buffer_length, "%d", g_high_score);
    g_display.displayZoneText(1, "Hi", PA_LEFT, 0, 0, PA_PRINT);
    g_display.displayZoneText(0, g_score_buffer, PA_CENTER, 0, 0, PA_PRINT);
    lowLight();
    digitalWrite(g_reset_led, HIGH);
  }

  // only reset when releasing the button to prevent multiple calls while held down.
  if (g_reset_held == true && digitalRead(g_reset_button) == LOW) {
    Serial.println(F("Start Button triggered"));
    g_reset_held = false;
    g_machine.transitionTo(g_state_pregame);
  } else if (digitalRead(g_reset_button) == HIGH && !g_reset_held) {
    g_reset_held = true;
  }
  // TODO idle animation
}

void s_pregame() {
  unsigned long current_time = millis();
  if (g_machine.executeOnce) {
    Serial.println(F("State Pregame"));
    g_display.begin(1);
    g_display.setZone(0,0,4);

    // set the mood
    lowLight();
    digitalWrite(g_reset_led, LOW);

    g_state_start_time = current_time;
  }

  unsigned long counter = current_time - g_state_start_time;
  if ((counter + 1000) < g_pregame_length) {
    snprintf(g_timer_buffer, g_timer_buffer_length, "%d", (g_pregame_length - counter)/1000);
    g_display.displayZoneText(0, g_timer_buffer, PA_CENTER, 0, 0, PA_PRINT);
  } else if (counter < g_pregame_length) {
    snprintf(g_timer_buffer, g_timer_buffer_length, "GO");
    g_display.displayZoneText(0, g_timer_buffer, PA_CENTER, 0, 0, PA_PRINT);
  } else {
    // timer over: start the game
    g_machine.transitionTo(g_state_play);
  }
}

void s_play() {
  unsigned long current_time = millis();
  if (g_machine.executeOnce) {
    Serial.println(F("State Play"));
    g_display.begin(2);
    g_display.setZone(0,0,1);
    g_display.setZone(1,2,3);

    lowLight();
    digitalWrite(g_reset_led, LOW);
 
    g_score = 0;
    g_state_start_time = current_time;
    g_activation_timestamp = g_state_start_time + g_activation_max;
  }
  
  unsigned long counter = current_time - g_state_start_time;
  if (counter <= g_game_length) { // prevent going negative on unsigned, while also checking if the game has ended
    counter = g_game_length - counter;

    // see about lighting up a button
    if (g_activation_timestamp <= current_time) {
      int pin_light = getRandomUnlitUnpressedIndex();
      if (pin_light >= 0) {
        digitalWrite(g_leds[pin_light], HIGH);
        g_led_times[pin_light] = current_time;
        // set up next timer
        g_activation_timestamp = current_time + random(g_activation_min, g_activation_max);
      } else {
        // alter timer for faster next attempt
        g_activation_timestamp = current_time + g_activation_min;
      }
    }

    // check button presses
    for (int i = 0; i < g_leds_cnt; ++i) {
      if (digitalRead(g_buttons[i]) == HIGH && digitalRead(g_leds[i]) == HIGH) {
          digitalWrite(g_leds[i], LOW);
          g_score++;
      } else if (digitalRead(g_leds[i]) == HIGH && current_time - g_led_times[i] >= g_led_activation_length) {
        // light timed out. remove it and reduce points
        digitalWrite(g_leds[i], LOW);
        g_score--;
        if (g_score < 0) {
          g_score = 0;
        }
      }
    }

    // update timer
    snprintf(g_timer_buffer, g_timer_buffer_length, "%d", (counter/1000)+1);
    g_display.displayZoneText(1, g_timer_buffer, PA_CENTER, 0, 0, PA_PRINT);

    // update score
    snprintf(g_score_buffer, g_score_buffer_length, "%d", g_score);
    g_display.displayZoneText(0, g_score_buffer, PA_RIGHT, 0, 0, PA_PRINT);
  } else { // game timer finished
    g_machine.transitionTo(g_state_end);
  }
}

void s_end() {
  if (g_machine.executeOnce) {
    Serial.println(F("State End"));
    //check if high score needs to be saved
    if (g_score > g_high_score) {
      setHiScore(g_score);
    }
  }
  g_machine.transitionTo(g_state_idle);
  resetFunc(); //call reset 
}
