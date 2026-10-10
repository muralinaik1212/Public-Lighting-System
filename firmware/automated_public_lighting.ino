#include "esp_camera.h"

// =====================================================
// AI-THINKER ESP32-CAM CAMERA PINS
// =====================================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


// =====================================================
// PROJECT LED PINS
// =====================================================

#define LED_ZONE_1 13
#define LED_ZONE_2 14
#define LED_ZONE_3 15


// =====================================================
// LIGHT LEVELS
// =====================================================

#define STANDBY_BRIGHTNESS 40
#define HALF_BRIGHTNESS    128
#define ACTIVE_BRIGHTNESS  255


// =====================================================
// MOVEMENT SETTINGS
// =====================================================

#define MOVEMENT_THRESHOLD 25
#define MOVEMENT_PERCENT   3.0

#define MOVEMENT_TIMEOUT 3000

unsigned long lastMovementTime = 0;


// =====================================================
// CAMERA SETUP
// =====================================================

void startCamera() {

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

  // Grayscale camera image
  config.pixel_format = PIXFORMAT_GRAYSCALE;

  // Small image for faster movement detection
  config.frame_size = FRAMESIZE_QQVGA;

  config.jpeg_quality = 12;

  config.fb_count = 2;

  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
  } 
  else {
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t result = esp_camera_init(&config);

  if (result != ESP_OK) {

    Serial.print("Camera initialization failed: 0x");
    Serial.println(result, HEX);

    while (true) {
      delay(1000);
    }
  }

  Serial.println("Camera initialized successfully.");
}


// =====================================================
// SET LIGHTS ACCORDING TO MOVEMENT
//
// bit 0 = Zone 1 movement
// bit 1 = Zone 2 movement
// bit 2 = Zone 3 movement
// =====================================================

void setLights(byte movementZones) {

  bool zone1 = movementZones & 0b001;
  bool zone2 = movementZones & 0b010;
  bool zone3 = movementZones & 0b100;


  // ---------------------------------------------------
  // NO MOVEMENT
  // ---------------------------------------------------

  if (!zone1 && !zone2 && !zone3) {

    analogWrite(LED_ZONE_1, STANDBY_BRIGHTNESS);
    analogWrite(LED_ZONE_2, STANDBY_BRIGHTNESS);
    analogWrite(LED_ZONE_3, STANDBY_BRIGHTNESS);

    Serial.println("NO MOVEMENT - ALL STANDBY");

    return;
  }


  // ---------------------------------------------------
  // ZONE 1
  // ---------------------------------------------------

  if (zone1) {
    analogWrite(LED_ZONE_1, ACTIVE_BRIGHTNESS);
  }
  else {
    analogWrite(LED_ZONE_1, STANDBY_BRIGHTNESS);
  }


  // ---------------------------------------------------
  // ZONE 2
  // ---------------------------------------------------

  if (zone2) {
    analogWrite(LED_ZONE_2, ACTIVE_BRIGHTNESS);
  }
  else {
    // Zone 2 stays at 50% when movement
    // is detected in Zone 1 or Zone 3.
    if (zone1 || zone3) {
      analogWrite(LED_ZONE_2, HALF_BRIGHTNESS);
    }
    else {
      analogWrite(LED_ZONE_2, STANDBY_BRIGHTNESS);
    }
  }


  // ---------------------------------------------------
  // ZONE 3
  // ---------------------------------------------------

  if (zone3) {
    analogWrite(LED_ZONE_3, ACTIVE_BRIGHTNESS);
  }
  else {
    // Zone 3 gets 50% only when Zone 2 is active.
    if (zone2) {
      analogWrite(LED_ZONE_3, HALF_BRIGHTNESS);
    }
    else {
      analogWrite(LED_ZONE_3, STANDBY_BRIGHTNESS);
    }
  }


  // ---------------------------------------------------
  // SERIAL MESSAGE
  // ---------------------------------------------------

  Serial.print("ACTIVE ZONES: ");

  if (zone1) Serial.print("ZONE 1 ");
  if (zone2) Serial.print("ZONE 2 ");
  if (zone3) Serial.print("ZONE 3 ");

  Serial.println();
}


// =====================================================
// FIND MOVEMENT IN ALL THREE ZONES
// =====================================================

