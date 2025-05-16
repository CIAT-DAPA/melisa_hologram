#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <PNGdec.h>
#include "esp32dumbdisplay.h"
#include <driver/i2s.h>

// —————— Bluetooth DumbDisplay ——————
#define BLUETOOTH "ESP32BT"
DumbDisplay dumbdisplay(new DDBluetoothSerialIO(BLUETOOTH));
int soundChunkId = -1;

// —————— I²S MIC (INMP441) ——————
#define I2S_WS 25
#define I2S_SD 33
#define I2S_SCK 32
#define I2S_BITS 16
#define SAMPLE_RATE 8000
#define CHANNEL_COUNT 1
#define I2S_PORT I2S_NUM_0

// —————— I²S AMP (MAX98357) ——————
#define I2S_BCK 13
#define I2S_LRC 4
#define I2S_DIN 21
#define I2S_NUM I2S_NUM_1
#define I2S_SAMPLE_BITS 16
#define BUFFER_SIZE 1024

// Buffer I²S
static const int BUF_BYTES = (SAMPLE_RATE == 16000 ? 512 : 256);
static const int BUF_LEN = (I2S_BITS == 32 ? BUF_BYTES / 4 : BUF_BYTES / 2);
#if I2S_BITS == 32
int32_t i2sBuf[BUF_LEN];
#else
int16_t i2sBuf[BUF_LEN];
#endif

// —————— TFT + PNG dec ——————
#define TFT_CS 27
#define TFT_RST 26
#define TFT_DC 14
#define SD_CS 5

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
PNG png;
uint16_t lineBuf[256];
const int16_t X_OFF = (320 - 256) / 2, Y_OFF = (240 - 256) / 2;

// Callbacks PNGdec
static void* pngOpen(const char* name, int32_t* size) {
  File* f = new File(SD.open(name, FILE_READ));
  if (f && *f) {
    *size = f->size();
    return f;
  }
  delete f;
  return nullptr;
}
static void pngClose(void* h) {
  File* f = (File*)h;
  if (f) {
    f->close();
    delete f;
  }
}
static int32_t pngRead(PNGFILE* pf, uint8_t* b, int32_t l) {
  return ((File*)pf->fHandle)->read(b, l);
}
static int32_t pngSeek(PNGFILE* pf, int32_t p) {
  return ((File*)pf->fHandle)->seek(p);
}
static void pngDraw(PNGDRAW* p) {
  png.getLineAsRGB565(p, lineBuf, PNG_RGB565_LITTLE_ENDIAN, 0);
  tft.drawRGBBitmap(X_OFF, Y_OFF + p->y, lineBuf, p->iWidth, 1);
}

// Reproduce animación desde carpeta
void playAnim(const char* folder, uint16_t frames, const uint16_t* delays) {
  static uint16_t frame = 1;
  static uint32_t last = millis();
  uint32_t now = millis();
  if (now - last < delays[frame - 1]) return;
  last = now;
  char path[32];
  snprintf(path, sizeof(path), "/%s/%u.png", folder, frame);
  if (png.open(path, pngOpen, pngClose, pngRead, pngSeek, pngDraw) == PNG_SUCCESS) {
    png.decode(NULL, 0);
    png.close();
  }
  frame = frame < frames ? frame + 1 : 1;
}

// Configs de animación
const uint16_t idleDelays[] = { 100, 100, 100, 100, 100, 100, 200, 200, 1500 };
const uint16_t thinkDelays[] = { 100, 100, 100, 300, 300, 300, 200, 200, 1000 };
const uint16_t talkDelays[] = { 100, 100, 100, 100, 100, 100, 100, 100, 100, 100 };
enum State { IDLE,
             THINK,
             TALK } state = IDLE;

// Umbral y temporización
const int16_t THRESH = 200;  // ajustar según tu micrófono
uint32_t talkStartMs = 0;
uint32_t silenceStartMs = 0;
const uint32_t SILENCE_DELAY_MS = 250;
const uint32_t VOICE_DELAY_MS = 500;
uint32_t voiceStartMs = 0;

// —————— Funciones I2S ——————
esp_err_t installI2S() {
  i2s_config_t cfg = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = i2s_bits_per_sample_t(I2S_BITS),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 4,
    .dma_buf_len = BUF_LEN,
    .use_apll = false
  };
  return i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
}
esp_err_t configPins() {
  i2s_pin_config_t pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };
  return i2s_set_pin(I2S_PORT, &pins);
}

void setupI2SOutput() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,  // Default, will be updated based on WAV
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };

  i2s_pin_config_t pin_config = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM);
}

bool audioPlaying = false;

