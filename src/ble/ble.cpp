#include "ble.h"
#include "BLEProtocol.h"
#include "TCS3448Manager/TCS3448Manager.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace {
using namespace AtlantesProtocol;
struct QueuedCommand { Command command; uint32_t session; };
QueueHandle_t commands = nullptr;
std::atomic<bool> connected{false};
std::atomic<bool> queueFull{false};
std::atomic<uint32_t> session{0};
uint32_t handledSession = 0;
BLECharacteristic* statusCharacteristic = nullptr;
BLECharacteristic* configCharacteristic = nullptr;
BLECharacteristic* sampleCharacteristic = nullptr;
BLE2902* statusSubscription = nullptr;
BLE2902* sampleSubscription = nullptr;
Command lastCommand = Command::None;
Result lastResult = Result::Ok;
uint16_t responseCounter = 0;
uint32_t lastStatusMs = 0;
bool restartAdvertising = false;
uint32_t disconnectedMs = 0;
uint8_t encodedSample[SampleSize] = {};
uint32_t sampleSequence = 0;
uint8_t fragmentIndex = FragmentCount;
uint32_t lastFragmentMs = 0;

void publishStatus(bool notify) {
  const auto& status = TCS3448Manager::GetStatus();
  uint8_t packet[PacketSize] = {};
  packet[0] = Version;
  packet[1] = status.ready;
  packet[2] = status.acquiring;
  packet[3] = static_cast<uint8_t>(status.error);
  packet[4] = static_cast<uint8_t>(lastCommand);
  packet[5] = static_cast<uint8_t>(lastResult);
  Put16(packet + 6, responseCounter);
  Put32(packet + 8, status.sampleCount);
  Put32(packet + 12, millis());
  Put32(packet + 16, TCS3448Manager::GetConfig().sampleIntervalMs);
  statusCharacteristic->setValue(packet, sizeof(packet));
  if (notify && connected.load() && statusSubscription->getNotifications()) statusCharacteristic->notify();
  lastStatusMs = millis();
}

void publishConfig() {
  const auto& config = TCS3448Manager::GetConfig();
  uint8_t packet[PacketSize] = {};
  packet[0] = Version;
  packet[1] = static_cast<uint8_t>(config.gain);
  packet[2] = config.atime;
  packet[3] = config.ledCurrent;
  Put16(packet + 4, config.astep);
  Put16(packet + 6, config.sampleIntervalMs);
  const uint32_t integrationUs = static_cast<uint32_t>(
      (config.atime + 1.0f) * (config.astep + 1.0f) * 2.78f + 0.5f);
  Put32(packet + 8, integrationUs);
  Put32(packet + 12, config.i2cFrequency);
  packet[16] = config.sdaPin;
  packet[17] = config.sclPin;
  configCharacteristic->setValue(packet, sizeof(packet));
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    connected.store(true);
    session.fetch_add(1);
  }
  void onDisconnect(BLEServer*) override {
    connected.store(false);
    session.fetch_add(1);
    // CCCDs must be enabled again by each new browser connection.
    statusSubscription->setNotifications(false);
    sampleSubscription->setNotifications(false);
  }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    const String text = characteristic->getValue();
    Command command = Command::Invalid;
    if (text == "START") command = Command::Start;
    else if (text == "STOP") command = Command::Stop;
    else if (text == "GET_STATUS") command = Command::GetStatus;
    else if (text == "GET_CONFIG") command = Command::GetConfig;
    QueuedCommand queued{command, session.load()};
    // No sensor access or acquisition work on the Bluetooth task.
    if (xQueueSend(commands, &queued, 0) != pdTRUE) queueFull.store(true);
  }
};
ServerCallbacks serverCallbacks;
CommandCallbacks commandCallbacks;

void execute(Command command) {
  lastCommand = command;
  lastResult = Result::Ok;
  switch (command) {
    case Command::Start:
      if (!TCS3448Manager::StartAcquisition()) lastResult = Result::SensorError;
      break;
    case Command::Stop:
      fragmentIndex = FragmentCount;
      if (!TCS3448Manager::StopAcquisition()) lastResult = Result::SensorError;
      break;
    case Command::GetStatus:
      break;
    case Command::GetConfig:
      publishConfig();
      break;
    default:
      lastResult = Result::UnknownCommand;
      break;
  }
  ++responseCounter;
  publishStatus(true);
}
}

