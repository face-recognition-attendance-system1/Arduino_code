//kkkkk
#include <SPI.h>
#include <ArduinoWebsockets.h>
#include <WiFi.h>
#include <TJpg_Decoder.h>
#include <TFT_eSPI.h>

const char* ssid = "ESP32";
const char* password = "California";

using namespace websockets;
WebsocketsServer server;
WebsocketsClient client;

TFT_eSPI tft = TFT_eSPI();  // TFT library

// JPEG rendering callback
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (y >= tft.height()) return 0;
  tft.pushImage(x, y, w, h, bitmap);
  return 1;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Initialize TFT in portrait mode
  tft.begin();
  tft.setRotation(0); // Portrait orientation
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(true); // Required for JPEG decoding

  // JPEG decoder setup
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(tft_output);

  // Start Wi-Fi Access Point
  Serial.println("Setting AP...");
  WiFi.softAP(ssid, password);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP Address : ");
  Serial.println(IP);

  // Start WebSocket server
  server.listen(8888);
}

void loop() {
  if (server.poll()) {
    client = server.accept();
  }

  if (client.available()) {
    client.poll();
    WebsocketsMessage msg = client.readBlocking();

    if (msg.isBinary()) {
      // Decode and draw JPEG below time bar
      uint32_t t = millis();
      uint16_t w = 0, h = 0;
      TJpgDec.getJpgSize(&w, &h, (const uint8_t*)msg.c_str(), msg.length());
      Serial.print("Width = "); Serial.print(w); Serial.print(", height = "); Serial.println(h);

      TJpgDec.drawJpg(0, 20, (const uint8_t*)msg.c_str(), msg.length()); // Leave space for time bar

      t = millis() - t;
      Serial.print(t); Serial.println(" ms");
    } else {
      // Display time at top edge
      String timeStr = msg.data();
      Serial.println("Time received: " + timeStr);

      tft.fillRect(0, 0, tft.width(), 20, TFT_BLACK); // Clear time bar
      tft.setCursor(10, 2);
      tft.setTextSize(1.5);
      tft.print("Time: " + timeStr);
    }
  }
}
