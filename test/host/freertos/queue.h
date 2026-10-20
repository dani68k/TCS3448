#pragma once
#include <deque>
#include <vector>
#include <cstring>
struct FakeQueue { size_t limit, size; std::deque<std::vector<uint8_t>> items; };
using QueueHandle_t=FakeQueue*;
inline QueueHandle_t xQueueCreate(size_t n,size_t size) { return new FakeQueue{n,size,{}}; }
inline int xQueueSend(QueueHandle_t q,const void* p,int) {
  if(q->items.size()==q->limit) return 0;
  q->items.emplace_back((const uint8_t*)p,(const uint8_t*)p+q->size); return 1;
}
inline int xQueueReceive(QueueHandle_t q,void* p,int) {
  if(q->items.empty()) return 0;
  memcpy(p,q->items.front().data(),q->size); q->items.pop_front(); return 1;
}
