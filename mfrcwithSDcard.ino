#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>
#include <driver/i2s.h>

// ------------------- PIN -------------------
#define RFID_SCK  19  
#define RFID_MISO 15  
#define RFID_MOSI 22 
#define RFID_SS   23
#define RFID_RST  4

#define SD_SCK  17
#define SD_MISO 5
#define SD_MOSI 18 
#define SD_CS   2

#define I2S_BCLK 25
#define I2S_LRC  26
#define I2S_DIN  33

// ------------------- SPI BUS -------------------
SPIClass SPI_SD(VSPI);
SPIClass SPI_RFID(HSPI);
MFRC522 rfid(RFID_SS, RFID_RST, &SPI_RFID);

// ------------------- GLOBAL VAR -------------------
File file;
bool isPlaying = false;
byte lastUID[4] = {0,0,0,0};

uint8_t buffer[512];
size_t bytes_written;

unsigned long lastSeen = 0;
const unsigned long CARD_TIMEOUT = 1500;

// ------------------- UID ของเพลง -------------------
byte style[4] =      {0x55, 0x9C, 0xD1, 0x49};
byte shakeitoff[4] = {0xAE, 0x4F, 0x37, 0x06};
byte blankspace[4] = {0x9C, 0x4C, 0x3A, 0x06};

// ------------------- FUNCTIONS -------------------
bool isSameUID(byte *a, byte *b) {
  for (int i=0;i<4;i++) if (a[i] != b[i]) return false;
  return true;
}

// เปิดไฟล์ตาม UID
void openFileForUID(byte *uid) {

  if (file) file.close();

  if (isSameUID(uid, style))
    file = SD.open("/music/Taylor-Swift-style.wav");
  else if (isSameUID(uid, shakeitoff))
    file = SD.open("/music/Taylor-Swift-shakeitoff.wav");
  else if (isSameUID(uid, blankspace))
    file = SD.open("/music/Taylor-Swift-blankSpace.wav");

  if (!file) {
    Serial.println("❌ Failed to open file");
    return;
  }

  Serial.println("🎵 New song selected");
  file.seek(44);
  memcpy(lastUID, uid, 4);
  isPlaying = true;
}

// ------------------- I2S INIT -------------------
void i2s_init() {

  i2s_config_t config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 256
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DIN,
    .data_in_num = -1
  };

  i2s_driver_install(I2S_NUM_0, &config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}


// ------------------- TASK: RFID -------------------
void TaskRFID(void *pv) {

  while (1) {

    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {

        lastSeen = millis();

        // ถ้าเป็นบัตรใบเดิม → resume
        if (isSameUID(rfid.uid.uidByte, lastUID)) {
            Serial.println("▶ Resume song");
            isPlaying = true;

            // ❌ ห้าม return
            // ✅ ใช้ continue เพื่อกลับไป loop ใหม่
            vTaskDelay(50 / portTICK_PERIOD_MS);
            continue;
        }

        // ถ้าเป็นบัตรใหม่ → เปิดเพลงใหม่
        openFileForUID(rfid.uid.uidByte);
    }

    // ถ้าบัตรหายไปนาน
    if (isPlaying && (millis() - lastSeen > CARD_TIMEOUT)) {
        Serial.println("❌ Card removed → Pause");
        isPlaying = false;
        // ไม่ต้องล้าง lastUID
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}



// ------------------- TASK: AUDIO -------------------
void TaskAudio(void *pv) {

  while (1) {

    if (isPlaying && file) {

      if (file.available()) {
        size_t bytes_read = file.read(buffer, sizeof(buffer));
        i2s_write(I2S_NUM_0, buffer, bytes_read, &bytes_written, portMAX_DELAY);
      } else {
        // เล่นซ้ำ
        file.seek(44);
      }
    }

    vTaskDelay(1);
  }
}


// ------------------- SETUP -------------------
void setup() {
  Serial.begin(115200);

  // SD
  SPI_SD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI_SD)) {
    Serial.println("❌ SD mount fail");
    while(1);
  }
  Serial.println("✅ SD Ready");

  // RFID
  SPI_RFID.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
  rfid.PCD_Init();
  Serial.println("RFID Ready");

  // Audio
  i2s_init();
  Serial.println("I2S Ready");

  // Create Tasks
  xTaskCreatePinnedToCore(TaskRFID,  "RFID",  4096, NULL, 1, NULL, 1); // core 1
  xTaskCreatePinnedToCore(TaskAudio, "AUDIO", 4096, NULL, 1, NULL, 0); // core 0
}

void loop() {
  // ไม่ใช้ loop แล้ว
}
