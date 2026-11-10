#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Adafruit_GFX.h>
#include <esp_timer.h>
#include "enums.h"
#include "bitmaps.h"

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

volatile InputEvent pendingInputEvent = NONE;

GameState currentGameState = MENU;

const uint8_t speedMultipliers[3] = {1, 2, 3};
Difficulty currentDifficulty = EASY;
uint8_t currentMultiplier = speedMultipliers[currentDifficulty];

SettingMenu currentSettingMenu = DIFFICULTY_MENU; // TODO: doesnt change for now
uint8_t currentSettingIndex = 0; // option index within the current setting menu

uint8_t frameTimeMs = 10/currentMultiplier; 

uint8_t color_r = 255;
uint8_t color_g = 255;
uint8_t color_b = 255;
uint16_t color = 0;

constexpr uint64_t JUMP_DURATION_US = 500000;
esp_timer_handle_t jumpTimer = nullptr;
volatile bool jumpTimerExpired = false;
bool isJumping = false;
bool stepCount = true;
uint32_t lastStepTimeMs = 0;

void handleJump();
void handleSettings();
void onJumpTimer(void* arg);
void drawTrex(bool jump = false);
void updateFrame();

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
  dma_display->setBrightness8(200);
  color = dma_display->color565(color_r, color_g, color_b);

  esp_timer_create_args_t jumpTimerArgs = {};
  jumpTimerArgs.callback = onJumpTimer;
  jumpTimerArgs.arg = nullptr;
  jumpTimerArgs.dispatch_method = ESP_TIMER_TASK;
  jumpTimerArgs.name = "jump_timer";
  esp_err_t timerResult = esp_timer_create(&jumpTimerArgs, &jumpTimer);
  if (timerResult != ESP_OK) {
    Serial.printf("Failed to create jump timer: %s\n", esp_err_to_name(timerResult));
    jumpTimer = nullptr;
  }

  updateFrame();
}

void loop() {
  if (jumpTimerExpired) {
    jumpTimerExpired = false;
    isJumping = false;
  }

  noInterrupts();
  InputEvent inputEvent = pendingInputEvent;
  pendingInputEvent = NONE;
  interrupts();

  switch (inputEvent) {
    case JUMP_PRESSED:
      switch (currentGameState) {
        case MENU: // start playing
          currentGameState = PLAYING;
          // TODO: initPlaying();
          tone(SPEAKER, TONESELECT, 100);
          Serial.println("Start playing");
          break;
        case PLAYING: // jump
          if (!isJumping) { // if not already jumping, start jump timer
            isJumping = true;
            jumpTimerExpired = false;
            esp_err_t timerResult = esp_timer_start_once(jumpTimer, JUMP_DURATION_US);
            if (timerResult != ESP_OK) {
              Serial.printf("Failed to start jump timer: %s\n", esp_err_to_name(timerResult));
              isJumping = false;
            } else {
              tone(SPEAKER, TONEJUMP, 100);
              Serial.println("Jump pressed");
            }
          }
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

    case NONE:
      updateFrame();

      break;
  }

  delay(frameTimeMs);
}

void handleJump() {
  pendingInputEvent = JUMP_PRESSED;
}

void handleSettings() {
  pendingInputEvent = SETTINGS_PRESSED;
}

void onJumpTimer(void*) {
  jumpTimerExpired = true;
}

void drawTrex(bool jump) {
  if (jump) {
    dma_display->drawBitmap(5, 5, trex10x10_jump, 10, 10, color);
  } else {
    if(stepCount) {
      dma_display->drawBitmap(5, 15, trex10x10_left, 10, 10, color);
    } else {
      dma_display->drawBitmap(5, 15, trex10x10_right, 10, 10, color);
    }
  }
}

void updateFrame() {
  dma_display->clearScreen();
  uint32_t now = millis();
  if (!isJumping && now - lastStepTimeMs >= 120 && currentGameState == PLAYING) {
    stepCount = !stepCount;
    lastStepTimeMs = now;
  }
  drawTrex(isJumping);
}
  