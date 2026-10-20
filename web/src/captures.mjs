import { createRawHistogram } from "./ui.mjs";

export const CSV_HEADER = "record_id,description,timestamp_ms,gain,integration_ms,led_current_ma,led_mode,auto_smux,autozero,sample_period_ms,adc_full_scale,f1,f2,fz,f3,f4,f5,fy,fxl,f6,f7,f8,nir,vis_tl,vis_br,valid,analog_saturation,digital_saturation,sequence,channel_count,request_id,saturated,vis_tl_1,vis_br_1,vis_tl_2,vis_br_2";
export const CAPTURE_ERRORS = ["OK", "NO_SAMPLE", "INVALID_DESCRIPTION", "STORAGE_UNAVAILABLE", "STORAGE_FULL", "RECORD_NOT_FOUND", "INVALID_RECORD_IDS", "BUSY", "CSV_CORRUPT", "WRITE_FAILED", "TRANSFER_FAILED", "INVALID_PAYLOAD"];
export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const b of bytes) { crc ^= b; for (let i=0;i<8;i++) crc=(crc>>>1) ^ ((crc&1)?0xedb88320:0); }
  return (~crc)>>>0;
}
export const SUMMARY_HEADER = "record_id,description,timestamp_ms,gain,integration_ms,led_current_ma";
function parseCSV(bytes, header) {
  const text = new TextDecoder("utf-8", { fatal:true }).decode(bytes);
  const lines = text.split("\n");
  if (lines.shift() !== header || lines.pop() !== "" || lines.length>100) throw new Error("CSV_CORRUPT");
  let previous=0;
  return lines.map(line => {
    const fields=[]; let i=0;
    while (i<=line.length) {
      let field="";
      if (line[i]==='"') {
        i++; let closed=false;
        while(i<line.length) { const c=line[i++]; if(c==='"') { if(line[i]==='"') { field+='"'; i++; } else { closed=true; break; } } else field+=c; }
        if(!closed || (i<line.length && line[i]!==',')) throw new Error("CSV_CORRUPT");
      } else { while(i<line.length && line[i]!==',') { if(line[i]==='"') throw new Error("CSV_CORRUPT"); field+=line[i++]; } }
      fields.push(field); if(i===line.length) break; i++;
    }
    if(fields.length!==header.split(",").length) throw new Error("CSV_CORRUPT");
    const r=Object.fromEntries(header.split(",").map((name,i)=>[name,fields[i]]));
    r.id=Number(r.record_id);
    if(!Number.isSafeInteger(r.id)||r.id<=previous||r.id>0xffffffff) throw new Error("CSV_CORRUPT");
    previous=r.id;
    return r;
  });
}
export function parseSummaries(bytes) { return parseCSV(bytes, SUMMARY_HEADER); }
export function parseCaptures(bytes) {
  return parseCSV(bytes, CSV_HEADER).map(r=>{
    const names=["F1","F2","FZ","F3","F4","F5","FY","FXL","F6","F7","F8","NIR","VIS_TL_0","VIS_BR_0","VIS_TL_1","VIS_BR_1","VIS_TL_2","VIS_BR_2"];
    const keys=["f1","f2","fz","f3","f4","f5","fy","fxl","f6","f7","f8","nir","vis_tl","vis_br","vis_tl_1","vis_br_1","vis_tl_2","vis_br_2"];
    r.sample={ sequence:Number(r.sequence), timestampMs:Number(r.timestamp_ms), gain:Number(r.gain), saturated:r.saturated==="1", channelCount:Number(r.channel_count), channels:names.flatMap((name,i)=>r[keys[i]]===""?[]:[[name,Number(r[keys[i]])]]) };
    return r;
  });
}

