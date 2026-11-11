#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <Adafruit_GFX.h>
#include <esp_timer.h>

#include "bitmaps.h"
#include "enums.h"

#define BTNJUMP 34
#define BTNSETTINGS 35
#define LED1 18
#define LED2 2
#define SPEAKER 33

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

#define PANEL_CHAIN_LENGTH 1
#define PANEL_BRIGHTNESS 200
#define FRAME_INTERVAL_MS 20
#define JUMP_DURATION_US 500000
#define RUN_ANIMATION_INTERVAL_MS 120
#define TREX_X 5
#define TREX_Y 15
#define TREX_WIDTH 10
#define TREX_HEIGHT 10
#define JUMP_TREX_Y 5
#define OBSTACLE_GROUND_Y 25
#define GRASS_Y 28
#define OBSTACLE_COUNT 3 // total types of obstacles

const uint8_t obstacleSizes[OBSTACLE_COUNT] = {2, 3, 5}; //contains the nxn size of each obstacle type

struct ObstacleState {
  int16_t x;
  uint8_t type;
};

const uint8_t* const obstacleBitmaps[] = {
    obstacle2x2, obstacle3x3, obstacle5x5};

MatrixPanel_I2S_DMA* display = nullptr;
esp_timer_handle_t jumpTimer = nullptr;

volatile InputEvent pendingInputEvent = NONE;
volatile bool jumpTimerExpired = false;

GameState gameState = MENU;
SettingMenu settingMenu = DIFFICULTY_MENU; //subset of menu screens when in SETTINGS state
uint8_t settingIndex = 0;
ObstacleState obstacle = {64, 0};
int16_t grassX = 64;
uint8_t color_r = 255;
uint8_t color_g = 255;
uint8_t color_b = 255;
uint16_t displayColor = 0;
uint32_t lastRunAnimationMs = 0;
bool isJumping = false;
bool useLeftRunFrame = true;

void handleJumpButton();
void handleSettingsButton();
void handleInput(InputEvent inputEvent);
void initializeDisplay();
void initializeJumpTimer();
void onJumpTimer(void* arg);
void updateGame();
bool trexCollidesWithObstacle();
void drawTrex();
void playTone(uint16_t frequency);

void setup() {
  Serial.begin(115200);
  randomSeed(micros());

  pinMode(BTNJUMP, INPUT);
  pinMode(BTNSETTINGS, INPUT);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(SPEAKER, OUTPUT);
  pinMode(E_PIN, OUTPUT);
  digitalWrite(E_PIN, LOW);

  attachInterrupt(digitalPinToInterrupt(BTNJUMP),
                  handleJumpButton, FALLING);
  attachInterrupt(digitalPinToInterrupt(BTNSETTINGS),
                  handleSettingsButton, FALLING);

  ledcSetup(0, 5000, 8);
  ledcAttachPin(SPEAKER, 0);

  obstacle.x = random(RGB_PANEL_WIDTH, RGB_PANEL_WIDTH * 2);
  obstacle.type = random(0, OBSTACLE_COUNT);
  initializeDisplay();
  delay(50); 
  initializeJumpTimer();
  updateGame();
}

void loop() {
  noInterrupts();
  InputEvent inputEvent = pendingInputEvent;
  pendingInputEvent = NONE;
  interrupts();

  if (jumpTimerExpired) {
    jumpTimerExpired = false;
    isJumping = false;
  }

  handleInput(inputEvent);
  updateGame();
  delay(FRAME_INTERVAL_MS);
}

void handleInput(InputEvent inputEvent) {
  switch (inputEvent) {
    case JUMP_PRESSED:
      switch (gameState) {
        case MENU:
          gameState = PLAYING;
          playTone(TONESELECT);
          Serial.println("Start playing");
          break;
        case PLAYING:
          if (!isJumping && jumpTimer != nullptr) {
            isJumping = true;
            jumpTimerExpired = false;
            esp_err_t result = esp_timer_start_once(
                jumpTimer, JUMP_DURATION_US);
            if (result != ESP_OK) {
              Serial.printf("Failed to start jump timer: %s\n",
                            esp_err_to_name(result));
              isJumping = false;
            } else {
              playTone(TONEJUMP);
              Serial.println("Jump pressed");
            }
          }
          break;
        case SETTINGS:
          gameState = MENU;
          playTone(TONESELECT);
          Serial.println("Select setting");
          break;
      }
      break;

    case SETTINGS_PRESSED:
      switch (gameState) {
        case MENU:
          gameState = SETTINGS;
          playTone(TONESELECT);
          Serial.println("Settings pressed");
          break;
        case SETTINGS:
          if (settingMenu == DIFFICULTY_MENU) {
            Serial.println("Cycle setting: " + String(settingIndex));
          }
          ++settingIndex;
          playTone(TONESELECT);
          break;
      }
      break;

    case NONE:
      break;
  }
}

