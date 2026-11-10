#ifndef ENUMS_H
#define ENUMS_H

enum Flag {
  JUMP_PRESSED,
  SETTINGS_PRESSED,
  NONE
};

enum GameState {
  MENU,
  PLAYING,
  SETTINGS
};

enum SettingMenu {
  DIFFICULTY_MENU
};

enum Difficulty {
  EASY,
  MEDIUM,
  HARD
};

#endif