export class CaptureTransport {
  constructor(getChannel, linked, progress=()=>{}) { this.getChannel=getChannel; this.linked=linked; this.progress=progress; this.counter=globalThis.crypto.getRandomValues(new Uint32Array(1))[0]; }
  async rpc(op, payload=new Uint8Array(), sequence=0) {
    const channel=this.getChannel();
    if(!channel||!this.linked()) throw new Error("DISCONNECTED");
    const request=(++this.counter)>>>0 || ++this.counter;
    const packet=new Uint8Array(20), view=new DataView(packet.buffer);
    packet[0]=op; view.setUint32(2,request,true); view.setUint16(6,sequence,true); packet.set(payload,8);
    await channel.writeValueWithResponse(packet);
    const deadline=Date.now()+6000;
    while(Date.now()<deadline) {
      if(this.getChannel()!==channel||!this.linked()) throw new Error("DISCONNECTED");
      const result=await channel.readValue();
      if((op===12 ? result.byteLength>=8 && result.byteLength<=512 : result.byteLength===20) && result.getUint8(0)===op && result.getUint32(2,true)===request && result.getUint16(6,true)===sequence) {
        if(result.getUint8(1)) throw new Error(CAPTURE_ERRORS[result.getUint8(1)]||"TRANSFER_FAILED");
        return {view:result, request};
      }
      await new Promise(resolve=>setTimeout(resolve,25));
    }
    throw new Error("ACK_TIMEOUT — resultado desconocido; refrescar Data antes de otra captura");
  }
  async stage(bytes) {
    if(bytes.length>400) throw new Error("INVALID_PAYLOAD");
    for(let i=0;i<bytes.length;i+=9) {
      const payload=new Uint8Array(12); new DataView(payload.buffer).setUint16(0,i,true);
      payload[2]=Math.min(9,bytes.length-i); payload.set(bytes.subarray(i,i+9),3);
      await this.rpc(2,payload);
    }
  }
  async list() {
    const {bytes, fast}=await this.transfer(1);
    return fast ? parseSummaries(bytes) : parseCaptures(bytes);
  }
  async record(id) {
    const {bytes, fast}=await this.transfer(2,id);
    let csv=bytes;
    if(fast) { const header=new TextEncoder().encode(CSV_HEADER+"\n"); csv=new Uint8Array(header.length+bytes.length); csv.set(header); csv.set(bytes,header.length); }
    const records=parseCaptures(csv);
    if(records.length!==1 || records[0].id!==id) throw new Error("RECORD_NOT_FOUND");
    return records[0];
  }
  async download() { return (await this.transfer(0)).bytes; }
  async transfer(kind=0,id=0) {
    try { return await this.readTransfer(kind,id); }
    catch(error) {
      // A few host Bluetooth stacks expose only the first ATT read fragment.
      // This retry is read-only and occurs after END has released the first transfer.
      if(this.fast===true && error.message==="TRANSFER_FAILED: chunk length") {
        this.fast=false; return this.readTransfer(kind,id);
      }
      throw error;
    }
  }
  async readTransfer(kind=0,id=0) {
    this.cancelled=false;
    const payload=new Uint8Array(4); new DataView(payload.buffer).setUint32(0,id,true);
    let begin, fast=false;
    if(this.fast!==false) {
      const request=new Uint8Array(8); request[0]=kind; new DataView(request.buffer).setUint32(4,id,true);
      try { begin=await this.rpc(11,request); fast=true; this.fast=true; }
      catch(error) { if(this.fast===undefined && error.message==="INVALID_PAYLOAD") this.fast=false; else throw error; }
    }
    if(!fast) begin=await this.rpc(kind===2?8:4,payload);
    const stride=fast?begin.view.getUint16(18,true):12;
    const size=begin.view.getUint32(8,true), expected=begin.view.getUint32(12,true);
    try {
      if(size>71000 || size<1 || (fast && stride!==504)) throw new Error("TRANSFER_FAILED");
      const bytes=new Uint8Array(size);
      this.progress(0,size);
      for(let offset=0;offset<size;offset+=stride) {
        if(this.cancelled) throw new Error("TRANSFER_CANCELLED");
        const next=new Uint8Array(8), v=new DataView(next.buffer);
        v.setUint32(0,offset,true); v.setUint32(4,begin.request,true);
        const {view}=await this.rpc(fast?12:5,next,offset/stride);
        const length=Math.min(stride,size-offset);
        if(fast && view.byteLength!==8+length) throw new Error("TRANSFER_FAILED: chunk length");
        bytes.set(new Uint8Array(view.buffer,view.byteOffset+8,length),offset);
        this.progress(Math.min(size,offset+stride),size);
      }
      if(this.cancelled) throw new Error("TRANSFER_CANCELLED");
      if(crc32(bytes)!==expected) throw new Error("TRANSFER_FAILED: CRC32");
      return {bytes, fast};
    } finally {
      this.progress(0,0);
      const end=new Uint8Array(4); new DataView(end.buffer).setUint32(0,begin.request,true);
      if(this.linked()) await this.rpc(6,end);
    }
  }
}

