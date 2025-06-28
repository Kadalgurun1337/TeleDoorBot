#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

const char* ssid = "Z-tech";  // Ganti dengan SSID WiFi kamu
const char* password = "ngokngok";  // Ganti dengan password WiFi kamu
const char* botToken = "7006935571:AAF44wIrEJ0x-r3QRd-XyM7wR2VXnQ38zAg";  // Ganti dengan token bot Telegram kamu
const String chatID = "5663178105";  // Ganti dengan chat ID kamu (gunakan bot @userinfobot untuk mendapatkannya)

WiFiClientSecure client;
UniversalTelegramBot bot(botToken, client);

const int relayPin = 1;  // Relay tersambung ke D1
const int doorPin = 2;   // Sensor pintu tersambung ke D2
int lastDoorState = LOW; // Menyimpan status terakhir pintu

unsigned long lastTimeBotRan = 0;
const int botRequestDelay = 1000;

void setup() {
  Serial.begin(9600);

  pinMode(relayPin, OUTPUT);
  pinMode(doorPin, INPUT_PULLUP);

  digitalWrite(relayPin, LOW);  // Pastikan relay mati saat startup

  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Tersambung!");
}

void checkDoorStatus() {
  int doorState = digitalRead(doorPin);

  if (doorState != lastDoorState) {
    if (doorState == HIGH) {
      bot.sendMessage(chatID, "🚪 Pintu Terbuka!", "");
    } else {
      bot.sendMessage(chatID, "🔒 Pintu Tertutup!", "");
    }
    lastDoorState = doorState;
  }
}

void handleNewMessages(int numNewMessages) {
  Serial.println("Pesan diterima!");

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    Serial.println("Pesan dari Telegram: " + text);

    if (text == "/on") {
      digitalWrite(relayPin, LOW);
      bot.sendMessage(chat_id, "Relay ON ✅", "");
    } 
    else if (text == "/off") {
      digitalWrite(relayPin, HIGH);
      bot.sendMessage(chat_id, "Relay OFF ❌", "");
    } 
    else {
      bot.sendMessage(chat_id, "Perintah tidak dikenali. Gunakan /on atau /off.", "");
    }
  }
}

void loop() {
  if (millis() - lastTimeBotRan > botRequestDelay) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  checkDoorStatus();
}
