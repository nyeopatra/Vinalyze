#include "esp_camera.h"
#include <WiFi.h>
#include <Preferences.h>

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     5
#define Y9_GPIO_NUM       4
#define Y8_GPIO_NUM       6
#define Y7_GPIO_NUM       7
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       17
#define Y4_GPIO_NUM       21
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM       16
#define VSYNC_GPIO_NUM    1
#define HREF_GPIO_NUM     2
#define PCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     8
#define SIOC_GPIO_NUM     9
#define LED_GPIO_NUM      47

Preferences preferences;
const char* ssid = "FRITZ!Box 7530 CQ";
const char* password = "83872120889660197895";

void startCameraServer();
void setupLedFlash(int pin);

bool connectToWiFi(const char* ssid, const char* password, int maxRetries = 40) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to WiFi: %s\n", ssid);
  
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < maxRetries) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("Failed to connect to WiFi.");
    return false;
  }
}

void initWiFi() {
  // 1. TRY HARDCODED CREDENTIALS FIRST
  Serial.println("Trying hardcoded WiFi credentials...");
  if (connectToWiFi(ssid, password)) {
    return; // Success! Exit here
  }
  
  // 2. Try saved credentials from NVS
  preferences.begin("wifi", false);
  String savedSSID = preferences.getString("ssid", "");
  String savedPASS = preferences.getString("password", "");

  if (savedSSID.length() > 0) {
    Serial.println("Trying saved WiFi credentials...");
    if (connectToWiFi(savedSSID.c_str(), savedPASS.c_str())) {
      preferences.end();
      return; // Success!
    }
  }

  // 3. Last resort: Ask via Serial
  Serial.println("\n=== WiFi Setup Required ===");
  Serial.println("Open Serial Monitor (115200 baud)");
  
  // Wait a bit for user to open serial monitor
  delay(3000);
  
  while (Serial.available()) Serial.read(); // Clear buffer

  Serial.println("Enter SSID: ");
  while (Serial.available() == 0) delay(100);
  String inputSSID = Serial.readStringUntil('\n');
  inputSSID.trim();

  Serial.println("Enter Password: ");
  while (Serial.available() == 0) delay(100);
  String inputPASS = Serial.readStringUntil('\n');
  inputPASS.trim();

  if (connectToWiFi(inputSSID.c_str(), inputPASS.c_str())) {
    preferences.putString("ssid", inputSSID);
    preferences.putString("password", inputPASS);
    Serial.println("New WiFi credentials saved to NVS.");
  }

  preferences.end();
}

void initCamera(){
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }
}

void setupLedFlash(int pin) {
  // Configure LED flash if available
  if (pin >= 0) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(1000);
  Serial.println("\n\nESP32-CAM Starting...");
  
  initCamera();
  setupLedFlash(LED_GPIO_NUM);
  initWiFi();
  
  if (WiFi.status() == WL_CONNECTED) {
    startCameraServer();
    Serial.println("\n========================================");
    Serial.printf("Camera Ready! Go to: http://%s\n", WiFi.localIP().toString().c_str());
    Serial.println("========================================\n");
  } else {
    Serial.println("ERROR: WiFi not connected. Camera server not started.");
  }
}

void loop() {
  delay(10000);
}
