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

uint16_t lineBuffer[320];  // Buffer para una línea de la imagen

// Funciones de manejo de archivos para PNGdec
static void *pngOpen(const char *filename, int32_t *size) {
  File *file = new File(SD.open(filename, FILE_READ));
  if (file && *file) {
    *size = file->size();
    return (void *)file;
  }
  delete file;
  return nullptr;
}

static void pngClose(void *handle) {
  File *file = (File *)handle;
  if (file) {
    file->close();
    delete file;
  }
}

static int32_t pngRead(PNGFILE *file, uint8_t *buffer, int32_t length) {
  File *f = (File *)file->fHandle;
  return f->read(buffer, length);
}

static int32_t pngSeek(PNGFILE *file, int32_t position) {
  File *f = (File *)file->fHandle;
  return f->seek(position);
}

// Función de dibujo para PNGdec
void pngDraw(PNGDRAW *pDraw) {
  png.getLineAsRGB565(pDraw, lineBuffer, PNG_RGB565_LITTLE_ENDIAN, 0x000000);
  tft.drawRGBBitmap(0, pDraw->y, lineBuffer, pDraw->iWidth, 1);
}

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);

  if (!SD.begin(SD_CS)) {
    Serial.println("Error al inicializar la tarjeta SD");
    while (true)
      ;
  }
}

void loop() {
  static int frame = 1;
  char filename[32];
  sprintf(filename, "/idle/%d.png", frame);

  int16_t rc = png.open(filename, pngOpen, pngClose, pngRead, pngSeek, pngDraw);
  if (rc == PNG_SUCCESS) {
    png.decode(NULL, 0);
    png.close();
  } else {
    Serial.print("Error al decodificar PNG: ");
    Serial.println(rc);
  }

  // Determinar la duración de la imagen
  int delayTime = (frame == 5) ? 600 : 100;
  delay(delayTime);

  frame++;
  // Reiniciar la secuencia si no se encuentra el siguiente archivo
  char nextFilename[32];
  sprintf(nextFilename, "/idle/%d.png", frame);
  if (!SD.exists(nextFilename)) {
    frame = 1;
  }
}
