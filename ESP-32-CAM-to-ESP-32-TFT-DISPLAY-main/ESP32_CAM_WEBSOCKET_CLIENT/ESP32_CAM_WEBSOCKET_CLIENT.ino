//kkkkk
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoWebsockets.h>

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

using namespace websockets;

// Wi-Fi credentials
const char* ssid = "ESP32";
const char* password = "California";

// Static IP configuration
IPAddress local_IP(192, 168, 4, 50);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// WebSocket server info (ESP32-TFT)
const char* websockets_server_host = "192.168.4.1";
const uint16_t websockets_server_port = 8888;

WebsocketsClient client;
WebServer server(80);

// Time buffer
String receivedTime = "No time received";

// MJPEG client tracking
WiFiClient mjpegClient;
bool mjpegActive = false;

void handleMJPEGStream() {
  if (mjpegActive) {
    server.send(503, "text/plain", "MJPEG stream already active");
    return;
  }

  mjpegClient = server.client();
  mjpegActive = true;

  mjpegClient.print("HTTP/1.1 200 OK\r\n"
                    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n");
}

void startCustomCameraServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html",
      "<html><body><h1>ESP32-CAM Stream</h1><img src=\"/stream\" /><p>Time: " + receivedTime + "</p></body></html>");
  });

  server.on("/stream", HTTP_GET, handleMJPEGStream);

  server.on("/time", HTTP_POST, []() {
    receivedTime = server.arg("plain");
    Serial.println("Received time: " + receivedTime);
    server.send(200, "text/plain", "Time received");
  });

  server.begin();
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Camera configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Set portrait resolution: 240x320
  config.frame_size = FRAMESIZE_CIF;  // Portrait mode
  config.jpeg_quality = 10;
  config.fb_count = psramFound() ? 2 : 1;

  // Initialize camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Flip image vertically
  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);      // upside-down fix
  s->set_hmirror(s, 0);    // optional: set to 1 if mirrored

  // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("Failed to configure static IP");
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to view stream");

  startCustomCameraServer();

  // Connect to WebSocket server
  while (!client.connect(websockets_server_host, websockets_server_port, "/")) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WebSocket Connected!");
}

void loop() {
  static unsigned long lastFrameTime = 0;
  const unsigned long frameInterval = 100; // ~10 fps

  unsigned long now = millis();
  if (now - lastFrameTime < frameInterval) return;
  lastFrameTime = now;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // Send to WebSocket
  if (client.available()) {
    client.sendBinary((const char*) fb->buf, fb->len);
    client.send(receivedTime);
  }

  // Send to MJPEG client
  if (mjpegActive && mjpegClient.connected()) {
    char header[64];
    size_t hlen = snprintf(header, sizeof(header),
                           "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                           fb->len);
    mjpegClient.write(header, hlen);
    mjpegClient.write(fb->buf, fb->len);
    mjpegClient.write("\r\n", 2);
  } else {
    mjpegActive = false;
  }

  esp_camera_fb_return(fb);
  server.handleClient();
}
