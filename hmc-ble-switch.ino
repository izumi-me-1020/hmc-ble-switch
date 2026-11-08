#include <Arduino.h>
#include <BLEDevice.h>
#include <BLE2902.h>
#include <atomic>

constexpr uint8_t switchPin = 25;  // ESP32-WROOM-32 / DevKitC
constexpr unsigned long debounceMs = 25;
constexpr char serviceUuid[] = "9460e301-ec3a-4c4a-a1b0-e07c149758ec";
constexpr char stateUuid[] = "9460e302-ec3a-4c4a-a1b0-e07c149758ec";

BLECharacteristic* switchState;
std::atomic<bool> restartAdvertising{false};
bool lastSample;
bool stableState;
unsigned long lastChange;

class SwitchServerCallbacks : public BLEServerCallbacks {
  void onDisconnect(BLEServer*) override {
    restartAdvertising = true;
  }
};

SwitchServerCallbacks callbacks;

void publishState() {
  uint8_t value = stableState ? 1 : 0;
  switchState->setValue(&value, sizeof(value));
  switchState->notify();
}

void setup() {
  pinMode(switchPin, INPUT_PULLUP);
  stableState = lastSample = digitalRead(switchPin) == LOW;
  lastChange = millis();

  BLEDevice::init("発車ベルスイッチ");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(&callbacks);
  BLEService* service = server->createService(serviceUuid);
  switchState = service->createCharacteristic(
    stateUuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  switchState->addDescriptor(new BLE2902());
  uint8_t initialValue = stableState ? 1 : 0;
  switchState->setValue(&initialValue, sizeof(initialValue));
  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(serviceUuid);
  advertising->setScanResponse(true);
  advertising->start();
}

void loop() {
  const bool sample = digitalRead(switchPin) == LOW;
  const unsigned long now = millis();
  if (sample != lastSample) {
    lastSample = sample;
    lastChange = now;
  }
  if (sample != stableState && now - lastChange >= debounceMs) {
    stableState = sample;
    publishState();
  }
  if (restartAdvertising.exchange(false)) {
    BLEDevice::startAdvertising();
  }
  delay(2);
}