void audioTask(void* param) {
  // Usa tu código de playAudio() aquí:
  File wav = SD.open("/audio/test1.wav");
  if (!wav) {
    Serial.println("ERROR: Cannot open WAV");
    vTaskDelete(NULL);
  }
  wav.seek(24);
  uint32_t sampleRate;
  wav.read((uint8_t*)&sampleRate, 4);
  i2s_set_sample_rates(I2S_NUM, sampleRate);
  wav.seek(44);
  uint8_t buffer[BUFFER_SIZE];
  size_t bytesRead, bytesWritten;
  while ((bytesRead = wav.read(buffer, BUFFER_SIZE)) > 0) {
    i2s_write(I2S_NUM, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
  }
  wav.close();
  vTaskDelete(NULL);
}

void setup() {
  Serial.begin(115200);
  // — TFT + SD —
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  if (!SD.begin(SD_CS)) {
    while (1)
      ;
  }
  // — I2S —
  installI2S();
  configPins();
  i2s_zero_dma_buffer(I2S_PORT);
  i2s_start(I2S_PORT);
  setupI2SOutput();
  // — Bluetooth audio —
  dumbdisplay.recordLayerSetupCommands();
  dumbdisplay.playbackLayerSetupCommands("stream");
  soundChunkId = dumbdisplay.streamSound16(SAMPLE_RATE, CHANNEL_COUNT);
}


void loop() {
  // 1) Leer I2S
  size_t bytesRead = 0;
  if (i2s_read(I2S_PORT, i2sBuf, BUF_BYTES, &bytesRead, portMAX_DELAY) != ESP_OK) return;

  // 2) Calcular amplitud máxima
  int16_t maxAmp = 0;
  int16_t* buf16 = (int16_t*)i2sBuf;
  int samples = bytesRead / 2;
  for (int i = 0; i < samples; i++) {
    int16_t v = abs(buf16[i]);
    if (v > maxAmp) maxAmp = v;
  }

  // *** Serial debug ***
  Serial.print("THRESH = ");
  Serial.print(THRESH);
  Serial.print("  |  maxAmp = ");
  Serial.println(maxAmp);
  // ********************

  // 3) Enviar audio al teléfono
  if (soundChunkId >= 0 && bytesRead > 0) {
    dumbdisplay.sendSoundChunk16(soundChunkId, buf16, samples, false);
  }

  // 4) Lógica de estado VAD con retardo de detección de voz
  uint32_t now = millis();
  switch (state) {
    case IDLE:
      if (maxAmp > THRESH) {
        // Si es la primera vez que detectamos voz, iniciamos el conteo
        if (voiceStartMs == 0) {
          voiceStartMs = now;
        }
        // Si llevamos suficiente tiempo con voz, pasamos a THINK
        else if (now - voiceStartMs >= VOICE_DELAY_MS) {
          state = THINK;
          silenceStartMs = 0;
          playAnim("think", 9, thinkDelays);
          // limpiamos voiceStartMs para la próxima vez
          voiceStartMs = 0;
          break;
        }
        // Mientras aún no cumple el retraso, seguimos en IDLE visualmente
        playAnim("idle", 9, idleDelays);
      } else {
        // No hay voz: reiniciamos el contador y permanecemos en IDLE
        voiceStartMs = 0;
        playAnim("idle", 9, idleDelays);
      }
      break;

    case THINK:
      if (maxAmp > THRESH) {
        // Sigue hablando: mantenemos THINK y reiniciamos silencio
        silenceStartMs = 0;
        playAnim("think", 9, thinkDelays);
      } else {
        // Ha cesado la voz: comprobamos retardo de silencio
        if (silenceStartMs == 0) {
          silenceStartMs = now;
        } else if (now - silenceStartMs >= SILENCE_DELAY_MS) {
          state = TALK;
          talkStartMs = now;
          playAnim("talk", 10, talkDelays);
        } else {
          playAnim("think", 9, thinkDelays);
        }
      }
      break;

    case TALK:
      // Inicia la tarea solo una vez
      if (!audioPlaying) {
        audioPlaying = true;
        // Pínala al core 1 para no interferir con I²S lecturas en core 0
        xTaskCreatePinnedToCore(
          audioTask,    // función
          "AudioTask",  // nombre
          4096,         // tamaño de stack (ajusta si falla)
          NULL,         // parámetro
          1,            // prioridad
          NULL,         // handle
          1             // core
        );
      }
      // Siempre seguimos animando
      playAnim("talk", 10, talkDelays);

      // Tras 3 s volvemos a IDLE
      if (millis() - talkStartMs >= 3000) {
        state = IDLE;
        audioPlaying = false;
      }
      break;
  }
}