#include "esp_camera.h"
#include <WiFi.h>
#include <Preferences.h>

// --------------- Camera Pinout (adjust to your module) ---------------
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
#define LED_GPIO_NUM      47   // adjust or disable if not present

// --------------- Globals ---------------
Preferences preferences;       // NVS storage for WiFi credentials

void startCameraServer();      // from the ESP32 CameraWebServer example
void setupLedFlash(int pin);   // implement or stub out if not needed

// --------------- WiFi Connection Helper ---------------
bool connectToWiFi(const char* ssid, const char* password, uint8_t maxRetries = 40) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Connecting to WiFi: %s\n", ssid);

  uint8_t retries = 0;
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

// --------------- WiFi Scan + Serial Input Helpers ---------------
int scanAndPrintNetworks() {
  Serial.println("Scanning for WiFi networks...");
  int n = WiFi.scanNetworks();
  if (n == 0) {
    Serial.println("No networks found.");
    return 0;
  }

  Serial.printf("%d networks found:\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("%d: %s (RSSI %d) %s\n",
                  i,
                  WiFi.SSID(i).c_str(),
                  WiFi.RSSI(i),
                  (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "OPEN" : "SECURED");
    delay(10);
  }
  return n;
}

String readLineFromSerial(const char* prompt) {
  Serial.println(prompt);
  // Clear any old input
  while (Serial.available()) Serial.read();

  while (!Serial.available()) {
    delay(10);
  }
  String line = Serial.readStringUntil('\n');
  line.trim();
  return line;
}

bool getWiFiFromSerial(String &ssid, String &pass) {
  // Scan and show networks
  int n = scanAndPrintNetworks();
  if (n <= 0) {
    Serial.println("No networks to choose from. You can still enter SSID manually.");
  } else {
    // Ask user to select network index or type 'm' for manual
    while (true) {
      String choice = readLineFromSerial("Enter network number (0 - " + String(n - 1) + ") or 'm' for manual SSID:");
      if (choice.equalsIgnoreCase("m")) {
        break; // will handle manual entry below
      }

      int idx = choice.toInt();
      if (idx >= 0 && idx < n) {
        ssid = WiFi.SSID(idx);
        Serial.println("Selected SSID: " + ssid);
        goto PASSWORD_ENTRY;
      } else {
        Serial.println("Invalid selection. Try again.");
      }
    }
  }

  // Manual SSID entry
  ssid = readLineFromSerial("Enter SSID:");

PASSWORD_ENTRY:
  pass = readLineFromSerial("Enter Password (leave blank if open network):");

  return ssid.length() > 0; // at least SSID must be non-empty
}

// --------------- WiFi Initialization ---------------
void initWiFi() {
  preferences.begin("wifi", false);
  String savedSSID = preferences.getString("ssid", "");
  String savedPASS = preferences.getString("password", "");

  // 1. Try stored credentials first
  if (savedSSID.length() > 0) {
    Serial.println("Found saved WiFi credentials.");
    Serial.println("Trying stored SSID: " + savedSSID);

    if (connectToWiFi(savedSSID.c_str(), savedPASS.c_str())) {
      preferences.end();
      return; // Successful connection using stored creds
    } else {
      Serial.println("Stored credentials failed.");
    }
  } else {
    Serial.println("No stored WiFi credentials.");
  }

  // 2. Ask via Serial (scan & choose or manual)
  Serial.println("\n--- WiFi Setup via Serial ---");
  Serial.println("Open Serial Monitor at 115200 baud, 'Newline' line ending.");

  String inputSSID, inputPASS;
  if (!getWiFiFromSerial(inputSSID, inputPASS)) {
    Serial.println("No SSID provided. Cannot continue.");
    preferences.end();
    return;
  }

  if (connectToWiFi(inputSSID.c_str(), inputPASS.c_str())) {
    // Save credentials
    preferences.putString("ssid", inputSSID);
    preferences.putString("password", inputPASS);
    Serial.println("WiFi credentials saved.");
  } else {
    Serial.println("Failed to connect. Credentials not saved.");
  }

  preferences.end();
}

// --------------- Camera Initialization ---------------
void initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size   = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count     = 2;
      config.grab_mode    = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size   = FRAMESIZE_SVGA;
      config.fb_location  = CAMERA_FB_IN_DRAM;
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

// --------------- LED Flash (stub) ---------------
// If your board has a flash LED, you can implement brightness control here.
// If not, you can leave this as a no-op or change pin to -1.
void setupLedFlash(int pin) {
  if (pin < 0) return;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW); // off by default
}

// --------------- Arduino Setup / Loop ---------------
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  delay(1000);
  Serial.println();
  Serial.println("ESP32-CAM starting...");

  initCamera();
  setupLedFlash(LED_GPIO_NUM);
  initWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    startCameraServer();
    Serial.printf("Camera Stream Ready! Go to: http://%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi not connected. Camera server not started.");
  }
}

void loop() {
  // Everything handled by web server tasks
  delay(10000);
}
