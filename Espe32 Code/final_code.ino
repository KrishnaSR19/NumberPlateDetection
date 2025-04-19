#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <FirebaseESP32.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED display settings
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET    -1  // Reset pin (or -1 if sharing Arduino reset)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Camera model selection for AI-Thinker ESP32-CAM
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

// Button pin for capturing image
#define BUTTON_PIN 13

// WiFi credentials
const char* ssid = "realme 8i";
const char* password = "7517003690";

// Firebase configuration
#define FIREBASE_HOST "https://number-plate-db-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "AIzaSyC857JI9uIRcDUq2WpVlSdbqFnAnakfSr4" 

// Firebase Objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbConfig;

// Server URL (update with your Python server IP)
const char* serverUrl = "http://192.168.58.196:5000/upload";

// Target License Plate for testing (will be updated after image processing)
String targetLicensePlate = "";

// Function to display messages on OLED
void displayMessage(String message) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println(message);
  display.display();
}

// Check if the License Plate Exists in Firebase
void checkLicensePlate(String plate) {
  displayMessage("Fetching Data From Firebase...");
  delay(3000);
  // Adjust the Firebase path below if your data is structured differently.
  // For example, if data is stored at "/vehicles/<plate>", update the path accordingly.
  String path = "/vehicles/" + plate;
  
  if (Firebase.getJSON(fbdo, path)) {  // Fetch specific vehicle data
    Serial.println("License Plate Found!");
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, fbdo.jsonString());
    
    if (error) {
      Serial.print("JSON Parsing failed: ");
      Serial.println(error.c_str());
      displayMessage("JSON Error!");
      return;
    }
    
    // Extract Data
    String owner = doc["owner"].as<String>();
    String flat_no = doc["flat_no"].as<String>();
    String vehicle_type = doc["vehicle_type"].as<String>();
    String plate_no = doc["plate_no"].as<String>();
    
    Serial.print("Owner: ");
    Serial.println(owner);
    Serial.print("Flat No: ");
    Serial.println(flat_no);
    Serial.print("Type of Vehicle ");
    Serial.println(vehicle_type);
    Serial.print("License Number: ");
    Serial.println(plate_no);
    
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Vehicle Verified!");
    display.println("Owner: " + owner);
    display.println("Flat Number: " + flat_no);
    display.println("Type of vehicle: " + vehicle_type);
    display.println("License Plate No: " + plate_no);
    delay(10000);
    //time add
    display.display();

  } else {
    Serial.println("License Plate Not Found");
    displayMessage("License Not Found!");
  }
}

void setup() {

  Serial.begin(115200);
  pinMode(12, OUTPUT);      // Set GPIO 12 as output
  digitalWrite(12, LOW);    // Set it to LOW
  
  // Initialize I2C with your OLED's SDA and SCL pins
  Wire.begin(14, 15);
  
  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED allocation failed");
    while (true);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Starting Server...");
  display.display();
  delay(1000); 
  
  // Initialize button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
   display.setCursor(0,9);
  display.println("Connecting WiFi...");
  display.display();
  delay(1000); 
  Serial.print("Connecting to WiFi");
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    Serial.print(".");
    attempt++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    display.setCursor(0,18);
    display.println("WiFi Connected!");
    display.display();
    delay(1000); 
  } else {
    Serial.println("\nWiFi Connection Failed!");
    displayMessage("WiFi Failed! Restarting...");
    delay(3000);
    ESP.restart();
  }
  
  // Initialize Firebase
  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &auth);
  Firebase.reconnectWiFi(true);
  
  // Configure camera settings
  camera_config_t camConfig;
  camConfig.ledc_channel = LEDC_CHANNEL_0;
  camConfig.ledc_timer = LEDC_TIMER_0;
  camConfig.pin_d0 = Y2_GPIO_NUM;
  camConfig.pin_d1 = Y3_GPIO_NUM;
  camConfig.pin_d2 = Y4_GPIO_NUM;
  camConfig.pin_d3 = Y5_GPIO_NUM;
  camConfig.pin_d4 = Y6_GPIO_NUM;
  camConfig.pin_d5 = Y7_GPIO_NUM;
  camConfig.pin_d6 = Y8_GPIO_NUM;
  camConfig.pin_d7 = Y9_GPIO_NUM;
  camConfig.pin_xclk = XCLK_GPIO_NUM;
  camConfig.pin_pclk = PCLK_GPIO_NUM;
  camConfig.pin_vsync = VSYNC_GPIO_NUM;
  camConfig.pin_href = HREF_GPIO_NUM;
  camConfig.pin_sccb_sda = SIOD_GPIO_NUM;
  camConfig.pin_sccb_scl = SIOC_GPIO_NUM;
  camConfig.pin_pwdn = PWDN_GPIO_NUM;
  camConfig.pin_reset = RESET_GPIO_NUM;
  camConfig.xclk_freq_hz = 20000000;
  camConfig.pixel_format = PIXFORMAT_JPEG;
  camConfig.frame_size = FRAMESIZE_QVGA;
  camConfig.jpeg_quality = 12;
  camConfig.fb_count = 1;
  
  // Initialize camera
  esp_err_t err = esp_camera_init(&camConfig);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed! Error 0x%x\n", err);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Camera init failed!");
    display.display();
    return;
  }
   display.setCursor(0,27);
  Serial.println("Camera Ready!");
  display.println("Camera Ready!");
  display.display();
  delay(1000); 
}

void loop() {
  // When button is pressed, capture and send image
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("📸 Button Pressed! Capturing image...");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Detecting Plate...");
    display.display();
    delay(2000); 
    
    captureAndSendImage();
    
    delay(1000);  // Debounce delay
  }
}

void captureAndSendImage() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Capture failed!");
    display.display();
    return;
  }
  Serial.printf(" Captured Image! Size: %d bytes\n", fb->len);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "image/jpeg");

    int httpResponseCode = http.POST(fb->buf, fb->len);
    String response = http.getString();

    if (httpResponseCode > 0) {
      Serial.printf(" Image sent! Server response: %d\n", httpResponseCode);
      Serial.println("Response: " + response);
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Image sent to the Server!");
      display.display();
      delay(1000);

      // Parse JSON response using ArduinoJson
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, response);
      if (!error) {
        if (doc.containsKey("plate_number")) {
          String plateNumber = doc["plate_number"].as<String>();
          Serial.println("Extracted Plate Number: " + plateNumber);
          targetLicensePlate = plateNumber;  // Update target license plate based on detection

          checkLicensePlate(targetLicensePlate);
          delay(10000);

          display.clearDisplay();
          display.setCursor(0, 0);
          display.println("Detected Plate:");
          display.println(plateNumber);
          display.display();
        } else {
          Serial.println(" No plate number detected.");
          display.clearDisplay();
          display.setCursor(0, 0);
          display.println("No plate detected");
          display.display();
        }
      } else {
        Serial.println(" JSON parse error!");
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("JSON parse error");
        display.display();
      }
    } else {
      Serial.printf(" Failed to send image! Error: %s\n", http.errorToString(httpResponseCode).c_str());
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("Send error!");
      display.display();
    }
    http.end();
  } else {
    Serial.println(" WiFi not connected!");
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("WiFi not connected!");
    display.display();
  }
  esp_camera_fb_return(fb);  // Release buffer memory
}