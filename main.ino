#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include "secrets.h"
// WiFi Credentials
const char* ssid = "Nut_IoT";
const char* password = "IoT09200128";

// Telegram Bot Token และ Chat ID
String BOT_TOKEN = "7570642665:AAHyN3ETHSK3F3-fTRb6RfiTfZdpAbWOF2s"; // ใส่ token จริง
String CHAT_ID = "8192731413"; // ใส่ chat_id ของคุณ

// ค่าฝุ่น
int pm25 = 0;
int pm10 = 0;

// ไม่ให้ประมวลผล message ซ้ำ
int last_message_id = -1;

// Serial สำหรับ PMS5003
HardwareSerial pmsSerial(2);

void setup() {
  Serial.begin(115200);
  pmsSerial.begin(9600, SERIAL_8N1, 16, 17); // RX=16, TX=17

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println("IP Address: " + WiFi.localIP().toString());

  testSendMessage();
}

void loop() {
  // อ่านค่าฝุ่นจาก PMS5003
  if (pmsSerial.available()) {
    uint8_t data[32];
    size_t bytesRead = pmsSerial.readBytes(data, 32);
    if (bytesRead == 32 && data[0] == 0x42 && data[1] == 0x4D) {
      pm25 = (data[12] << 8) | data[13];
      pm10 = (data[14] << 8) | data[15];

      Serial.print("PM2.5: ");
      Serial.print(pm25);
      Serial.print(" µg/m³, PM10: ");
      Serial.print(pm10);
      Serial.println(" µg/m³");

      if (pm25 > 35) {
        String message = "แจ้งเตือน! ค่าฝุ่น PM2.5 สูง: " + String(pm25) + " µg/m³";
        sendTelegramMessage(CHAT_ID, message);
      }
    }
  }

  // เช็คคำสั่งจาก Telegram
  checkTelegramCommands();

  delay(2000);
}

void checkTelegramCommands() {
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/getUpdates?offset=" + String(last_message_id + 1);
  http.begin(url);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc["ok"]) {
      JsonArray results = doc["result"].as<JsonArray>();
      for (JsonObject result : results) {
        int update_id = result["update_id"];
        last_message_id = update_id;

        JsonObject message = result["message"];
        String chat_id = String(message["chat"]["id"]);
        String text = message["text"];
        Serial.println("Received command: " + text);

        handleCommand(chat_id, text);
      }
    } else {
      Serial.print("JSON Error: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.println("HTTP request failed");
  }

  http.end();
}

void handleCommand(String chat_id, String command) {
  command.trim();
  command.toLowerCase();

  if (command == "/start" || command == "start") {
    sendTelegramMessage(chat_id, "ยินดีต้อนรับ! ใช้คำสั่งต่อไปนี้:\n/now หรือ /pm เพื่อดูค่าฝุ่น");
  } else if (command == "/now" || command == "/pm" || command == "now" || command == "pm") {
    sendTelegramMessage(chat_id, "🌫️ PM2.5: " + String(pm25) + " µg/m³\n🌫️ PM10: " + String(pm10) + " µg/m³");
  } else {
    sendTelegramMessage(chat_id, "ไม่รู้จักคำสั่ง \"" + command + "\"\nพิมพ์ /start เพื่อดูคำสั่งที่ใช้ได้");
  }
}

void sendTelegramMessage(String chat_id, String message) {
  String url = "https://api.telegram.org/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + chat_id + "&text=" + urlencode(message);

  HTTPClient http;
  http.begin(url);
  int code = http.GET();

  if (code > 0) {
    Serial.println("ส่งข้อความเรียบร้อย");
  } else {
    Serial.print("ส่งข้อความล้มเหลว: ");
    Serial.println(code);
  }

  http.end();
}

void testSendMessage() {
  sendTelegramMessage(CHAT_ID, "ทดสอบการเชื่อมต่อสำเร็จ");
}

// ฟังก์ชันเข้ารหัส URL
String urlencode(String str) {
  String encoded = "";
  char c;
  char code0, code1;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 += 7;
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 += 7;
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}
