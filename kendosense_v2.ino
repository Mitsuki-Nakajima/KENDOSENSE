#include <Arduino_BMI270_BMM150.h>
#include <ArduinoBLE.h>

const float GY_STRIKE_THRESHOLD = 300.0;
const float GZ_STRIKE_THRESHOLD = 150.0;

const unsigned long SAMPLE_DELAY_MS = 20;   // ~50 Hz
const unsigned long MIN_EVENT_GAP_MS = 250; // debounce
const unsigned long LED_ON_MS = 60;

BLEService kendoService("19B10000-E8F2-537E-4F6C-D104768A1214");
BLEStringCharacteristic dataCharacteristic(
  "19B10001-E8F2-537E-4F6C-D104768A1214",
  BLERead | BLENotify,
  80
);

unsigned long lastEventTime = 0;
unsigned long ledOffTime = 0;
int strikeCount = 0;

enum StrikeType {
  NONE,
  SLOW_STRIKE,
  FAST_STRIKE
};

const char* strikeTypeToString(StrikeType type) {
  switch (type) {
    case SLOW_STRIKE: return "SLOW";
    case FAST_STRIKE: return "FAST";
    default: return "NONE";
  }
}

StrikeType detectStrike(float gy, float gz, unsigned long nowMs) {
  float absGy = abs(gy);
  float absGz = abs(gz);

  if (nowMs - lastEventTime < MIN_EVENT_GAP_MS) {
    return NONE;
  }

  if (absGy >= 500.0 || absGz >= 250.0) {
    lastEventTime = nowMs;
    return FAST_STRIKE;
  }

  if (absGy >= GY_STRIKE_THRESHOLD || absGz >= GZ_STRIKE_THRESHOLD) {
    lastEventTime = nowMs;
    return SLOW_STRIKE;
  }

  return NONE;
}

void turnOnLed() {
  digitalWrite(LED_BUILTIN, HIGH);
  ledOffTime = millis() + LED_ON_MS;
}

void updateLed() {
  if (ledOffTime > 0 && millis() >= ledOffTime) {
    digitalWrite(LED_BUILTIN, LOW);
    ledOffTime = 0;
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  delay(500);

  if (!IMU.begin()) {
    while (1) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
  }

  if (!BLE.begin()) {
    while (1) {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(300);
      digitalWrite(LED_BUILTIN, LOW);
      delay(300);
    }
  }

  BLE.setLocalName("KendoSense");
  BLE.setDeviceName("KendoSense");
  BLE.setAdvertisedService(kendoService);

  kendoService.addCharacteristic(dataCharacteristic);
  BLE.addService(kendoService);

  dataCharacteristic.writeValue("0,0,NONE,0");
  BLE.advertise();
}

void loop() {
  updateLed();
  BLE.poll();

  float ax, ay, az;
  float gx, gy, gz;

  if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
    IMU.readAcceleration(ax, ay, az);
    IMU.readGyroscope(gx, gy, gz);

    unsigned long nowMs = millis();
    StrikeType eventType = detectStrike(gy, gz, nowMs);

    if (eventType != NONE) {
      strikeCount++;
      turnOnLed();
    }

    // Format: gy,gz,event,count
    char payload[80];
    snprintf(payload, sizeof(payload), "%.2f,%.2f,%s,%d",
             gy, gz, strikeTypeToString(eventType), strikeCount);

    dataCharacteristic.writeValue(payload);
  }

  delay(SAMPLE_DELAY_MS);
}