#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <SD.h>
#include <driver/i2s.h>
// mfrc522
/*
#define SS_PIN 23
#define RST_PIN 4
#define SCK_PIN 19
#define MISO_PIN 15
#define MOSI_PIN 22
*/
#define RFID_SCK  19  
#define RFID_MISO 15  
#define RFID_MOSI 22 
#define RFID_SS   23
#define RFID_RST  4

// SD Card reader
/*
#define SD_CS   2
#define SD_SCK  17
#define SD_MISO 5
#define SD_MOSI 18
*/
#define SD_SCK  17
#define SD_MISO 5
#define SD_MOSI 18 
#define SD_CS   2

// MAX98357A
#define I2S_BCLK 25
#define I2S_LRC  26
#define I2S_DIN  33

SPIClass SPI_SD(VSPI);   // สำหรับ SD
SPIClass SPI_RFID(HSPI); // สำหรับ MFRC522
// mfrc522
MFRC522 rfid(RFID_SS, RFID_RST, &SPI_RFID);
byte style[4] = {0x55, 0x9C, 0xD1, 0x49};  // UID ของบัตรที่อนุญาต
byte shakeitoff[4] = {0xAE, 0x4F, 0x37, 0x06};  // UID ของบัตรที่อนุญาต
byte blankspace[4] = {0x9C, 0x4C, 0x3A, 0x06};  // UID ของบัตรที่อนุญาต
byte badblood[4] = {0xAA, 0xBB, 0xCC, 0xDD};  // UID ของบัตรที่อนุญาต
byte all[4] = {0xAA, 0xBB, 0xCC, 0xDD};  // UID ของบัตรที่อนุญาต

byte lastUID[4] = {0,0,0,0};
bool isPlaying = false;
File file;
size_t filePosition = 0;  // เก็บตำแหน่งไฟล์ที่เล่นค้าง
uint8_t buffer[512];
size_t bytes_written;

bool isSameUID(byte *uidA, byte *uidB) {
  for (byte i = 0; i < 4; i++) {
    if (uidA[i] != uidB[i]) return false;
  }
  return true;
}

// i2s
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


// ฟังก์ชันเช็ค UID ว่าเป็นบัตรเดิมหรือใหม่
bool isSameAsLast(byte *uid) {
    for (byte i = 0; i < 4; i++) {
        if (uid[i] != lastUID[i]) return false;
    }
    return true;
}

// ฟังก์ชันเปิดไฟล์เพลงตาม UID
void openFileForUID(byte *uid) {
    if (isSameUID(uid, style)){ 
      file = SD.open("/music/Taylor-Swift-style.wav"); 
      Serial.println("Taylor-Swift-style.wav");
    }
    else if (isSameUID(uid, shakeitoff)) {
      file = SD.open("/music/Taylor-Swift-shakeitoff.wav"); 
      Serial.println("Taylor-Swift-shakeitoff.wav");
    }
    else if (isSameUID(uid, blankspace)) {
      file = SD.open("/music/Taylor-Swift-blankSpace.wav"); 
      Serial.println("Taylor-Swift-blankSpace");
    }
    else if (isSameUID(uid, badblood)) {
      file = SD.open("/music/Taylor-Swift-badblood.wav"); 
      Serial.println("Taylor-Swift-badblood");
    }
    // เพิ่มบัตรอื่นๆ ตามต้องการ

    if (!file) {
        Serial.println("❌ Failed to open file!");
        return;
    }
    file.seek(44);       // ข้าม header
    filePosition = 44;
    memcpy(lastUID, uid, 4);
    isPlaying = true;
}

void setup() {
  Serial.begin(115200);



  // SD ใช้ VSPI
  SPI_SD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI_SD)) {
    Serial.println("❌ SD Card mount fail");
    while(1);
  }
  Serial.println("✅ SD Card OK");

    // RFID ใช้ HSPI
  SPI_RFID.begin(RFID_SCK, RFID_MISO, RFID_MOSI, RFID_SS);
  rfid.PCD_Init();
  Serial.println("RFID Ready");

  i2s_init();
  Serial.println("I2S Ready");
}

bool cardDetected = false;
unsigned long lastSeen = 0;
const unsigned long CARD_TIMEOUT = 1500; // ms

void loop() {

    bool newCard = false;

    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        newCard = true;
        lastSeen = millis();    // บันทึกเวลาเห็นบัตร
        cardDetected = true;
    }

    if (cardDetected) {
        // เล่นเพลงตาม lastUID
        if (newCard && !isSameAsLast(rfid.uid.uidByte)) {
            if (file) file.close();
            openFileForUID(rfid.uid.uidByte);
        }
    }

    // ตรวจ timeout
    if (cardDetected && (millis() - lastSeen > CARD_TIMEOUT)) {
        Serial.println("❌ บัตรถูกเอาออก หยุดเพลง");
        if (file) file.close();
        cardDetected = false;
        isPlaying = false;
        memset(lastUID, 0, 4);
    }

    static unsigned long lastPrint = 0;
    // เล่นเพลงต่อถ้ามีบัตรและกำลังเล่น
    if (isPlaying && file) {
        if (file.available()) {
            size_t bytes_read = file.read(buffer, sizeof(buffer));
            i2s_write(I2S_NUM_0, buffer, bytes_read, &bytes_written, portMAX_DELAY);
            filePosition += bytes_read;
              // พิมพ์สถานะทุก 500 ms จะได้ไม่รก
            if (millis() - lastPrint > 500) {
              Serial.print("Playing... bytes Read: ");
              Serial.println(bytes_read);
              lastPrint = millis();
            }
        } else {
            // เล่นจบ → loop หรือหยุดก็ได้
            file.seek(44);   // ถ้าอยากให้เล่นซ้ำ
            filePosition = 44;
        }
    }

}
