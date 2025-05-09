#include "test_lcd.h"

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
AnimatedGIF gif;

String gifFiles[MAX_GIFS];
int totalGifs = 0;
int currentGifIndex = 0;

// Callback para dibujar el GIF
static void GIFDraw(GIFDRAW *pDraw) {
  uint16_t *pixels = (uint16_t *)pDraw->pPixels;
  uint8_t *pPalette = (uint8_t *)pDraw->pPalette;

  // Convertir RGB888 a RGB565
  for (int i = 0; i < pDraw->iWidth; i++) {
    uint8_t r = pPalette[(pDraw->pPixels[i] * 3) + 0];
    uint8_t g = pPalette[(pDraw->pPixels[i] * 3) + 1];
    uint8_t b = pPalette[(pDraw->pPixels[i] * 3) + 2];
    pixels[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }

  // Dibujar en la pantalla
  tft.startWrite();
  tft.setAddrWindow(pDraw->iX, pDraw->iY + pDraw->y, pDraw->iWidth, 1);
  tft.writePixels(pixels, pDraw->iWidth);
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
  SPI.begin(18, 19, 23, TFT_CS);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
}

void initSD() {
  if (!SD.begin(SD_CS, SPI)) {
    tft.println("Error SD!");
    delay(3000);
    return;
  }
  tft.println("SD OK!");
  delay(1000);

  File root = SD.open("/");
  while (File entry = root.openNextFile()) {
    String name = String(entry.name());
    name.toLowerCase();  // Convierte el nombre a minúsculas en sitio

    // Ahora endsWith funciona porque name es un String ya modificado
    if (!entry.isDirectory() && name.endsWith(".gif")) {
      if (totalGifs < MAX_GIFS) {
        gifFiles[totalGifs++] = String(entry.name());
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

  if (totalGifs == 0) {
    // No hay GIFs, nada que hacer
    return;
  }

  if (!isPlaying) {
    tft.fillScreen(ILI9341_BLACK);
    const char* filename = gifFiles[currentGifIndex].c_str();

    tft.println("Abriendo:");
    tft.println(filename);
    delay(500);

    // Verificamos existencia intentando abrir
    File testFile = SD.open(filename, FILE_READ);
    if (!testFile) {
      tft.println("No existe o error!");
      delay(1000);
      currentGifIndex = (currentGifIndex + 1) % totalGifs;
      return;
    }
    testFile.close();

    // Ahora abrimos para AnimatedGIF
    if (gif.open(filename, GIFOpen, GIFClose, GIFRead, GIFSeek, GIFDraw)) {
      startTime = millis();
      isPlaying = true;
    } else {
      tft.println("Error open GIF");
      delay(1000);
      currentGifIndex = (currentGifIndex + 1) % totalGifs;
    }
  }

  if (isPlaying) {
    if (gif.playFrame(false, nullptr)) {
      if (millis() - startTime >= 5000) {
        gif.close();
        isPlaying = false;
        currentGifIndex = (currentGifIndex + 1) % totalGifs;
      }
    }
  }
}