void handleJumpButton() {
  pendingInputEvent = JUMP_PRESSED;
}

void handleSettingsButton() {
  pendingInputEvent = SETTINGS_PRESSED;
}

void onJumpTimer(void*) {
  jumpTimerExpired = true;
}

void initializeDisplay() {
  HUB75_I2S_CFG::i2s_pins panelPins = {
      R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN,
      A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
  HUB75_I2S_CFG panelConfig(RGB_PANEL_WIDTH, RGB_PANEL_HEIGHT,
                            PANEL_CHAIN_LENGTH, panelPins);

  display = new MatrixPanel_I2S_DMA(panelConfig);
  if (!display->begin()) {
    Serial.println("MatrixPanel_I2S_DMA initialization failed");
    return;
  }

  display->clearScreen();
  display->setBrightness8(PANEL_BRIGHTNESS);
  display->setLatBlanking(2);
  displayColor = display->color565(color_r, color_g, color_b);
}

void initializeJumpTimer() {
  esp_timer_create_args_t timerArgs = {};
  timerArgs.callback = onJumpTimer;
  timerArgs.arg = nullptr;
  timerArgs.dispatch_method = ESP_TIMER_TASK;
  timerArgs.name = "jump_timer";

  esp_err_t result = esp_timer_create(&timerArgs, &jumpTimer);
  if (result != ESP_OK) {
    Serial.printf("Failed to create jump timer: %s\n", esp_err_to_name(result));
    jumpTimer = nullptr;
  }
}

void updateGame() {
  display->clearScreen();
  display->drawFastHLine(0, RGB_PANEL_HEIGHT - 1, RGB_PANEL_WIDTH, displayColor);
  display->drawFastHLine(0, OBSTACLE_GROUND_Y,
                         RGB_PANEL_WIDTH, displayColor);

  if (gameState == PLAYING) {
    
    const int16_t obstacleSize = obstacleSizes[obstacle.type];
    if (--obstacle.x < -obstacleSize) {
      obstacle.x = random(RGB_PANEL_WIDTH, RGB_PANEL_WIDTH * 2);
      obstacle.type = random(0, OBSTACLE_COUNT);
    }
    if (--grassX < -3) {
      grassX = RGB_PANEL_WIDTH;
    }

    const uint32_t now = millis();
    if (!isJumping &&
        now - lastRunAnimationMs >= RUN_ANIMATION_INTERVAL_MS) {
      useLeftRunFrame = !useLeftRunFrame;
      lastRunAnimationMs = now;
    }

    if (trexCollidesWithObstacle()) {
      gameState = MENU;
      obstacle.x = random(RGB_PANEL_WIDTH, RGB_PANEL_WIDTH * 2);
      obstacle.type = random(0, OBSTACLE_COUNT);
      playTone(TONEDIE);
      Serial.println("Died");
    }
  }

  drawTrex();
  if (gameState == PLAYING) {
    display->drawBitmap(grassX, GRASS_Y, grass2x3, 3, 2, displayColor);
    const uint8_t obstacleSize = obstacleSizes[obstacle.type];
    display->drawBitmap(obstacle.x, OBSTACLE_GROUND_Y - obstacleSize,
                        obstacleBitmaps[obstacle.type], obstacleSize,
                        obstacleSize, displayColor);
  }
}

bool trexCollidesWithObstacle() {
  const int16_t trexLeft = TREX_X;
  const int16_t trexRight = trexLeft + TREX_WIDTH;

  const int16_t obstacleRight = obstacle.x + obstacleSizes[obstacle.type];
  const int16_t obstacleLeft = obstacle.x;

  return !isJumping && !(trexRight < obstacleLeft || trexLeft > obstacleRight);
}

void drawTrex() {
  if (gameState != PLAYING) {
    display->drawBitmap(TREX_X, TREX_Y,
                        trex10x10_jump, TREX_WIDTH,
                        TREX_HEIGHT, displayColor);
  } else if (isJumping) {
    display->drawBitmap(TREX_X, JUMP_TREX_Y,
                        trex10x10_jump, TREX_WIDTH,
                        TREX_HEIGHT, displayColor);
  } else {
    const uint8_t* runFrame =
        useLeftRunFrame ? trex10x10_left : trex10x10_right;
    display->drawBitmap(TREX_X, TREX_Y, runFrame,
                        TREX_WIDTH, TREX_HEIGHT, displayColor);
  }
}

void playTone(uint16_t frequency) {
  tone(SPEAKER, frequency, 100);
}
