#pragma once
#include "Arduino.h"
#include <vector>
class BLECharacteristic;
class BLECharacteristicCallbacks {
public:
  virtual void onRead(BLECharacteristic*) {}
  virtual void onWrite(BLECharacteristic*) {}
};
class BLECharacteristic {
  std::vector<uint8_t> bytes;
public:
  static constexpr unsigned PROPERTY_READ=1, PROPERTY_WRITE=2;
  BLECharacteristicCallbacks* callbacks=nullptr;
  void setCallbacks(BLECharacteristicCallbacks* c) { callbacks=c; }
  void setValue(const uint8_t* b,size_t n) { bytes.assign(b,b+n); }
  size_t getLength() { return bytes.size(); }
  uint8_t* getData() { return bytes.data(); }
  void clientWrite(const uint8_t* b,size_t n) { setValue(b,n); callbacks->onWrite(this); }
  const uint8_t* clientRead() { callbacks->onRead(this); return bytes.data(); }
};
class BLEService {
public:
  BLECharacteristic characteristic;
  BLECharacteristic* createCharacteristic(const char*,unsigned) { return &characteristic; }
};
