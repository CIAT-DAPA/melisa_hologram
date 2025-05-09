#include "test_lcd.h"

void setup() {
  initLCD();
  initSD();
}

void loop() {
  playNextGIF();
  delay(10);
}