byte findMovementZones(camera_fb_t *currentFrame,
                       camera_fb_t *previousFrame) {

  int width = currentFrame->width;
  int height = currentFrame->height;

  long zone1 = 0;
  long zone2 = 0;
  long zone3 = 0;


  // ---------------------------------------------------
  // COMPARE CAMERA FRAMES
  // ---------------------------------------------------

  for (int y = 0; y < height; y += 4) {

    for (int x = 0; x < width; x += 4) {

      int position = y * width + x;

      int difference =
        abs((int)currentFrame->buf[position] -
            (int)previousFrame->buf[position]);


      if (difference > MOVEMENT_THRESHOLD) {

        if (x < width / 3) {

          zone1++;
        }

        else if (x < (2 * width / 3)) {

          zone2++;
        }

        else {

          zone3++;
        }
      }
    }
  }


  // ---------------------------------------------------
  // CALCULATE MOVEMENT PERCENTAGE
  // ---------------------------------------------------

  int totalSamples =
    ((width + 3) / 4) *
    ((height + 3) / 4);


  float zone1Percent =
    ((float)zone1 / totalSamples) * 100.0;

  float zone2Percent =
    ((float)zone2 / totalSamples) * 100.0;

  float zone3Percent =
    ((float)zone3 / totalSamples) * 100.0;


  Serial.print("Z1: ");
  Serial.print(zone1Percent, 1);
  Serial.print("%  ");

  Serial.print("Z2: ");
  Serial.print(zone2Percent, 1);
  Serial.print("%  ");

  Serial.print("Z3: ");
  Serial.print(zone3Percent, 1);
  Serial.println("%");


  // ---------------------------------------------------
  // CREATE MOVEMENT ZONE MASK
  // ---------------------------------------------------

  byte movementZones = 0;


  if (zone1Percent >= MOVEMENT_PERCENT) {

    movementZones |= 0b001;
  }


  if (zone2Percent >= MOVEMENT_PERCENT) {

    movementZones |= 0b010;
  }


  if (zone3Percent >= MOVEMENT_PERCENT) {

    movementZones |= 0b100;
  }


  return movementZones;
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("AUTOMATED PUBLIC LIGHTING");
  Serial.println("REVIEW 2 PROTOTYPE");
  Serial.println("================================");


  // LED pins

  pinMode(LED_ZONE_1, OUTPUT);
  pinMode(LED_ZONE_2, OUTPUT);
  pinMode(LED_ZONE_3, OUTPUT);


  // Start with all lights at standby

  setLights(0);


  // Start camera

  Serial.println("Starting camera...");

  startCamera();

  Serial.println("System ready.");
  Serial.println("Waiting for movement...");
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {


  // ---------------------------------------------------
  // GET FIRST FRAME
  // ---------------------------------------------------

  camera_fb_t *previousFrame =
    esp_camera_fb_get();


  if (!previousFrame) {

    Serial.println("Camera frame error.");

    delay(500);

    return;
  }


  delay(100);


  // ---------------------------------------------------
  // GET SECOND FRAME
  // ---------------------------------------------------

  camera_fb_t *currentFrame =
    esp_camera_fb_get();


  if (!currentFrame) {

    Serial.println("Camera frame error.");

    esp_camera_fb_return(previousFrame);

    delay(500);

    return;
  }


  // ---------------------------------------------------
  // FIND MOVEMENT IN EACH ZONE
  // ---------------------------------------------------

  byte movementZones =
    findMovementZones(currentFrame, previousFrame);


  // ---------------------------------------------------
  // RELEASE CAMERA FRAMES
  // ---------------------------------------------------

  esp_camera_fb_return(previousFrame);
  esp_camera_fb_return(currentFrame);


  // ---------------------------------------------------
  // MOVEMENT DETECTED
  // ---------------------------------------------------

  if (movementZones != 0) {

    setLights(movementZones);

    lastMovementTime = millis();
  }


  // ---------------------------------------------------
  // NO MOVEMENT
  // ---------------------------------------------------

  else {

    if (millis() - lastMovementTime >
        MOVEMENT_TIMEOUT) {

      setLights(0);
    }
  }


  delay(100);
}
