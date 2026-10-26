#include <WiFi.h>
#include <esp_now.h>

// 受信データの構造体（送信側と一致させる）
typedef struct struct_message {
  char uid[16];
  int readerId;
} struct_message;

struct_message incomingData;

// 登録可能タグ数の上限と配列の準備
#define MAX_TAGS 20
String registeredTags[MAX_TAGS] = {"B9732C02", "E2014302"}; // 初期登録
int currentTagCount = 2; // 現在の登録数

// 動作モードの定義
enum Mode { MODE_READ, MODE_ADD, MODE_DELETE };
Mode currentMode = MODE_READ;

// 関数プロトタイプ宣言
void checkSerialInput();
void processReadMode(String uid, int readerId);
void processAddMode(String uid);
void processDeleteMode(String uid);
int findTagIndex(String uid);
void printTagList();
void printHelp();

// ESP-NOW受信時コールバック関数（v3.x系対応）
void OnDataRecv(const esp_now_recv_info_t *mac, const uint8_t *incoming, int len) {
  memcpy(&incomingData, incoming, sizeof(incomingData));
  
  String uid = String(incomingData.uid);
  int readerId = incomingData.readerId;

  Serial.print("\n[Received via ESP-NOW] Reader #");
  Serial.print(readerId);
  Serial.print(" UID: ");
  Serial.println(uid);

  switch (currentMode) {
    case MODE_READ:
      processReadMode(uid, readerId);
      break;
    case MODE_ADD:
      processAddMode(uid);
      break;
    case MODE_DELETE:
      processDeleteMode(uid);
      break;
  }
}

void setup() {
  Serial.begin(115200);

  // ルーター接続は行わず、ESP-NOWのためにステーションモードに設定
  WiFi.mode(WIFI_STA);

  // 受信側のMACアドレスを表示（送信側に設定するために使用します）
  Serial.print("Receiver MAC Address: ");
  Serial.println(WiFi.macAddress());

  // ESP-NOWの初期化
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // 受信コールバックの登録
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Receiver ESP32 Ready (No Wi-Fi Router needed).");
  printHelp();
}

void loop() {
  // シリアル入力によるモード切り替えチェック（追加・削除・一覧表示など）
  checkSerialInput();
}

void processReadMode(String uid, int readerId) {
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