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

#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13
#define A_PIN 23
#define B_PIN 19
#define C_PIN 5
#define D_PIN 17
#define E_PIN 32
#define LAT_PIN 4
#define OE_PIN 15
#define CLK_PIN 16

MatrixPanel_I2S_DMA *dma_display = nullptr;

volatile Flag currentFlag = NONE;

GameState currentGameState = MENU;

const uint8_t speedMultipliers[3] = {1, 2, 3};
Difficulty currentDifficulty = EASY;
uint8_t currentMultiplier = speedMultipliers[currentDifficulty];

SettingMenu currentSettingMenu = DIFFICULTY_MENU; // TODO: doesnt change for now
uint8_t currentSettingIndex = 0; // option index within the current setting menu

uint8_t frameTimeMs = 10/currentMultiplier; 

bool jumpPressed = false;

void handleJump();
void handleSettings();

void setup() {
  Serial.begin(115200);
  pinMode(BTNJUMP, INPUT);
  pinMode(BTNSETTINGS, INPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(SPEAKER, OUTPUT);
  pinMode(32, OUTPUT);
  digitalWrite(32, LOW);

  attachInterrupt(digitalPinToInterrupt(BTNJUMP),handleJump, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTNSETTINGS),handleSettings, FALLING);

  ledcSetup(0, 5000, 8);
  ledcAttachPin(SPEAKER, 0);

  HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG mxconfig(
    RGB_PANEL_WIDTH, // Module width
    RGB_PANEL_HEIGHT, // Module height
    1, 
    _pins
  );
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_display->begin()) {
    Serial.println("MatrixPanel_I2S_DMA initialization failed");
  }
  dma_display->clearScreen();
  dma_display->setBrightness8(255); // Set brightness to maximum
  dma_display->drawPixel(0, 0, dma_display->color565(255, 255, 255)); // Draw a white dot at top-left
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
            case DIFFICULTY_MENU:
              // TODO: cycleSetting();
              Serial.println("Cycle setting: " + String(currentSettingIndex));
              break;
          }
          currentSettingIndex++;
          tone(SPEAKER, TONESELECT, 100);
          break;
      }
      break;

    case NONE: // update frame

      break;
  }

  delay(frameTimeMs);
}

void handleJump() {
  currentFlag = JUMP_PRESSED;
}

void handleSettings() {
  currentFlag = SETTINGS_PRESSED;
}