export function createCaptureUI({ getChannel, linked, running, action }) {
  const get=id=>document.getElementById(id);
  const text=(id,value)=>{ get(id).textContent=value; };
  const histogram=createRawHistogram(get("saved-chart"),get("saved-extrema"),get("saved-vis"));
  const transport=new CaptureTransport(getChannel,linked,(n,total)=>{ get("data-cancel").hidden=!total; if(total) text("data-status",`Descargando ${n} / ${total} bytes`); });
  get("data-cancel").hidden=true;
  const recordCache=new Map();
  let latest=null, frozen=null, pin=0, records=[], selected=null, blocked=false;
  function controls(busy=blocked) {
    blocked=busy;
    get("log-capture").disabled=busy||!linked()||!latest;
    for(const id of ["data-refresh","data-download","data-delete"]) get(id).disabled=busy||!linked()||(id==="data-delete"&&!checks().length);
    get("capture-save").disabled=busy||!linked()||!pin;
  }
  function checks() { return records.filter(r=>r.checkbox?.checked).map(r=>r.id); }
  function fail(error) { text("data-status",error.message); text("capture-status",error.message); throw error; }
  async function refresh() {
    recordCache.clear();
    try {
      records=await transport.list();
      // Old firmware lists full rows: reuse those verified remote snapshots too.
      for(const record of records) if(record.sample) recordCache.set(record.id,record);
      text("data-count",`${records.length} / 100`);
      get("data-rows").replaceChildren(...[...records].reverse().map(r=>{
        const row=document.createElement("tr"), box=document.createElement("input"); box.type="checkbox"; box.setAttribute("aria-label",`Seleccionar #${r.id}`); r.checkbox=box;
        box.addEventListener("change",()=>controls());
        const cell=document.createElement("td"); cell.append(box); row.append(cell);
        for(const value of [r.id,r.description,`${r.timestamp_ms} ms`,`${r.gain}x`,`${r.integration_ms} ms`,`${r.led_current_ma} mA`]) { const td=document.createElement("td"); td.textContent=String(value); row.append(td); }
        const td=document.createElement("td"), button=document.createElement("button"); button.textContent="VIEW"; button.className="button button-secondary";
        button.addEventListener("click",()=>action(async()=>{ try { const full=recordCache.get(r.id) ?? await transport.record(r.id); recordCache.set(r.id,full); if(!full||full.id!==r.id) throw new Error("RECORD_NOT_FOUND"); selected=r.id; histogram.update(full.sample,Number(full.adc_full_scale)); text("saved-title",`Saved record #${full.id} — ${full.description}`); text("saved-setup",`${full.gain}x · ${full.integration_ms} ms · LED ${full.led_current_ma} mA ${full.led_mode} · SMUX ${full.auto_smux} · AZ ${full.autozero} · period ${full.sample_period_ms} ms · ADC ${full.adc_full_scale} · uptime ${full.timestamp_ms} ms`); text("data-status","Registro cargado desde ESP32"); } catch(e) { fail(e); } }));
        td.append(button); row.append(td); return row;
      }));
      if(selected&&!records.some(r=>r.id===selected)) { selected=null; histogram.update(null); text("saved-title","Select a saved record"); text("saved-setup",""); }
      else if(selected) text("saved-title",`Last loaded record #${selected} — ${records.find(r=>r.id===selected).description}`);
      text("data-status","LittleFS disponible · más recientes primero");
    } catch(e) { fail(e); }
  }
  get("log-capture").addEventListener("click",()=>{
    frozen=latest; // Freeze before the first await; new notifications cannot change this reference.
    action(async()=>{
      try {
        const payload=new Uint8Array(4); new DataView(payload.buffer).setUint32(0,frozen.sequence,true);
        const ack=await transport.rpc(1,payload); pin=ack.request;
        get("capture-description").value="";
        text("capture-status",`Muestra #${frozen.sequence} · uptime ${frozen.timestampMs} ms${running()?"":" · adquisición parada: última muestra visible"}. Descripción opcional, máximo 80 bytes UTF-8.`);
        get("capture-dialog").showModal();
      } catch(e) { fail(e); }
    });
  });
  get("capture-cancel").addEventListener("click",()=>{ get("capture-dialog").close(); pin=0; frozen=null; });
  get("capture-save").addEventListener("click",()=>action(async()=>{
    try {
      const description=get("capture-description").value, bytes=new TextEncoder().encode(description);
      if(bytes.length>80||/[\x00-\x1f\x7f]/.test(description)) throw new Error("INVALID_DESCRIPTION: máximo 80 bytes, sin saltos de línea");
      text("capture-status","Guardando…"); get("capture-cancel").disabled=true;
      await transport.stage(bytes);
      const payload=new Uint8Array(6), v=new DataView(payload.buffer); v.setUint32(0,pin,true); v.setUint16(4,bytes.length,true);
      const ack=await transport.rpc(3,payload);
      recordCache.clear();
      pin=0; frozen=null; get("capture-dialog").close();
      const message=`Registro #${ack.view.getUint32(8,true)} guardado · ${ack.view.getUint16(12,true)} / 100`;
      text("message",message); await refresh(); text("data-status",message);
    } catch(e) { pin=0; get("capture-dialog").close(); fail(e); }
    finally { get("capture-cancel").disabled=false; }
  }));
  get("capture-dialog").addEventListener("cancel",event=>{ if(blocked) event.preventDefault(); else { pin=0; frozen=null; } });
  get("data-refresh").addEventListener("click",()=>action(refresh));
  get("data-cancel").addEventListener("click",()=>{ transport.cancelled=true; });
  get("data-download").addEventListener("click",()=>action(async()=>{
    try {
      const bytes=await transport.download();
      const url=URL.createObjectURL(new Blob([bytes],{type:"text/csv;charset=utf-8"}));
      const a=document.createElement("a"); a.href=url; a.download="data.csv"; a.click(); setTimeout(()=>URL.revokeObjectURL(url),1000);
      text("data-status",`CSV verificado: ${bytes.length} bytes`);
    } catch(e) { fail(e); }
  }));
  get("data-delete").addEventListener("click",()=>{
    const ids=checks(); if(!ids.length||!window.confirm(`¿Borrar ${ids.length} capturas? IDs: ${ids.join(", ")}`)) return;
    action(async()=>{ try {
      await transport.rpc(9); const bytes=new Uint8Array(ids.length*4), v=new DataView(bytes.buffer); ids.forEach((id,i)=>v.setUint32(i*4,id,true)); await transport.stage(bytes);
      const payload=new Uint8Array(2); new DataView(payload.buffer).setUint16(0,ids.length,true);
      const ack=await transport.rpc(7,payload); recordCache.clear(); if(ack.view.getUint16(10,true)!==ids.length) throw new Error("TRANSFER_FAILED");
      for(let offset=0;offset<bytes.length;offset+=12) {
        const p=new Uint8Array(8), view=new DataView(p.buffer); view.setUint32(0,offset,true); view.setUint32(4,ack.request,true);
        const receipt=await transport.rpc(10,p,offset/12);
        for(let i=0;i<Math.min(12,bytes.length-offset);++i) if(receipt.view.getUint8(8+i)!==bytes[offset+i]) throw new Error("TRANSFER_FAILED: DELETE receipt");
      }
      await refresh(); text("data-status",`Borrados IDs ${ids.join(", ")} · ${ack.view.getUint16(8,true)} / 100`);
    } catch(e) { fail(e); } });
  });
  return {controls, refresh, unavailable:message=>{text("data-status",message); controls();}, sample:s=>{latest=s; controls();}, clearSample:()=>{latest=null; controls();}, disconnect:()=>{ recordCache.clear(); transport.fast=undefined; latest=frozen=null; pin=0; get("capture-dialog").close(); text("data-status","DISCONNECTED · lista anterior, refrescar al reconectar"); if(selected) text("saved-title",`Last loaded record #${selected} · disconnected`); controls(); }};
}
