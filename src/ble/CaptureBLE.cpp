#include "CaptureBLE.h"
#include "BLEProtocol.h"
#include "TCS3448Manager/TCS3448Manager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <atomic>

namespace {
using namespace TCS3448Protocol;
using namespace DataStore;
BLECharacteristic* channel=nullptr;
QueueHandle_t queue=nullptr;
portMUX_TYPE responseLock=portMUX_INITIALIZER_UNLOCKED;
uint8_t response[512]={};
size_t responseLength=20;
struct Request { uint8_t bytes[20]; uint32_t epoch; };
std::atomic<uint32_t> epoch{0};
Snapshot history[32], pinned;
uint8_t next=0;
uint32_t pinToken=0, transferToken=0, transferTime=0;
uint8_t staging[400]={}; size_t staged=0;
uint8_t deleteReceipt[400]={}; size_t receiptLength=0;
uint32_t receiptToken=0;
class Callbacks: public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic* c) override {
    // Writes replace a characteristic's value before onWrite is invoked.
    // Always serve the processed response, never that unexecuted command echo.
    uint8_t copy[512]; size_t length;
    portENTER_CRITICAL(&responseLock); length=responseLength; memcpy(copy,response,length); portEXIT_CRITICAL(&responseLock);
    c->setValue(copy,length);
  }
  void onWrite(BLECharacteristic* c) override {
    if(c->getLength()!=20) return;
    Request r; memcpy(r.bytes,c->getData(),20); r.epoch=epoch.load();
    xQueueSend(queue,&r,0); // Full queue => bounded client timeout, never a false ACK.
  }
} callbacks;
}
namespace CaptureBLE {
bool Init(BLEService* service) {
  DataStore::Init();
  queue=xQueueCreate(8,sizeof(Request));
  if(!queue) return false;
  channel=service->createCharacteristic(CaptureUUID,BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_WRITE);
  channel->setCallbacks(&callbacks);
  uint8_t empty[20]={}; channel->setValue(empty,20);
  return true;
}
void Reset() {
  epoch.fetch_add(1); DataStore::Close(); pinToken=transferToken=receiptToken=0; staged=receiptLength=0;
  for(auto& s:history) s.fullScale=0;
  portENTER_CRITICAL(&responseLock); memset(response,0,20); responseLength=20; portEXIT_CRITICAL(&responseLock);
}
void Remember(const TCS3448Sample& sample) {
  auto& s=history[next++%32]; s.sample=sample; s.config=TCS3448Manager::GetConfig(); s.fullScale=TCS3448Manager::GetADCFullScale();
}
void Update() {
  if(transferToken && millis()-transferTime>15000) { DataStore::Close(); transferToken=0; }
  Request r;
  if(!queue||xQueueReceive(queue,&r,0)!=pdTRUE||r.epoch!=epoch.load()) return;
  const auto* p=r.bytes; uint8_t out[512]={}; size_t outLength=20;
  out[0]=p[0]; memcpy(out+2,p+2,6); // request32 + sequence16 are echoed
  Error e=DataStore::OK; uint32_t request=Get32(p+2);
  if(!request) e=INVALID_PAYLOAD;
  else switch(p[0]) {
    case 1: { // Pin exactly the sample visible when LOG was clicked.
      bool found=false;
      if(transferToken) { e=DataStore::BUSY; break; }
      for(const auto& s:history) if(s.fullScale&&s.sample.sequence==Get32(p+8)) { pinned=s; found=true; break; }
      if(!found) { e=NO_SAMPLE; break; }
      pinToken=request; staged=0; Put32(out+8,pinned.sample.sequence); break;
    }
    case 2: { // Staged bytes, strict ordered offsets, max 9 per request.
      size_t offset=Get16(p+8), length=p[10];
      if(transferToken) e=DataStore::BUSY;
      else if(length>9 || offset!=staged || offset+length>sizeof(staging)) e=INVALID_PAYLOAD;
      else { memcpy(staging+offset,p+11,length); staged+=length; }
      break;
    }
    case 3: {
      const uint32_t started=millis();
      uint32_t token=Get32(p+8), id=0; size_t length=Get16(p+12);
      if(transferToken) e=DataStore::BUSY;
      else if(!pinToken||token!=pinToken) e=NO_SAMPLE;
      else if(length!=staged||length>80) e=INVALID_DESCRIPTION;
      else e=DataStore::Insert(pinned,std::string((char*)staging,length),token,id);
      Serial.printf("[DATA] LOG result=%u duration=%lums rows=%u\n",unsigned(e),static_cast<unsigned long>(millis()-started),DataStore::Count());
      Put32(out+8,id); Put16(out+12,DataStore::Count()); break;
    }
    case 4: case 8: { // Begin exact CSV or GET_RECORD by ID.
      if(transferToken) { e=DataStore::BUSY; break; }
      if(p[0]==8&&!Get32(p+8)) { e=RECORD_NOT_FOUND; break; }
      uint32_t size=0,crc=0;
      e=DataStore::Open(p[0]==8?Get32(p+8):0,size,crc);
      if(e==DataStore::OK) { transferToken=request; transferTime=millis(); }
      Put32(out+8,size); Put32(out+12,crc); Put16(out+16,DataStore::Count()); break;
    }
    case 5: {
      if(!transferToken||Get32(p+12)!=transferToken) { e=TRANSFER_FAILED; break; }
      uint32_t offset=Get32(p+8); size_t length=0;
      if(offset/12!=Get16(p+6)) { e=INVALID_PAYLOAD; break; }
      e=DataStore::Read(offset,out+8,length); transferTime=millis(); break;
    }
    case 6:
      if(transferToken && Get32(p+8)!=transferToken) e=TRANSFER_FAILED;
      else { DataStore::Close(); transferToken=0; } break;
    case 7: {
      const uint32_t started=millis();
      size_t count=Get16(p+8); uint32_t ids[100];
      if(transferToken) e=DataStore::BUSY;
      else if(!count||count>100||count*4!=staged) e=INVALID_RECORD_IDS;
      else { for(size_t i=0;i<count;++i) ids[i]=Get32(staging+4*i); e=DataStore::Delete(ids,count); }
      if(e==DataStore::OK) { memcpy(deleteReceipt,staging,staged); receiptLength=staged; receiptToken=request; }
      Serial.printf("[DATA] DELETE result=%u duration=%lums rows=%u\n",unsigned(e),static_cast<unsigned long>(millis()-started),DataStore::Count());
      Put16(out+8,DataStore::Count()); Put16(out+10,count); break;
    }
    case 9: if(transferToken) e=DataStore::BUSY; else { staged=0; pinToken=0; } break;
    case 10: { // Pull exact deleted IDs from the correlated successful ACK.
      size_t offset=Get32(p+8);
      if(!receiptToken||Get32(p+12)!=receiptToken||offset>=receiptLength||offset%12||offset/12!=Get16(p+6)) e=INVALID_PAYLOAD;
      else memcpy(out+8,deleteReceipt+offset,std::min(size_t(12),receiptLength-offset));
      break;
    }
    case 11: { // Fast begin: exact CSV, compact summaries or one headerless row.
      if(transferToken) { e=DataStore::BUSY; break; }
      if(p[8]>2) { e=INVALID_PAYLOAD; break; }
      const uint32_t id=Get32(p+12);
      if((p[8]==2&&!id)||(p[8]!=2&&id)) { e=INVALID_PAYLOAD; break; }
      uint32_t size=0,crc=0;
      e=DataStore::Open(id,size,crc,static_cast<Transfer>(p[8]));
      if(e==DataStore::OK) { transferToken=request; transferTime=millis(); }
      Put32(out+8,size); Put32(out+12,crc); Put16(out+16,DataStore::Count()); Put16(out+18,504); break;
    }
    case 12: {
      uint32_t offset=Get32(p+8); size_t length=0;
      if(!transferToken||Get32(p+12)!=transferToken) e=TRANSFER_FAILED;
      else if(offset/504!=Get16(p+6)) e=INVALID_PAYLOAD;
      else e=DataStore::Read(offset,out+8,length,504);
      if(e==DataStore::OK) outLength=8+length;
      transferTime=millis(); break;
    }
    default: e=INVALID_PAYLOAD;
  }
  out[1]=e;
  portENTER_CRITICAL(&responseLock); memcpy(response,out,outLength); responseLength=outLength; portEXIT_CRITICAL(&responseLock);
}
}
