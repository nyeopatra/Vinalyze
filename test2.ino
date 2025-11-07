#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ===== Wi-Fi credentials =====
const char* ssid     = "test";
const char* password = "test";

// ===== DFRobot ESP32-S3 AI CAM (DFR1154) Pin Map (OV3660) =====
// Source: DFRobot Wiki (SKU DFR1154)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    15
#define SIOD_GPIO_NUM    4
#define SIOC_GPIO_NUM    7

#define Y9_GPIO_NUM      16
#define Y8_GPIO_NUM      17
#define Y7_GPIO_NUM      18
#define Y6_GPIO_NUM      12
#define Y5_GPIO_NUM      10
#define Y4_GPIO_NUM      8
#define Y3_GPIO_NUM      9
#define Y2_GPIO_NUM      11

#define VSYNC_GPIO_NUM   6
#define HREF_GPIO_NUM    5
#define PCLK_GPIO_NUM    13

WebServer server(80);

void handleRoot() {
  String html =
    "<!DOCTYPE html><html><head><title>ESP32-S3 AI CAM</title>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'/>"
    "<style>body{font-family:Arial;background:#111;color:#eee;text-align:center;}a{color:#4af;}img{border:2px solid #444;border-radius:6px;}</style>"
    "</head><body>"
    "<h2>ESP32-S3 AI CAM Live Stream</h2>"
    "<p><img src='/stream' style='width:100%;max-width:640px;height:auto;' /></p>"
    "<p>Resolution can be changed in code (frame_size). Quality adjusted by jpeg_quality.</p>"
    "</body></html>";
  server.send(200, "text/html", html);
}

void handleStream() {
  WiFiClient client = server.client();
  String hdr =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
    "Cache-Control: no-cache\r\n"
    "Pragma: no-cache\r\n"
    "Connection: close\r\n\r\n";
  server.sendContent(hdr);

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Frame grab failed");
      break;
    }

    server.sendContent("--frame\r\n");
    server.sendContent("Content-Type: image/jpeg\r\n");
    server.sendContent("\r\n");

    size_t w = client.write(fb->buf, fb->len);
    server.sendContent("\r\n");

    esp_camera_fb_return(fb);

    if (w != fb->len) {
      Serial.println("Client disconnected mid-frame");
      break;
    }

    // Adjust delay for desired frame rate (lower = faster, higher load)
    delay(80);
  }

  client.stop();
  Serial.println("Stream ended");
}

void startCameraServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/stream", HTTP_GET, handleStream);
  server.onNotFound([](){
    server.send(404, "text/plain", "Not found");
  });
  server.begin();
  Serial.println("Web server started");
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  delay(100);

  // ===== Enable I2C pull-ups for camera sensor =====
  Serial.println("Initializing camera I2C pins...");
  pinMode(SIOD_GPIO_NUM, INPUT_PULLUP);
  pinMode(SIOC_GPIO_NUM, INPUT_PULLUP);
  delay(200);

  // ===== Wi-Fi =====
  WiFi.setSleep(false); // improve stream stability
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    Serial.print(".");
    delay(500);
    attempts++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed.");
    return;
  }
  Serial.println("\nWiFi connected.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // ===== Camera Config =====
  camera_config_t config;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d0       = Y2_GPIO_NUM;

  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;

  config.xclk_freq_hz = 10000000;  // Reduced to 10MHz for better stability
  config.ledc_timer   = LEDC_TIMER_0;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.pixel_format = PIXFORMAT_JPEG;

  // Choose frame size & buffers based on PSRAM
  if (psramFound()) {
    Serial.println("PSRAM found - using higher quality settings");
    config.frame_size   = FRAMESIZE_VGA; // Options: CIF, VGA, SVGA, XGA, ...
    config.jpeg_quality = 10;            // Lower = better quality
    config.fb_count     = 2;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    Serial.println("No PSRAM - using lower quality settings");
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 20;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // CAMERA_GRAB_LATEST is alternative

  // Wait before initializing camera
  Serial.println("Initializing camera...");
  delay(500);
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    Serial.println("Check your wiring and camera connection!");
    return;
  }
  Serial.println("Camera initialized OK");

  // ===== Optional Sensor Tuning =====
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    // OV3660 orientation adjustments if needed:
    // s->set_vflip(s, 1);
    // s->set_hmirror(s, 1);
    // Image tweaks:
    // s->set_brightness(s, 1);   // -2 to +2
    // s->set_contrast(s, 1);     // -2 to +2
    // s->set_saturation(s, 1);   // -2 to +2
    // s->set_whitebal(s, 1);
    // s->set_gainceiling(s, GAINCEILING_2X);
  }

  // ===== Start Server =====
  startCameraServer();

  Serial.println("\n==================================");
  Serial.println("ESP32-S3 AI CAM Ready");
  Serial.print("Landing Page: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  Serial.print("Stream (MJPEG): http://");
  Serial.print(WiFi.localIP());
  Serial.println("/stream");
  Serial.println("==================================\n");
}

void loop() {
  // Handle non-stream (root) requests
  server.handleClient();
  delay(10);
}
