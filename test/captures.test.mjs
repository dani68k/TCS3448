import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { CaptureTransport, CSV_HEADER, SUMMARY_HEADER, crc32, parseCaptures, createCaptureUI } from "../web/src/captures.mjs";
import { createTestDOM } from "../scripts/web-test-dom.mjs";

const encode=s=>new TextEncoder().encode(s);
function csvRow(id=1,description='a, "b" café',sequence=7) {
  const fields=[id,`"${description.replaceAll('"','""')}"`,1234,"64.0","50.04000",10,"AUTO",18,1,500,18000,
    1200,600,0,700,800,1500,100,200,900,1300,1400,300,400,500,1,"unknown","unknown",sequence,18,456,1,1000,1100,1600,1700];
  return fields.join(",");
}
function fixture(rows=[csvRow()]) {return encode(CSV_HEADER+"\n"+rows.map(r=>r+"\n").join(""));}
class Channel {
  constructor(bytes=fixture()) { this.bytes=bytes; this.calls=[]; this.value=new DataView(new ArrayBuffer(20)); this.staged=[]; this.linked=true; this.stale=0; this.commits=0; }
  async writeValueWithResponse(packet) {
    const p=new DataView(packet.buffer,packet.byteOffset,packet.byteLength); this.calls.push(packet.slice());
    let out=packet.slice(); out.fill(0,8); let v=new DataView(out.buffer);
    switch(packet[0]) {
      case 1: this.pin=p.getUint32(8,true); this.staged=[]; break;
      case 2: assert.equal(p.getUint16(8,true),this.staged.length); this.staged.push(...packet.slice(11,11+packet[10])); break;
      case 3:
        this.commits++; this.description=new TextDecoder().decode(Uint8Array.from(this.staged));
        if(this.holdCommit) await new Promise(resolve=>this.release=resolve);
        this.bytes=fixture([csvRow(1,this.description,this.pin)]); v.setUint32(8,1,true); v.setUint16(12,1,true); break;
      case 4: case 8:
        this.transfer=packet[0]===8?fixture([csvRow(p.getUint32(8,true))]):this.bytes;
        this.token=p.getUint32(2,true); v.setUint32(8,this.transfer.length,true); v.setUint32(12,crc32(this.transfer)^(this.badCRC?1:0),true); break;
      case 5:
        assert.equal(p.getUint32(12,true),this.token); assert.equal(p.getUint16(6,true),p.getUint32(8,true)/12);
        out.set(this.transfer.subarray(p.getUint32(8,true),p.getUint32(8,true)+12),8); break;
      case 6: this.ended=true; break;
      case 9: this.staged=[]; break;
      case 7:
        this.receipt=Uint8Array.from(this.staged); this.receiptToken=p.getUint32(2,true);
        this.deleted=Array.from({length:this.receipt.length/4},(_,i)=>new DataView(this.receipt.buffer).getUint32(i*4,true));
        this.bytes=fixture([]); v.setUint16(8,0,true); v.setUint16(10,this.deleted.length,true); break;
      case 10:
        assert.equal(p.getUint32(12,true),this.receiptToken);
        out.set(this.receipt.subarray(p.getUint32(8,true),p.getUint32(8,true)+12),8); break;
      case 11: {
        if(this.legacy) { out[1]=11; break; }
        const kind=packet[8];
        if(kind===0) this.transfer=this.bytes;
        if(kind===1) {
          const rows=parseCaptures(this.bytes);
          this.transfer=encode(SUMMARY_HEADER+"\n"+rows.map(r=>[r.id,`"${r.description.replaceAll('"','""')}"`,r.timestamp_ms,r.gain,r.integration_ms,r.led_current_ma].join(",")+"\n").join(""));
        }
        if(kind===2) {
          const id=p.getUint32(12,true);
          const line=new TextDecoder().decode(this.bytes).split("\n").find(line=>line.startsWith(id+","));
          if(!line) {out[1]=5;break;}
          this.transfer=encode(line+"\n");
        }
        this.token=p.getUint32(2,true); v.setUint32(8,this.transfer.length,true); v.setUint32(12,crc32(this.transfer)^(this.badCRC?1:0),true); v.setUint16(18,504,true); break;
      }
      case 12: {
        assert.equal(p.getUint32(12,true),this.token); const offset=p.getUint32(8,true);
        assert.equal(p.getUint16(6,true),offset/504);
        const chunk=this.transfer.subarray(offset,offset+504); out=new Uint8Array(8+chunk.length); out.set(packet.subarray(0,8)); out.set(chunk,8); v=new DataView(out.buffer); break;
      }
    }
    out[1]=this.error||out[1]||0; this.next=v;
    if(!this.stale) this.value=v;
  }
  async readValue() {
    if(this.stale-->0) return this.value;
    if(this.truncateLong && this.next.getUint8(0)===12) return new DataView(this.next.buffer,this.next.byteOffset,Math.min(22,this.next.byteLength));
    return this.value=this.next;
  }
}
function transport(channel) { return new CaptureTransport(()=>channel,()=>channel.linked); }

