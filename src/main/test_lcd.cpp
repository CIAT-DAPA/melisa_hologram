#include "test_lcd.h"

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
AnimatedGIF gif;

String gifFiles[MAX_GIFS];
int totalGifs = 0;
int currentGifIndex = 0;

// Buffer de trabajo para conversión (ancho máximo 320)
static uint16_t lineBuffer[320];

// Callback para dibujar el GIF
static void GIFDraw(GIFDRAW *pDraw) {
  uint8_t *pIdx = (uint8_t *)pDraw->pPixels;
  uint8_t *pPal = (uint8_t *)pDraw->pPalette;

  for (int x = 0; x < pDraw->iWidth; x++) {
    uint8_t idx = pIdx[x];
    uint8_t r = pPal[idx * 3 + 0];
    uint8_t g = pPal[idx * 3 + 1];
    uint8_t b = pPal[idx * 3 + 2];
    uint16_t c = ((r & 0xF8) << 8)
                 | ((g & 0xFC) << 3)
                 | (b >> 3);
    // swap bytes para ILI9341
    lineBuffer[x] = (c >> 8) | (c << 8);
  }

  tft.startWrite();
  tft.setAddrWindow(pDraw->iX, pDraw->iY + pDraw->y,
                    pDraw->iWidth, 1);
  tft.writePixels(lineBuffer, pDraw->iWidth);
  tft.endWrite();
}


// Callback para leer datos del GIF
static int32_t GIFRead(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  File *file = (File *)pFile->fHandle;
  return file->read(pBuf, iLen);
}

// Callback para buscar en el archivo
static int32_t GIFSeek(GIFFILE *pFile, int32_t iPos) {
  File *file = (File *)pFile->fHandle;
  return file->seek(iPos);
}

// Callback para abrir el archivo
static void *GIFOpen(const char *szFilename, int32_t *pFileSize) {
  File *file = new File(SD.open(szFilename, FILE_READ));  // Usar 'new' para evitar que el objeto se destruya
  if (*file) {
    *pFileSize = file->size();
    return (void *)file;
  }
  return nullptr;
}

// Callback para cerrar el archivo
static void GIFClose(void *pHandle) {
  File *file = (File *)pHandle;
  if (file) {
    file->close();
    delete file;  // Liberar la memoria asignada con 'new'
  }
}

void initLCD() {
  SPI.begin(18, 19, 23);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
}

void initSD() {
  if (!SD.begin(SD_CS)) {
    tft.println("Error SD!");
    delay(3000);
    return;
  }
  tft.println("SD OK!");
  delay(1000);

  File root = SD.open("/");
  while (File entry = root.openNextFile()) {
    String name = String(entry.name());
    name.toLowerCase();
    name = name;
    if (!entry.isDirectory() && name.endsWith(".gif")) {
      if (totalGifs < MAX_GIFS) {
        gifFiles[totalGifs++] = name;
      }
    }
    entry.close();
  }
  root.close();

  // Mostrar cuántos GIF encontró
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  tft.print("GIFs encontrados: ");
  tft.println(totalGifs);
  for (int i = 0; i < totalGifs; i++) {
    tft.print("  ");
    tft.println(gifFiles[i]);
    delay(200);
  }
  delay(1000);
}

void playNextGIF() {
  static unsigned long startTime = 0;
  static bool isPlaying = false;

  if (totalGifs == 0) return;

  if (!isPlaying) {
    tft.fillScreen(ILI9341_BLACK);
    String fullPath = "/" + gifFiles[currentGifIndex];
    tft.println("Abriendo:");
    tft.println(fullPath);
    delay(500);

    File testFile = SD.open(fullPath, FILE_READ);
    if (!testFile) {
      tft.println("No existe o error!");
      delay(1000);
      currentGifIndex = (currentGifIndex + 1) % totalGifs;
      return;
    }
    testFile.close();

    if (gif.open(fullPath.c_str(), GIFOpen, GIFClose, GIFRead, GIFSeek, GIFDraw)) {
      startTime = millis();
      isPlaying = true;
    } else {
      tft.println("Error open GIF");
      delay(1000);
      currentGifIndex = (currentGifIndex + 1) % totalGifs;
    }
    return;
  }

  // Si está reproduciendo, siempre llamo playFrame
  // para procesar sub-bloque tras sub-bloque.
  gif.playFrame(false, nullptr);

  // Y al mismo tiempo veo si pasaron 5s:
  if (millis() - startTime >= 5000) {
    gif.close();
    isPlaying = false;
    currentGifIndex = (currentGifIndex + 1) % totalGifs;
  }
}