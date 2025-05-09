#ifndef TEST_LCD_H
#define TEST_LCD_H

#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <AnimatedGIF.h>

#define TFT_CS 27
#define TFT_RST 26
#define TFT_DC 14
#define SD_CS 5

#define MAX_GIFS 20

extern Adafruit_ILI9341 tft;
extern AnimatedGIF gif;

extern String gifFiles[MAX_GIFS];
extern int totalGifs;
extern int currentGifIndex;

void initLCD();
void initSD();
void playNextGIF();

#endif
