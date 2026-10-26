#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <esp_now.h>

#define RST_PIN1    26  
#define SS_PIN1     15  
#define MISO_PIN    19
#define MOSI_PIN    23
#define RST_PIN2    21
#define SS_PIN2     17
#define SCK_PIN     18

MFRC522 rfid1(SS_PIN1, RST_PIN1);
MFRC522 rfid2(SS_PIN2, RST_PIN2);

// 受信側のESP32のMACアドレス（※ご自身の受信側のMACアドレスに書き換えてください）
uint8_t broadcastAddress[] = {0x28, 0x05, 0xA5, 0xFD, 0x21, 0x80};

// 送信データの構造体
typedef struct struct_message {
  char uid[16];
  int readerId;
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// 送信コールバック関数（v3.x系対応）
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);

  // ESP-NOWを利用するためにWi-Fiをステーションモードに設定（ルーター接続は不要）
  WiFi.mode(WIFI_STA);

  // ESP-NOWの初期化
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // 送信先（受信側）の登録
  esp_now_register_send_cb(OnDataSent);
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // CSピンの設定
  pinMode(SS_PIN1, OUTPUT);
  pinMode(SS_PIN2, OUTPUT);
  digitalWrite(SS_PIN1, HIGH);
  digitalWrite(SS_PIN2, HIGH);

  SPI.begin();

  rfid1.PCD_Init();
  rfid2.PCD_Init();
  delay(50);

  Serial.println("Sender ESP32 Ready. Waiting for tags...");
}

void loop() {
  // リーダー1の読み取り
  if (rfid1.PICC_IsNewCardPresent() && rfid1.PICC_ReadCardSerial()) {
    processCard(rfid1, 1);
  }

  // リーダー2の読み取り
  if (rfid2.PICC_IsNewCardPresent() && rfid2.PICC_ReadCardSerial()) {
    processCard(rfid2, 2);
  }
}

void processCard(MFRC522 &rfid, int readerId) {
  String currentUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      currentUID += "0";
    }
    currentUID += String(rfid.uid.uidByte[i], HEX);
  }
  currentUID.toUpperCase();

  Serial.print("\n[Card Scanned] Reader #");
  Serial.print(readerId);
  Serial.print(" UID: ");
  Serial.println(currentUID);

  // 構造体にデータをコピーして送信
  currentUID.toCharArray(myData.uid, 16);
  myData.readerId = readerId;

  // ESP-NOWで送信
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  if (result == ESP_OK) {
    Serial.println("-> [ESP-NOW] Sent successfully");
  } else {
    Serial.println("-> [ESP-NOW] Error sending data");
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
}