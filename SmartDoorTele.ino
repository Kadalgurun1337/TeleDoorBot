#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>


#define BUTTON_PIN 4 // D2
#define DOOR_PIN 5   // D1
#define EEPROM_SIZE 96
#define WIFI_FAIL_FLAG_ADDR 95

// Telegram Bot
const char* BOTtoken = "8145038401:AAHCKHkJ6idv7vlffP7RnzMFlVPTjj3KtJE";
WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

// Web server
ESP8266WebServer server(80);

// Subscriber
const int MAX_SUBSCRIBERS = 5;
String subscribers[MAX_SUBSCRIBERS];
int subscriberCount = 0;

// WiFi config
String ssid, password;
bool apMode = false;
unsigned long buttonPressTime = 0;
bool buttonPressed = false;
bool apTriggerExecuted = false;

// Door
int lastDoorState = HIGH;
volatile bool doorChanged = false;

// --- EEPROM ---
void saveWiFiToEEPROM(String ssid, String pass) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(0, ssid.length());
  EEPROM.write(1, pass.length());
  for (int i = 0; i < ssid.length(); i++) EEPROM.write(2 + i, ssid[i]);
  for (int i = 0; i < pass.length(); i++) EEPROM.write(34 + i, pass[i]);
  EEPROM.commit();
}

void loadWiFiFromEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  int ssidLen = EEPROM.read(0);
  int passLen = EEPROM.read(1);
  ssid = "";
  password = "";
  for (int i = 0; i < ssidLen; i++) ssid += char(EEPROM.read(2 + i));
  for (int i = 0; i < passLen; i++) password += char(EEPROM.read(34 + i));
}

void clearEEPROM() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
  EEPROM.commit();
  Serial.println("🧹 EEPROM telah dihapus.");
}

void setWiFiFailFlag(bool failed) {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(WIFI_FAIL_FLAG_ADDR, failed ? 1 : 0);
  EEPROM.commit();
}

bool getWiFiFailFlag() {
  EEPROM.begin(EEPROM_SIZE);
  return EEPROM.read(WIFI_FAIL_FLAG_ADDR) == 1;
}

// --- AP Mode ---
void startAPMode() {
  if (apMode) return;
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("smartdoor-123", "smartdoor-123");
  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();
  Serial.println("⚙️ AP mode aktif. SSID: smartdoor-123");
}

void handleRoot() {
  bool failed = getWiFiFailFlag();
  String form = "<html><body><h1>WiFi Setup</h1>";

  if (failed) {
    form += "<p style='color:red;'>❌ Gagal konek ke WiFi sebelumnya. Coba lagi.</p>";
    setWiFiFailFlag(false); // reset agar tidak muncul lagi
  }

  form += "<form action='/save' method='POST'>"
          "SSID: <input type='text' name='ssid' required><br>"
          "Password: <input type='password' id='pass' name='pass' required><br>"
          "<input type='checkbox' onclick='togglePass()'> Tampilkan Password<br>"
          "<input type='submit' value='Simpan'>"
          "</form>"
          "<script>"
          "function togglePass() {"
          "  var x = document.getElementById('pass');"
          "  x.type = (x.type === 'password') ? 'text' : 'password';"
          "}"
          "</script></body></html>";

  server.send(200, "text/html", form);
}

void handleSave() {
  ssid = server.arg("ssid");
  password = server.arg("pass");
  saveWiFiToEEPROM(ssid, password);
  server.send(200, "text/html", "WiFi Tersimpan. Restarting...");
  delay(1000);
  ESP.restart();
}

// --- Connect ---
bool connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("🔌 Menghubungkan ke WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    if (millis() - start > 15000) return false;
  }
  Serial.println("\n✅ WiFi Terhubung: " + WiFi.localIP().toString());
  return true;
}

// --- ISR ---
void ICACHE_RAM_ATTR handleDoorInterrupt() {
  doorChanged = true;
}

// --- Telegram ---
bool isSubscribed(String chat_id) {
  for (int i = 0; i < subscriberCount; i++) {
    if (subscribers[i] == chat_id) return true;
  }
  return false;
}

void addSubscriber(String chat_id) {
  if (subscriberCount < MAX_SUBSCRIBERS && !isSubscribed(chat_id)) {
    subscribers[subscriberCount++] = chat_id;
    Serial.println("➕ Subscribed: " + chat_id);
  }
}

void removeSubscriber(String chat_id) {
  for (int i = 0; i < subscriberCount; i++) {
    if (subscribers[i] == chat_id) {
      for (int j = i; j < subscriberCount - 1; j++) {
        subscribers[j] = subscribers[j + 1];
      }
      subscriberCount--;
      Serial.println("➖ Unsubscribed: " + chat_id);
      break;
    }
  }
}

void broadcastMessage(String msg) {
  for (int i = 0; i < subscriberCount; i++) {
    bot.sendMessage(subscribers[i], msg, "");
  }
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;

    if (!isSubscribed(chat_id)) {
      addSubscriber(chat_id);
      bot.sendMessage(chat_id, "✅ Kamu sudah terdaftar untuk notifikasi pintu.", "");
    }

    if (text == "/state") {
      String status = "🚪 Status pintu: ";
      status += digitalRead(DOOR_PIN) ? "TERBUKA 🚪" : "TERTUTUP 🔒";
      bot.sendMessage(chat_id, status, "");
    } else if (text == "/unsub") {
      removeSubscriber(chat_id);
      bot.sendMessage(chat_id, "❎ Kamu berhenti menerima notifikasi otomatis.", "");
    } else {
      String msg = "👋 Hai, " + from_name + "!\n"
                   "Perintah:\n"
                   "/state - Cek status pintu\n"
                   "/unsub - Berhenti notifikasi";
      bot.sendMessage(chat_id, msg, "");
    }
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT); // D4 sebagai output indikator tombol
  digitalWrite(2, HIGH); // D3 sebagai output
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(DOOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(DOOR_PIN), handleDoorInterrupt, CHANGE);

  loadWiFiFromEEPROM();
  if (!connectToWiFi()) {
    Serial.println("\n❌ Gagal konek ke WiFi. Masuk ke mode AP...");
    setWiFiFailFlag(true);
    startAPMode();
  } else {
    client.setInsecure();
    Serial.println("🚀 Setup selesai");
  }
}

// --- Loop ---
unsigned long lastBotCheck = 0;

void loop() {
  digitalWrite(2, digitalRead(BUTTON_PIN) == LOW ? LOW : HIGH); // D3 HIGH saat tombol ditekan
  if (apMode) server.handleClient();

  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!buttonPressed) {
      buttonPressed = true;
      buttonPressTime = millis();
    } else if (millis() - buttonPressTime >= 5000 && !apTriggerExecuted) {
      Serial.println("🟡 Tombol ditekan >5 detik, clear EEPROM dan masuk AP mode");
      clearEEPROM();
      setWiFiFailFlag(false);
      startAPMode();
      apTriggerExecuted = true;
    }
  } else {
    buttonPressed = false;
  }

  if (doorChanged) {
    doorChanged = false;
    int state = digitalRead(DOOR_PIN);
    if (state != lastDoorState) {
      broadcastMessage(state == HIGH ? "⚠️🚪 Pintu DIBUKA!" : "🔒 Pintu DITUTUP!");
      lastDoorState = state;
    }
  }

  if (!apMode && millis() - lastBotCheck > 1000) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastBotCheck = millis();
  }
}
