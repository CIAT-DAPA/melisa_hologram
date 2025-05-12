#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <PNGdec.h>

#define TFT_CS 27
#define TFT_RST 26
#define TFT_DC 14
#define SD_CS 5

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
PNG png;

const uint16_t IMG_W = 256;
const uint16_t IMG_H = 256;
const uint16_t SCREEN_W = 320;
const uint16_t SCREEN_H = 240;
const int16_t X_OFFSET = (SCREEN_W - IMG_W) / 2;
const int16_t Y_OFFSET = (SCREEN_H - IMG_H) / 2;

uint16_t lineBuffer[IMG_W];

static unsigned long lastFrameTime = 0;
static unsigned long lastFolderSwitch = 0;
const unsigned long FOLDER_INTERVAL = 6000;  // ms per folder
static uint8_t currentFolderIndex = 0;

// PNGdec callbacks
static void* pngOpen(const char* filename, int32_t* size) {
  File* f = new File(SD.open(filename, FILE_READ));
  if (f && *f) {
    *size = f->size();
    return f;
  }
  delete f;
  return nullptr;
}
static void pngClose(void* handle) {
  File* f = (File*)handle;
  if (f) {
    f->close();
    delete f;
  }
}
static int32_t pngRead(PNGFILE* pf, uint8_t* buf, int32_t len) {
  return ((File*)pf->fHandle)->read(buf, len);
}
static int32_t pngSeek(PNGFILE* pf, int32_t pos) {
  return ((File*)pf->fHandle)->seek(pos);
}
static void pngDraw(PNGDRAW* pDraw) {
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0x000000);
  tft.drawRGBBitmap(X_OFFSET, Y_OFFSET + pDraw->y, lineBuffer, pDraw->iWidth, 1);
}

void playAnimation(const char* folder, uint16_t maxFrames, const uint16_t* frameDelays) {
  static uint16_t frame = 1;
  unsigned long now = millis();

  if (now - lastFrameTime >= frameDelays[frame - 1]) {
    lastFrameTime = now;
    char path[32];
    snprintf(path, sizeof(path), "/%s/%u.png", folder, frame);

    int16_t rc = png.open(path, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
    if (rc == PNG_SUCCESS) {
      png.decode(NULL, 0);
      png.close();
    } else {
      Serial.printf("Error decoding %s: %d\n", path, rc);
    }

    frame++;
    if (frame > maxFrames) {
      frame = 1;
    }
  }
}

// Animation configurations
const uint16_t idleDelays[] = { 100, 100, 100, 100, 100, 100, 200, 200, 1500 };
const uint16_t talkDelays[] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 };
const uint16_t thinkDelays[] = { 100, 100, 100, 300, 300, 300, 200, 200, 1000 };

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  if (!SD.begin(SD_CS)) {
    tft.setCursor(10, 10);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_RED);
    tft.println("SD ERROR");
    while (true)
      ;
  }
  lastFrameTime = millis();
  lastFolderSwitch = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastFolderSwitch >= FOLDER_INTERVAL) {
    currentFolderIndex = (currentFolderIndex + 1) % 3;
    lastFolderSwitch = now;
    lastFrameTime = now;
    Serial.printf("Switched to animation %d\n", currentFolderIndex);
  }

  switch (currentFolderIndex) {
    case 0: playAnimation("idle", 9, idleDelays); break;
    case 1: playAnimation("talk", 10, talkDelays); break;
    case 2: playAnimation("think", 9, thinkDelays); break;
  }
}
