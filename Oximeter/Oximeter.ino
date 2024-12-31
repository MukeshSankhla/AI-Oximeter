#include "DFRobot_BloodOxygen_S.h"
#include "DFRobot_GDL.h"
#include "Bitmap.h"
#include <AI_Oximeter_inferencing.h> // Include the Edge Impulse model

// Pin Definitions for ST7789 Display
#define TFT_DC  D2
#define TFT_CS  D6
#define TFT_RST D3
#define TFT_BL  D13

// Initialize the I2C MAX30102 sensor
#define I2C_ADDRESS  0x57
DFRobot_BloodOxygen_S_I2C MAX30102(&Wire, I2C_ADDRESS);

// Initialize the TFT display
DFRobot_ST7789_172x320_HW_SPI screen(TFT_DC, TFT_CS, TFT_RST, TFT_BL);

bool noFinger = true;

int pSPO2 = 0;
int pHeartRate = 0;

// Function to provide features to the classifier
int extract_features(size_t offset, size_t length, float *out_ptr, int heartRate, float bodyTemp, int SPO2) {
  float features[] = { (float)heartRate, bodyTemp, (float)SPO2 }; // Example input
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0; // Success
}

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);

  // Initialize the ST7789 display
  screen.begin();
  screen.fillScreen(COLOR_RGB565_GREEN);  // Clear the screen

  // Initialize the MAX30102 sensor
  while (!MAX30102.begin()) {
    Serial.println("MAX30102 init fail!");
    delay(1000);
  }
  Serial.println("MAX30102 init success!");
  MAX30102.sensorStartCollect();  // Start collecting data
}

void loop() {
  // Get Heartbeat, SPO2, and Temperature
  MAX30102.getHeartbeatSPO2();
  int SPO2 = MAX30102._sHeartbeatSPO2.SPO2;
  int heartRate = MAX30102._sHeartbeatSPO2.Heartbeat;
  float bodyTemp = MAX30102.getTemperature_C();
  
  Serial.println(bodyTemp);
  Serial.println(heartRate);
  Serial.println(SPO2);
  Serial.println("_____________________");

  // Check for invalid readings
  if (SPO2 == -1 || heartRate == -1) {
    if (noFinger) {
      screen.fillRoundRect(5, 5, 162, 310, 20, COLOR_RGB565_BLACK);
      screen.drawRGBBitmap(30, 90, (uint16_t *)finger, 110, 140);
      noFinger = false;
    }
  }
  else {
    // Wrap features in a signal_t object
    ei::signal_t signal;
    signal.total_length = 3; // Number of features
    signal.get_data = [=](size_t offset, size_t length, float *out_ptr) -> int {
      return extract_features(offset, length, out_ptr, heartRate, bodyTemp, SPO2);
    };

    // Run inference
    ei_impulse_result_t result; // Correct type for result
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);

    if (err != EI_IMPULSE_OK) {
      Serial.print("Error running Edge Impulse classifier: ");
      Serial.println(err);
      return;
    }
    // Get the classification result
    int classification = -1;
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      if (result.classification[ix].value > 0.5) { // Assuming 0.5 as threshold
        classification = ix;
        break;
      }
    }
    if(SPO2 != pSPO2 || heartRate != pHeartRate){
      screen.fillRoundRect(5, 5, 162, 310, 20, COLOR_RGB565_BLACK);

      screen.setFont(&FreeSerifBold12pt7b);

      screen.setTextSize(1);
      screen.setTextColor(COLOR_RGB565_GREEN);
      screen.setCursor(10, 30);
      screen.print("SpO2");
      screen.setCursor(86, 30);
      screen.print("PRbpm");
      screen.setCursor(10, 100);
      screen.print("BTemp");
      screen.setCursor(10, 170);
      screen.print("Status: ");

      screen.setTextSize(2);
      screen.setTextColor(COLOR_RGB565_GREEN);
      screen.setCursor(10, 70);
      screen.print(SPO2);
      screen.setCursor(86, 70);
      screen.print(heartRate);
      screen.setCursor(10, 140);
      screen.print(bodyTemp);
      screen.setCursor(10, 210);
      switch (classification) {
        case 0:
          screen.print("Good");
          screen.drawRGBBitmap(36, 230, (uint16_t *)good, 100, 80);
          break;
        case 1:
          screen.print("Bad");
          screen.drawRGBBitmap(36, 230, (uint16_t *)bad, 100, 80);
          break;
        case 2:
          screen.print("Sick");
          screen.drawRGBBitmap(36, 230, (uint16_t *)sick, 100, 80);
          break;
        default:
          screen.print("Unknown");
          break;
      }
      Serial.println(classification);
      Serial.println("-------------------------------");
      noFinger = true;
      pSPO2 = SPO2;
      pHeartRate = heartRate;
    }
  }
  delay(5000);
}