namespace AtlantesBLE {
bool Init() {
  commands = xQueueCreate(8, sizeof(QueuedCommand));
  if (!commands) return false;
  BLEDevice::init(DeviceName);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(&serverCallbacks);
  BLEService* service = server->createService(BLEUUID(ServiceUUID), 20);
  statusCharacteristic = service->createCharacteristic(StatusUUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  statusSubscription = new BLE2902();
  statusCharacteristic->addDescriptor(statusSubscription);
  auto* command = service->createCharacteristic(CommandUUID, BLECharacteristic::PROPERTY_WRITE);
  command->setCallbacks(&commandCallbacks);
  sampleCharacteristic = service->createCharacteristic(SampleUUID, BLECharacteristic::PROPERTY_NOTIFY);
  sampleSubscription = new BLE2902();
  sampleCharacteristic->addDescriptor(sampleSubscription);
  configCharacteristic = service->createCharacteristic(ConfigUUID, BLECharacteristic::PROPERTY_READ);
  publishConfig();
  publishStatus(false);
  service->start();
  auto* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(ServiceUUID);
  advertising->setScanResponse(true);
  advertising->start();
  return true;
}

void Update() {
  if (!commands) return;
  const uint32_t now = millis();
  const uint32_t currentSession = session.load();
  if (handledSession != currentSession) {
    handledSession = currentSession;
    TCS3448Manager::StopAcquisition();
    fragmentIndex = FragmentCount;
    lastCommand = Command::None;
    lastResult = Result::Ok;
    queueFull.store(false);
    restartAdvertising = !connected.load();
    disconnectedMs = now;
    publishStatus(true);
  }
  if (restartAdvertising && !connected.load() && now - disconnectedMs >= 200) {
    BLEDevice::startAdvertising();
    restartAdvertising = false;
  }
  QueuedCommand queued;
  for (uint8_t budget = 0; budget < 8 && xQueuePeek(commands, &queued, 0) == pdTRUE; ++budget) {
    // Leave commands from a newly connected session queued for the next loop.
    if (session.load() != currentSession) return;
    xQueueReceive(commands, &queued, 0);
    if (connected.load() && queued.session == currentSession) execute(queued.command);
  }
  if (queueFull.exchange(false)) {
    lastCommand = Command::Invalid;
    lastResult = Result::QueueFull;
    ++responseCounter;
    publishStatus(true);
  }
  if (!connected.load() || session.load() != currentSession) return;

  const auto previousError = TCS3448Manager::GetStatus().error;
  TCS3448Sample sample;
  if (TCS3448Manager::Update(sample)) {
    // Never replace an incomplete frame; drop the new transmission if needed.
    if (fragmentIndex == FragmentCount && sampleSubscription->getNotifications()) {
      EncodeSample(sample, encodedSample);
      sampleSequence = sample.sequence;
      fragmentIndex = 0;
      lastFragmentMs = now - 15;
    }
  }
  if (previousError != TCS3448Manager::GetStatus().error || now - lastStatusMs >= 1000) publishStatus(true);
  if (!TCS3448Manager::GetStatus().acquiring) fragmentIndex = FragmentCount;
  if (fragmentIndex < FragmentCount && now - lastFragmentMs >= 15 && connected.load()) {
    if (!sampleSubscription->getNotifications()) {
      fragmentIndex = FragmentCount;
      return;
    }
    uint8_t packet[PacketSize] = {};
    packet[0] = Version;
    packet[1] = fragmentIndex;
    packet[2] = FragmentCount;
    Put32(packet + 4, sampleSequence);
    memcpy(packet + 8, encodedSample + fragmentIndex * 12, 12);
    sampleCharacteristic->setValue(packet, sizeof(packet));
    sampleCharacteristic->notify();
    ++fragmentIndex;
    lastFragmentMs = now;
  }
}
}
