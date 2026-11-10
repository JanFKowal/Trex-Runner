#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Adafruit_GFX.h>
#include "enums.h"

#define BTNJUMP 34
#define BTNSETTINGS 35
#define LED1 18
#define LED2 2
#define SPEAKER 33
#define SDA 21
#define SCL 22

#define TONEJUMP 500
#define TONESELECT 750
#define TONEDIE 1000

#define PANEL_RES_X 64
#define PANEL_RES_Y 32

const uint8_t multipliers[3] = {1, 2, 3};

volatile Flag currentFlag = NONE;

GameState currentGameState = MENU;

SettingMenu currentSettingMenu = DIFFICULTY; // TODO: doesnt change for now
uint8_t currentSettingIndex = 0; // option index within the current setting menu

Difficulty currentDifficulty = EASY;
uint8_t currentMultiplier = multipliers[currentDifficulty];

bool jumpPressed = false;

void handleJump();
void handleSettings();
void temporaryDeath();

void setup() {
  Serial.begin(115200);
  pinMode(BTNJUMP, INPUT);
  pinMode(BTNSETTINGS, INPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(SPEAKER, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(BTNJUMP),handleJump, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTNSETTINGS),handleSettings, FALLING);

  ledcSetup(0, 5000, 8);
  ledcAttachPin(SPEAKER, 0);
}

void loop() {
  noInterrupts();
  Flag flag = currentFlag;
  currentFlag = NONE;
  interrupts();

  switch (flag) {
    case JUMP_PRESSED:
      switch (currentGameState) {
        case MENU: // start playing
          currentGameState = PLAYING;
          // TODO: initPlaying();
          tone(SPEAKER, TONESELECT, 100);
          Serial.println("Start playing");
          break;
        case PLAYING: // jump
          jumpPressed = true;
          tone(SPEAKER, TONEJUMP, 100);
          Serial.println("Jump pressed");
          break;
        case SETTINGS: // select setting and go back to menu
          currentGameState = MENU;
          // TODO: selectSetting();
          tone(SPEAKER, TONESELECT, 100);
          Serial.println("Select setting");
          break;
      }
      break;

    case SETTINGS_PRESSED:
      switch (currentGameState) {
        case PLAYING:
          // TODO: change later to do nothing or pause the game instead of dying
          currentGameState = MENU;
          tone(SPEAKER, TONEDIE, 100);
          Serial.println("Died");
          break;
        case MENU: // go to settings
          currentGameState = SETTINGS;
          // TODO: initSettings();
          tone(SPEAKER, TONESELECT, 100);
          Serial.println("Settings pressed");
          break;
        case SETTINGS: // cycle setting options
          switch (currentSettingMenu) {
            case DIFFICULTY:
              // TODO: cycleSetting();
              Serial.println("Cycle setting: " + String(currentSettingIndex));
              break;
          }
          currentSettingIndex++;
          tone(SPEAKER, TONESELECT, 100);
          break;
      }
      break;

    case NONE:
      break;
  }

  delay(100);
}

void handleJump() {
  currentFlag = JUMP_PRESSED;
}

void handleSettings() {
  currentFlag = SETTINGS_PRESSED;
}
