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
using namespace TCS3448Protocol;
struct QueuedCommand {
  Command command = Command::Invalid;
  uint32_t session = 0;
  TCS3448Config config;
  bool validPayload = true;
};
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
TCS3448Error commandError = TCS3448Error::None;
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
  packet[3] = static_cast<uint8_t>(commandError != TCS3448Error::None ? commandError : status.error);
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
  const auto& state = TCS3448Manager::GetStatus();
  uint8_t packet[PacketSize] = {};
  packet[0] = ConfigVersion;
  packet[1] = static_cast<uint8_t>(config.gain);
  packet[2] = static_cast<uint8_t>(config.smux);
  packet[3] = static_cast<uint8_t>(config.autozero);
  packet[4] = config.ledEnabled;
  packet[5] = state.ready;
  Put16(packet + 6, config.ledCurrent);
  Put32(packet + 8, static_cast<uint32_t>(config.integrationMs * 1000.0 + 0.5));
  Put32(packet + 12, config.sampleIntervalMs);
  Put16(packet + 16, TCS3448Manager::GetADCFullScale());
  Put16(packet + 18, state.configRevision);
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
    QueuedCommand queued;
    queued.session = session.load();
    const size_t size = characteristic->getLength();
    const auto* data = characteristic->getData();
    if (size && data[0] == ApplyTag) {
      queued.command = Command::ApplyConfig;
      queued.validPayload = DecodeApply(data, size, queued.config);
    } else {
      const String text = characteristic->getValue();
      if (text == "START") queued.command = Command::Start;
      else if (text == "STOP") queued.command = Command::Stop;
      else if (text == "GET_STATUS") queued.command = Command::GetStatus;
      else if (text == "GET_CONFIG") queued.command = Command::GetConfig;
      else if (text == "APPLY_CONFIG") {
        queued.command = Command::ApplyConfig;
        queued.validPayload = false;
      }
    }
    // No sensor access or acquisition work on the Bluetooth task.
    if (xQueueSend(commands, &queued, 0) != pdTRUE) queueFull.store(true);
  }
};
ServerCallbacks serverCallbacks;
CommandCallbacks commandCallbacks;

void execute(const QueuedCommand& queued) {
  const Command command = queued.command;
  lastCommand = command;
  lastResult = Result::Ok;
  commandError = TCS3448Error::None;
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
      if (!TCS3448Manager::RefreshConfig()) lastResult = Result::SensorError;
      publishConfig();
      break;
    case Command::ApplyConfig: {
      Serial.println("[BLE] APPLY_CONFIG received");
      auto config = queued.config;
      const auto& active = TCS3448Manager::GetConfig();
      config.sdaPin = active.sdaPin;
      config.sclPin = active.sclPin;
      config.i2cFrequency = active.i2cFrequency;
      commandError = queued.validPayload ? TCS3448Manager::ApplyConfig(config) : TCS3448Error::InvalidPayload;
      if (commandError != TCS3448Error::None) {
        lastResult = commandError == TCS3448Error::ConfigApplyFailed ? Result::ConfigApplyFailed : Result::InvalidConfig;
      }
      fragmentIndex = FragmentCount;
      publishConfig();  // Configuration snapshot is ready before the application ACK.
      Serial.printf("[BLE] CONFIG_ACK: %s\n", TCS3448Manager::ErrorName(commandError));
      break;
    }
    default:
      lastResult = Result::UnknownCommand;
      break;
  }
  ++responseCounter;
  publishStatus(true);
}
}

namespace TCS3448BLE {
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
    TCS3448Manager::StopAcquisition(true);
    publishConfig();
    fragmentIndex = FragmentCount;
    lastCommand = Command::None;
    lastResult = Result::Ok;
    commandError = TCS3448Error::None;
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
    if (connected.load() && queued.session == currentSession) execute(queued);
  }
  if (queueFull.exchange(false)) {
    lastCommand = Command::Invalid;
    lastResult = Result::QueueFull;
    commandError = TCS3448Error::None;
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
  if (previousError != TCS3448Manager::GetStatus().error) {
    commandError = TCS3448Error::None;
    publishConfig();
    publishStatus(true);
  } else if (now - lastStatusMs >= 1000) publishStatus(true);
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
