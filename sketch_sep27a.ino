#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define RST_PIN1   26  
#define SS_PIN1    15  
#define MISO_PIN  19
#define MOSI_PIN  23
#define RST_PIN2   21
#define SS_PIN2    17
#define SCK_PIN    18


// Wi-Fi接続情報
const char* ssid = "YOUR_WIFI_SSID";       // Wi-FiのSSID
const char* password = "YOUR_WIFI_PASSWORD"; // Wi-Fiのパスワード

// データの送り先URL（例: WebサーバーやGoogle Apps Scriptなど）
const char* serverUrl = "http://192.168.1.100/api/scan";



MFRC522 rfid1(SS_PIN1, RST_PIN1);
MFRC522 rfid2(SS_PIN2, RST_PIN2);

// 登録可能タグ数の上限と配列の準備
#define MAX_TAGS 20
String registeredTags[MAX_TAGS] = {"B9732C02", "E2014302"}; // 初期登録
int currentTagCount = 2; // 現在の登録数

// 動作モードの定義
enum Mode { MODE_READ, MODE_ADD, MODE_DELETE };
Mode currentMode = MODE_READ;

// 関数プロトタイプ宣言（コンパイラに関数の存在を事前に知らせる）
void checkSerialInput();
void processReadMode(String uid);
void processAddMode(String uid);
void processDeleteMode(String uid);
int findTagIndex(String uid);
void printTagList();
void printHelp();
void processCard(MFRC522 &rfid);

void setup() {
  Serial.begin(115200);

  // CSピンを明示的にOUTPUT設定し、HIGH（非選択）にする
  pinMode(SS_PIN1, OUTPUT);
  pinMode(SS_PIN2, OUTPUT);
  digitalWrite(SS_PIN1, HIGH);
  digitalWrite(SS_PIN2, HIGH);

  SPI.begin();

  // 1台目の初期化
  rfid1.PCD_Init();
  delay(50);

  // 2台目の初期化
  rfid2.PCD_Init();
  delay(50);

  Serial.println("RC522 initialized. Ready to read tags...");
  printHelp();
}

void loop() {
  // 1. シリアル入力によるモード切り替えチェック
  checkSerialInput();

  // 2. RFID1の読み取り処理
  if (rfid1.PICC_IsNewCardPresent() && rfid1.PICC_ReadCardSerial()) {
    processCard(rfid1);
  }

  // 3. RFID2の読み取り処理
  if (rfid2.PICC_IsNewCardPresent() && rfid2.PICC_ReadCardSerial()) {
    processCard(rfid2);
  }
}

// カードのUID処理を共通化した関数
void processCard(MFRC522 &rfid) {
  String currentUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      currentUID += "0";
    }
    currentUID += String(rfid.uid.uidByte[i], HEX);
  }
  currentUID.toUpperCase();

  Serial.print("\n[Card Scanned] UID: ");
  Serial.println(currentUID);

  switch (currentMode) {
    case MODE_READ:
      processReadMode(currentUID);
      break;
    case MODE_ADD:
      processAddMode(currentUID);
      break;
    case MODE_DELETE:
      processDeleteMode(currentUID);
      break;
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000); // 連続読み取り防止
}

// --------------------------------------------------
// モードごとの処理関数
// --------------------------------------------------

void processReadMode(String uid) {
  int matchedIndex = findTagIndex(uid);

  if (matchedIndex != -1) {
    Serial.print("-> [MATCH] Registered Tag ID #");
    Serial.print(matchedIndex + 1);
    Serial.println(" detected!");
  } else {
    Serial.println("-> [UNKNOWN] Unregistered Tag.");
  }
}

void processAddMode(String uid) {
  if (findTagIndex(uid) != -1) {
    Serial.println("-> [ERROR] This tag is ALREADY registered!");
  } 
  else if (currentTagCount >= MAX_TAGS) {
    Serial.println("-> [ERROR] Memory full! Cannot add more tags.");
  } 
  else {
    registeredTags[currentTagCount] = uid;
    currentTagCount++;
    Serial.print("-> [SUCCESS] Tag added at ID #");
    Serial.println(currentTagCount);
  }

  currentMode = MODE_READ;
  Serial.println(">> Returned to READ MODE.");
}

void processDeleteMode(String uid) {
  int index = findTagIndex(uid);

  if (index == -1) {
    Serial.println("-> [ERROR] Tag not found in registered list.");
  } 
  else {
    for (int i = index; i < currentTagCount - 1; i++) {
      registeredTags[i] = registeredTags[i + 1];
    }
    registeredTags[currentTagCount - 1] = "";
    currentTagCount--;
    Serial.println("-> [SUCCESS] Tag removed from list.");
  }

  currentMode = MODE_READ;
  Serial.println(">> Returned to READ MODE.");
}

// --------------------------------------------------
// 補助関数
// --------------------------------------------------

int findTagIndex(String uid) {
  for (int i = 0; i < currentTagCount; i++) {
    if (uid == registeredTags[i]) {
      return i;
    }
  }
  return -1;
}

void checkSerialInput() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'a' || cmd == 'A') {
      currentMode = MODE_ADD;
      Serial.println("\n>> [MODE] ADD MODE: Scan a new tag to register.");
    } else if (cmd == 'd' || cmd == 'D') {
      currentMode = MODE_DELETE;
      Serial.println("\n>> [MODE] DELETE MODE: Scan a registered tag to remove.");
    } else if (cmd == 'r' || cmd == 'R') {
      currentMode = MODE_READ;
      Serial.println("\n>> [MODE] READ MODE: Normal operation.");
    } else if (cmd == 'l' || cmd == 'L') {
      printTagList();
    }
  }
}

void printTagList() {
  Serial.println("\n--- Registered Tags List ---");
  if (currentTagCount == 0) {
    Serial.println("(No tags registered)");
  } else {
    for (int i = 0; i < currentTagCount; i++) {
      Serial.print("#");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.println(registeredTags[i]);
    }
  }
  Serial.println("----------------------------");
}

void printHelp() {
  Serial.println("--- Command Guide ---");
  Serial.println("Send 'a' -> Enter ADD mode");
  Serial.println("Send 'd' -> Enter DELETE mode");
  Serial.println("Send 'r' -> Return to READ mode");
  Serial.println("Send 'l' -> Show tag list");
  Serial.println("--------------------");
}