test("CSV schema, UTF-8, quoted commas/quotes, channel order and unknown flags survive replay",()=>{
  const [r]=parseCaptures(fixture());
  assert.equal(r.description,'a, "b" café'); assert.equal(r.analog_saturation,"unknown"); assert.equal(r.digital_saturation,"unknown");
  assert.equal(r.sample.channels.find(([name])=>name==="F1")[1],1200);
  assert.equal(r.sample.channels.find(([name])=>name==="FZ")[1],0);
  assert.equal(Number(r.adc_full_scale),18000);
  assert.equal(parseCaptures(fixture([])).length,0);
  for(const bytes of [encode("bad\n"),fixture([csvRow(2),csvRow(1)]),encode(CSV_HEADER+"\n\"bad\n"),fixture(Array.from({length:101},(_,i)=>csvRow(i+1)))]) assert.throws(()=>parseCaptures(bytes),/CSV_CORRUPT/);
});
test("pull download verifies exact bytes including header-only and 100 records, tolerates stale reads",async()=>{
  for(const bytes of [fixture([]),fixture(),fixture(Array.from({length:100},(_,i)=>csvRow(i+1)))]) {
    const channel=new Channel(bytes), t=transport(channel); channel.stale=1;
    assert.deepEqual(await t.download(),bytes); assert.equal(channel.ended,true);
    assert(channel.calls.every(p=>p.byteLength===20));
  }
  assert.equal(crc32(encode("123456789")),0xcbf43926);
});
test("CRC failures release lock; firmware errors and disconnects never return success",async()=>{
  const c=new Channel(); c.badCRC=true; await assert.rejects(transport(c).download(),/CRC32/); assert.equal(c.ended,true);
  c.error=8; await assert.rejects(transport(c).rpc(4),/CSV_CORRUPT/);
  c.linked=false; await assert.rejects(transport(c).rpc(1),/DISCONNECTED/);
});
test("staging respects minimum MTU and nine-byte UTF-8 slices",async()=>{
  const c=new Channel(), t=transport(c), bytes=encode('captura café "prueba",');
  await t.stage(bytes); assert.deepEqual(Uint8Array.from(c.staged),bytes);
  assert(c.calls.every(p=>p[10]<=9));
});
test("fast CSV needs 42 times fewer chunk commands; summaries omit channels; GET sends only its row",async()=>{
  const bytes=fixture(Array.from({length:100},(_,i)=>csvRow(i+1))), c=new Channel(bytes), t=transport(c);
  assert.deepEqual(await t.download(),bytes);
  const fastChunks=c.calls.filter(p=>p[0]===12).length;
  assert.equal(fastChunks,Math.ceil(bytes.length/504));
  assert(Math.ceil(bytes.length/12)>fastChunks*35);
  c.calls=[]; const summaries=await t.list(); assert.equal(summaries.length,100); assert(!summaries[0].sample);
  assert(c.transfer.length<bytes.length/2); assert(c.calls.some(p=>p[0]===11&&p[8]===1));
  c.calls=[]; const record=await t.record(50); assert.equal(record.id,50);
  assert(c.transfer.length<CSV_HEADER.length); assert(!new TextDecoder().decode(c.transfer).includes("record_id"));
  assert.equal(c.calls.filter(p=>p[0]===12).length,1);
});
test("older firmware and hosts truncating long reads safely fall back after closing fast transfer",async()=>{
  for(const property of ["legacy","truncateLong"]) {
    const c=new Channel(fixture(Array.from({length:4},(_,i)=>csvRow(i+1)))); c[property]=true;
    const t=transport(c); assert.deepEqual(await t.download(),c.bytes); assert.equal(t.fast,false);
    assert(c.calls.some(p=>p[0]===5)); assert(c.ended);
    const rows=await t.list(); assert.equal(rows.length,4);
    assert.equal((await t.record(2)).id,2);
  }
});
test("cancelled pull closes remote transfer without returning partial CSV",async()=>{
  const c=new Channel(), t=transport(c); t.progress=(n,total)=>{if(total&&n)t.cancelled=true;};
  await assert.rejects(t.download(),/TRANSFER_CANCELLED/); assert(c.ended);
});
const tick=()=>new Promise(resolve=>setTimeout(resolve,0));
async function until(predicate) {for(let i=0;i<100;++i) {if(predicate())return; await tick();} assert.fail("operation did not finish");}
test("LOG pins modal-opening sample, STOPPED last sample, cancel, no optimistic ACK, double clicks and disconnect",async()=>{
  const html=readFileSync(new URL("../web/src/index.template.html",import.meta.url),"utf8");
  const {document,window,el}=createTestDOM(html); globalThis.document=document; globalThis.window=window;
  const c=new Channel(fixture([])); let busy=false, ui, pending;
  const action=fn=>{ if(busy)return; busy=true; ui.controls(true); pending=(async()=>{try{await fn();}finally{busy=false;ui.controls(false);}})(); return pending; };
  ui=createCaptureUI({getChannel:()=>c,linked:()=>c.linked,running:()=>false,action}); ui.controls(false);
  assert(el["log-capture"].disabled);
  ui.sample({sequence:7,timestampMs:1234}); assert.equal(el["log-capture"].disabled,false);
  await el["log-capture"].click(); await pending;
  assert.equal(c.pin,7); assert.match(el["capture-status"].textContent,/parada/);
  ui.sample({sequence:8,timestampMs:9999});
  await el["capture-cancel"].click(); assert.equal(c.commits,0); assert(!el["capture-dialog"].open);
  await el["log-capture"].click(); await pending; assert.equal(c.pin,8);
  ui.sample({sequence:9,timestampMs:10000}); el["capture-description"].value='frozen, "eight"'; c.holdCommit=true;
  const save=el["capture-save"].click(); await until(()=>c.release);
  assert.match(el["capture-status"].textContent,/Guardando/); assert.doesNotMatch(el.message.textContent,/guardado/);
  await el["capture-save"].click(); assert.equal(c.commits,1);
  c.release(); await save; await pending;
  assert.match(el.message.textContent,/Registro #1 guardado/); assert.equal(parseCaptures(c.bytes)[0].sample.sequence,8);
  assert.equal(parseCaptures(c.bytes)[0].description,'frozen, "eight"');
  c.linked=false; ui.disconnect(); assert(el["log-capture"].disabled); assert(!el["capture-dialog"].open);
});
test("VIEW uses remotely fetched record scale; multi-delete confirms IDs, verifies receipt and clears selection",async()=>{
  const html=readFileSync(new URL("../web/src/index.template.html",import.meta.url),"utf8");
  const {document,window,el}=createTestDOM(html); globalThis.document=document; globalThis.window=window;
  const c=new Channel(fixture([csvRow(1),csvRow(2)])); let busy=false,ui,pending;
  const action=fn=>{if(busy)return; busy=true; ui.controls(true); pending=(async()=>{try{await fn();}finally{busy=false;ui.controls(false);}})();return pending;};
  ui=createCaptureUI({getChannel:()=>c,linked:()=>true,running:()=>false,action});
  await ui.refresh(); ui.controls(false);
  const rows=el["data-rows"].children; assert.equal(rows.length,2); assert.equal(rows[0].children[1].textContent,"2");
  await rows[0].children[7].children[0].click(); await pending;
  assert(c.calls.some(p=>p[0]===11&&p[8]===2)); assert.match(el["saved-title"].textContent,/Saved record #2/);
  const fetched=c.calls.length;
  await rows[0].children[7].children[0].click(); await pending;
  assert.equal(c.calls.length,fetched,"repeated VIEW must use the connection-scoped cache");
  assert.match(el["saved-setup"].textContent,/ADC 18000/); assert.match(el["saved-chart"].textContent,/18000/);
  for(const row of rows) {const box=row.children[0].children[0];box.checked=true;box.handlers.get("change")();}
  let confirmation; window.confirm=message=>{confirmation=message;return false;};
  await el["data-delete"].click(); assert(!c.calls.some(p=>p[0]===7));
  window.confirm=message=>{confirmation=message;return true;};
  await el["data-delete"].click(); await pending;
  assert.match(confirmation,/1, 2/); assert.deepEqual(c.deleted,[1,2]); assert(c.calls.some(p=>p[0]===10));
  assert.match(el["saved-title"].textContent,/Select a saved record/); assert.equal(el["data-count"].textContent,"0 / 100");
});
