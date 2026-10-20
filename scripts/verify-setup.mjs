import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { runInThisContext } from "node:vm";
import { createTestDOM } from "./web-test-dom.mjs";
import { physicalReadings } from "../web/src/ui.mjs";
import { UUID, COMMAND, decodeStatus, decodeConfig, encodeApply, SampleAssembler } from "../web/src/protocol.mjs";

const html = readFileSync(new URL("../web/dist/index.html", import.meta.url), "utf8");
const script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
const defaults = () => ({ gainCode: 7, integrationMs: 50.04, ledCurrentMa: 10, ledEnabled: false, smux: 18, autozero: 1, intervalMs: 500, adcFullScale: 18000, revision: 1, valid: true });
function configPacket(c) {
  const bytes = new Uint8Array(20);
  const v = new DataView(bytes.buffer);
  bytes.set([3, c.gainCode, c.smux, c.autozero, c.ledAuto ? 2 : Number(c.ledEnabled), Number(c.valid)]);
  v.setUint16(6, c.ledCurrentMa, true);
  v.setUint32(8, Math.round(c.integrationMs * 1000), true);
  v.setUint32(12, c.intervalMs, true);
  v.setUint16(16, c.adcFullScale, true);
  v.setUint16(18, c.revision, true);
  return v;
}
function samplePackets(count, sequence = 0x12345678) {
  const data = new Uint8Array(48);
  const view = new DataView(data.buffer);
  data.set([1, 7, 1, count]);
  view.setUint32(4, sequence, true);
  view.setUint32(8, 0xabcdef01, true);
  for (let i = 0; i < 18; ++i) view.setUint16(12 + i * 2, i * 1000, true);
  return Array.from({ length: 4 }, (_, i) => {
    const packet = new Uint8Array(20);
    packet.set([1, i, 4, 0]);
    new DataView(packet.buffer).setUint32(4, sequence, true);
    packet.set(data.slice(i * 12, i * 12 + 12), 8);
    return new DataView(packet.buffer);
  });
}
class Characteristic extends EventTarget {
  async startNotifications() { return this; }
  notify(value) { this.value = value; this.dispatchEvent(new Event("characteristicvaluechanged")); }
}
function setup({ ready = true, rejectApply = 0, failSave = false, holdApply = false, subscriptionFailure = false, unsupported = false, hash = "" } = {}) {
  const dom = createTestDOM(html, hash);
  const el = dom.el;
  const state = { ready, acquiring: false, counter: 0, command: 0, result: 0, error: ready ? 0 : 1, config: defaults(), release: null };
  state.config.valid = ready;
  const status = new Characteristic();
  const config = new Characteristic();
  const sample = new Characteristic();
  const command = new Characteristic();
  const writes = [];
  function statusValue() {
    const bytes = new Uint8Array(20);
    const view = new DataView(bytes.buffer);
    bytes.set([1, Number(state.ready), Number(state.acquiring), state.error, state.command, state.result]);
    view.setUint16(6, state.counter, true);
    view.setUint32(16, state.config.intervalMs, true);
    return view;
  }
  status.readValue = async () => statusValue();
  config.readValue = async () => configPacket(state.config);
  if (subscriptionFailure) sample.startNotifications = async () => { throw new Error("SUBSCRIPTION_FAILED"); };
  command.writeValueWithResponse = async bytes => {
    writes.push(Uint8Array.from(bytes));
    const isApply = bytes[0] === 0xa4;
    const name = isApply ? "APPLY_CONFIG" : new TextDecoder().decode(bytes);
    const finish = () => {
      state.command = COMMAND[name];
      ++state.counter;
      state.result = state.error = 0;
      if (isApply) {
        if (rejectApply) {
          state.result = rejectApply === 6 ? 5 : 4;
          state.error = rejectApply;
          if (rejectApply === 6) {
            state.ready = state.config.valid = state.acquiring = false;
          }
        } else {
          const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
          state.config = { ...state.config, gainCode: bytes[2], smux: bytes[3], autozero: bytes[4], ledEnabled: bytes[5] === 1, ledAuto: bytes[5] === 2,
            ledCurrentMa: v.getUint16(6, true), integrationMs: 50.001, intervalMs: v.getUint32(12, true), adcFullScale: 17986, revision: state.config.revision + 1 };
          if (failSave) { state.result = 6; state.error = 24; }
        }
      }
      if (name === "START") state.acquiring = state.ready;
      if (name === "STOP") state.acquiring = false;
      if (name === "GET_CONFIG" && !state.ready) { state.result = 2; state.error = 1; }
      status.notify(statusValue());
    };
    if (holdApply && isApply) state.release = finish;
    else finish();
  };
  const chars = { [UUID.status]: status, [UUID.command]: command, [UUID.sample]: sample, [UUID.config]: config };
  const device = new EventTarget();
  device.name = "TCS3448";
  device.gatt = {
    connected: false,
    async connect() { this.connected = true; return this; },
    async getPrimaryService(uuid) { assert.equal(uuid, UUID.service); return { getCharacteristic: async id => chars[id] }; },
    disconnect() {
      if (!this.connected) return;
      this.connected = false;
      state.acquiring = false;
      state.config.ledEnabled = false;
      ++state.config.revision;
      device.dispatchEvent(new Event("gattserverdisconnected"));
    },
  };
  Object.defineProperty(globalThis, "navigator", { configurable: true, value: unsupported ? {} : {
    bluetooth: { requestDevice: async () => device },
  } });
  globalThis.window = dom.window;
  globalThis.document = dom.document;
  globalThis.Option = dom.Option;
  runInThisContext(script, { filename: "generated-index.html" });
  return { el, state, device, writes, sample, window: dom.window, status, statusValue };
}

test("APPLY_CONFIG fits a minimum-MTU write and carries LED 258 mA without truncation", () => {
  const packet = encodeApply({ ...defaults(), ledCurrentMa: 258, ledEnabled: true });
  assert.equal(packet.length, 20);
  assert.deepEqual([...packet.slice(0, 8)], [0xa4, 2, 7, 18, 1, 1, 2, 1]);
  assert.equal(new DataView(packet.buffer).getUint32(8, true), 50040);
  assert.deepEqual([...packet.slice(16)], [0, 0, 0, 0]);
});

test("LED AUTO uses version 3 without changing packet size; legacy mode 2 is rejected", () => {
  const packet = encodeApply({ ...defaults(), ledAuto: true });
  assert.equal(packet.length, 20);
  assert.deepEqual([...packet.slice(0, 6)], [0xa4, 3, 7, 18, 1, 2]);
  const readback = configPacket({ ...defaults(), ledAuto: true });
  assert.equal(decodeConfig(readback).ledAuto, true);
  assert.equal(decodeConfig(readback).ledEnabled, false);
  readback.setUint8(0, 2);
  assert.throws(() => decodeConfig(readback), /invalida/);
});
test("rejects invalid requests before sending, including odd LED current and NaN", () => {
  for (const [field, value, expected] of [
    ["gainCode", 13, "INVALID_GAIN"], ["gainCode", -1, "INVALID_GAIN"],
    ["integrationMs", NaN, "INVALID_INTEGRATION"], ["integrationMs", Infinity, "INVALID_INTEGRATION"], ["integrationMs", 0, "INVALID_INTEGRATION"],
    ["ledCurrentMa", 3, "INVALID_LED_CURRENT"], ["ledCurrentMa", 5, "INVALID_LED_CURRENT"], ["ledCurrentMa", 21, "INVALID_LED_CURRENT"], ["ledCurrentMa", 260, "INVALID_LED_CURRENT"],
    ["ledEnabled", 2, "INVALID_CONFIG_PAYLOAD"], ["smux", 7, "INVALID_SMUX_MODE"], ["autozero", 3, "INVALID_AUTOZERO"],
    ["intervalMs", 0, "INVALID_SAMPLE_PERIOD"], ["intervalMs", 1.5, "INVALID_SAMPLE_PERIOD"], ["intervalMs", 100, "SAMPLE_PERIOD_TOO_SHORT"],
  ]) assert.throws(() => encodeApply({ ...defaults(), [field]: value }), new RegExp(expected));
  for (const ledCurrentMa of [4, 6, 20, 256, 258]) assert.equal(encodeApply({ ...defaults(), ledCurrentMa }).length, 20);
});
test("active config v2 decodes readback, ADC full scale and revision", () => {
  const c = decodeConfig(configPacket({ ...defaults(), ledCurrentMa: 258, revision: 65535 }));
  assert.equal(c.ledCurrentMa, 258);
  assert.equal(c.integrationMs, 50.04);
  assert.equal(c.adcFullScale, 18000);
  assert.equal(c.revision, 65535);
  assert.equal(c.gain, 64);
  const wrong = configPacket(defaults()); wrong.setUint8(0, 1);
  assert.throws(() => decodeConfig(wrong), /Version/);
});
test("sample transport remains v1 and handles exactly 6, 12 or 18 channels", () => {
  for (const count of [6, 12, 18]) {
    const assembler = new SampleAssembler();
    const packets = samplePackets(count);
    for (let i = 0; i < 3; ++i) assert.equal(assembler.push(packets[i], i), null);
    const sample = assembler.push(packets[3], 3);
    assert.equal(sample.channelCount, count);
    assert.equal(sample.channels.length, count);
    assert.equal(sample.channels.at(-1)[1], (count - 1) * 1000);
    assert.equal(sample.sequence, 0x12345678);
    assert.equal(sample.timestampMs, 0xabcdef01);
    assert.equal(sample.saturated, true);
  }
});
test("sample assembly discards gaps, mixed sequences, invalid counts, stale and partial frames", () => {
  const a = new SampleAssembler();
  const packets = samplePackets(18);
  a.push(packets[0], 0);
  assert.equal(a.push(packets[2], 1), null);
  assert.equal(a.push(packets[3], 2), null);
  a.push(packets[0], 0);
  assert.equal(a.push(packets[1], 2001), null);
  a.push(packets[0]);
  assert.equal(a.push(samplePackets(18, 3)[1]), null);
  let sample;
  for (const p of samplePackets(6, 0)) sample = a.push(p);
  assert.equal(sample.sequence, 0);
  assert.throws(() => { for (const p of samplePackets(7)) a.push(p); }, /Muestra/);
  a.push(packets[0]); a.reset();
  assert.equal(a.push(packets[1]), null);
});
test("Connect obtains firmware config and fills requested values without JS defaults", async () => {
  const { el, device, writes } = setup();
  assert.equal(el.integration.value, "");
  await el.connect.click();
  assert.deepEqual(writes.map(b => new TextDecoder().decode(b)), ["GET_STATUS", "GET_CONFIG"]);
  assert.equal(el.integration.value, "50.04");
  assert.match(el.config.textContent, /64x/);
  assert.equal(el.apply.disabled, false);
  device.gatt.disconnect();
});
test("APPLY shows no optimistic success, then displays actual firmware readback and resumes RUNNING", async () => {
  const { el, state, device, sample } = setup({ holdApply: true });
  await el.connect.click();
  await el.start.click();
  el.integration.value = "50";
  el.gain.value = "8";
  el.smux.value = "6";
  el["led-current"].value = "258";
  el["led-enabled"].checked = true;
  const before = el.config.textContent;
  const operation = el.apply.click();
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(el.config.textContent, before);
  assert.match(el["config-state"].textContent, /APPLYING/);
  assert.equal(el.apply.disabled, true);
  state.release();
  await operation;
  assert.match(el.config.textContent, /128x/);
  assert.match(el.config.textContent, /50.001 ms/);
  assert.equal(el.integration.value, "50.001");
  assert.match(el.config.textContent, /258 mA/);
  assert.match(el["config-state"].textContent, /CONFIGURATION VALID/);
  assert.match(el.status.textContent, /RUNNING/);
  for (const packet of samplePackets(6)) sample.notify(packet);
  assert.match(el.sample.textContent, /6 channels/);
  assert.doesNotMatch(el.sample.textContent, /F1 /);
  await el.start.click();
  assert.match(el.status.textContent, /STOPPED/);
  assert.equal(state.config.ledEnabled, true);
  device.gatt.disconnect();
});
test("client validation leaves active config and firmware untouched", async () => {
  const { el, writes, device } = setup();
  await el.connect.click();
  const before = el.config.textContent;
  el["led-current"].value = "21";
  await el.apply.click();
  assert.equal(writes.length, 2);
  assert.equal(el.config.textContent, before);
  assert.match(el.message.textContent, /even value/);
  assert.match(el["debug-log"].textContent, /INVALID_LED_CURRENT/);
  device.gatt.disconnect();
});
test("firmware timing rejection keeps old active config and requested edits", async () => {
  const { el, state, device } = setup({ rejectApply: 22 });
  await el.connect.click();
  await el.start.click();
  const before = el.config.textContent;
  el.period.value = "160"; // Integration fits superficially; firmware includes AZ and overhead.
  await el.apply.click();
  assert.equal(el.config.textContent, before);
  assert.equal(el.period.value, "160");
  assert.equal(state.acquiring, true);
  assert.match(el.message.textContent, /too short/);
  assert.match(el["debug-log"].textContent, /SAMPLE_PERIOD_TOO_SHORT/);
  assert.doesNotMatch(el["config-state"].textContent, /CONFIGURATION VALID/);
  device.gatt.disconnect();
});
test("unconfirmed rollback invalidates active config and disables START/APPLY", async () => {
  const { el, device } = setup({ rejectApply: 6 });
  await el.connect.click();
  await el.apply.click();
  assert.match(el.message.textContent, /could not apply/);
  assert.equal(el["config-state"].dataset.state, "warning");
  assert.equal(el.start.disabled, true);
  assert.equal(el.apply.disabled, true);
  assert.equal(el.refresh.disabled, false);
  device.gatt.disconnect();
});
test("reconnection refetches actual config and stopped state instead of old form data", async () => {
  const { el, state, device } = setup();
  await el.connect.click();
  await el.start.click();
  device.gatt.disconnect();
  assert.equal(el.integration.value, "");
  state.config = { ...defaults(), gainCode: 2, integrationMs: 100, intervalMs: 1000, ledEnabled: false };
  await el.connect.click();
  assert.equal(el.gain.value, "2");
  assert.equal(el.integration.value, "100");
  assert.equal(el["led-enabled"].checked, false);
  assert.match(el.status.textContent, /STOPPED/);
  device.gatt.disconnect();
});
test("sensor absent and subscription failure stay recoverable", async () => {
  let mock = setup({ ready: false });
  await mock.el.connect.click();
  assert.equal(mock.device.gatt.connected, true);
  assert.equal(mock.el.apply.disabled, true);
  assert.equal(mock.el["config-state"].textContent, "NOT AVAILABLE");
  mock.device.gatt.disconnect();
  mock = setup({ subscriptionFailure: true });
  await mock.el.connect.click();
  assert.equal(mock.device.gatt.connected, false);
  assert.equal(mock.el.connect.disabled, false);
  assert.match(mock.el["debug-log"].textContent, /SUBSCRIPTION_FAILED/);
});
test("generated local HTML is standalone and has no hardware/register fields", () => {
  assert.doesNotMatch(html, /<script[^>]+(?:src=|type=["']module)/i);
  assert.doesNotMatch(html, /<link[^>]+href=["'](?!data:)/i);
  assert.doesNotMatch(script, /^\s*(?:import|export)\s/m);
  assert.doesNotMatch(html, /ATIME|ASTEP|CFG20|Adafruit|0x59/);
  assert.match(html, /Sensor configuration/);
  assert.match(html, /Active configuration/);
  const { el } = setup({ unsupported: true });
  assert.equal(el.connect.disabled, true);
});

test("Measurement/Setup navigation retains BLE, acquisition, subscriptions and requested edits", async () => {
  const { el, device, writes, sample, window } = setup();
  await el.connect.click();
  await el.start.click();
  const before = writes.length;
  el.integration.value = "123";
  await el["nav-setup"].click();
  assert.equal(window.location.hash, "#/setup");
  assert.equal(el["setup-view"].hidden, false);
  assert.equal(el["measurement-view"].hidden, true);
  assert.equal(el["nav-setup"].getAttribute("aria-current"), "page");
  assert.equal(device.gatt.connected, true);
  assert.equal(el.connection.textContent, "CONNECTED");
  for (const packet of samplePackets(18)) sample.notify(packet);
  assert.match(el.sample.textContent, /#305419896/);
  assert.equal(el["sample-readout"].children.length, 2);
  await el["nav-measurement"].click();
  assert.equal(el.integration.value, "123");
  assert.equal(writes.length, before);
  assert.equal(el["acquisition-state"].textContent, "RUNNING");
  assert.equal(el["summary-integration"].textContent, "50.040 ms");
  assert.doesNotMatch(el["sample-readout"].textContent, /VIS_TL_|VIS_BR_/);
  assert.match(el.sample.textContent, /VIS_TL_2/);
  device.gatt.disconnect();
  assert.equal(el.connection.textContent, "DISCONNECTED");
  assert.match(el.sample.textContent, /#305419896/);
});
test("physical display uses cycle-0 VIS without averaging and distinguishes zero from unavailable", () => {
  const display = physicalReadings({ channels: [["FZ", 0], ["VIS_TL_0", 7], ["VIS_TL_1", 70], ["VIS_TL_2", 700], ["VIS_BR_0", 12]] });
  assert.equal(display.length, 14);
  assert.equal(display.find(c => c.name === "VIS TL").value, 7);
  assert.equal(display.find(c => c.name === "VIS BR").value, 12);
  assert.equal(display.find(c => c.name === "FZ").value, 0);
  assert.equal(display.find(c => c.name === "F1").value, null);
});

test("AUTO LED is applied and read back as a firmware mode, retained across START/STOP/reconnect", async () => {
  const { el, state, device, writes } = setup();
  await el.connect.click();
  el["led-auto"].checked = true;
  await el.apply.click();
  assert.equal(writes.at(-1)[1], 3);
  assert.equal(writes.at(-1)[5], 2);
  assert.equal(state.config.ledAuto, true);
  assert.equal(el["led-auto"].checked, true);
  assert.equal(el["led-enabled"].disabled, true);
  assert.match(el.config.textContent, /AUTO/);
  await el.start.click();
  await el.start.click();
  assert.equal(state.config.ledAuto, true);
  device.gatt.disconnect();
  await el.connect.click();
  assert.equal(el["led-auto"].checked, true);
  device.gatt.disconnect();
});

test("VIS levels use ADC full scale, TL above BR, keeping last levels on disconnect", async () => {
  const { el, device, sample } = setup();
  await el.connect.click();
  await el.start.click();
  for (const packet of samplePackets(18)) sample.notify(packet);
  const [tl, br] = el["sample-readout"].children;
  assert.match(tl.textContent, /VIS TL4000 counts/);
  assert.match(br.textContent, /VIS BR5000 counts/);
  assert.equal(Number(tl.children[2].children[1].getAttribute("width")), 4000 / 18000 * 100);
  assert.equal(Number(br.children[2].children[1].getAttribute("width")), 5000 / 18000 * 100);
  device.gatt.disconnect();
  assert.match(tl.textContent, /4000 counts/);
});

test("shared connection and measurement buttons follow confirmed state across views", async () => {
  const { el, state, status, statusValue, device, writes } = setup();
  assert.equal(el.connect.textContent, "CONNECT");
  assert.equal(el.start.textContent, "START");
  assert.equal(el.start.disabled, true);
  await el.connect.click();
  assert.equal(el.connect.textContent, "DISCONNECT");
  assert.equal(el.connect.disabled, false);
  await el.start.click();
  assert.equal(el.start.textContent, "STOP");
  assert.equal(el.start.disabled, false);
  state.acquiring = false;
  status.notify(statusValue());
  assert.equal(el.start.textContent, "START");
  await el.start.click();
  await el["nav-setup"].click();
  await el.connect.click();
  assert.equal(new TextDecoder().decode(writes.at(-1)), "STOP");
  assert.equal(device.gatt.connected, false);
  assert.equal(el.connect.textContent, "CONNECT");
  assert.equal(el.start.textContent, "START");
  assert.equal(el.start.disabled, true);
  assert.equal(el["setup-view"].hidden, false);
  assert.doesNotMatch(html, /id="(?:disconnect|stop)"|class="card device-card"/);
  assert.match(html, /<aside class="sidebar">[\s\S]*?id="nav-measurement"[\s\S]*?<\/aside>/);
});
test("measurement settings show confirmed integration, sample gain and LED current without the old summary", async () => {
  const { el, device, sample } = setup();
  await el.connect.click();
  await el.start.click();
  const packets = samplePackets(6, 1);
  packets[0].setUint8(10, 0); // sample header byte 2: no saturation.
  for (const packet of packets) sample.notify(packet);
  assert.equal(el["summary-gain"].textContent, "64x");
  assert.equal(el["summary-integration"].textContent, "50.040 ms");
  assert.equal(el["summary-led-current"].textContent, "10 mA");
  assert.doesNotMatch(html, /id="summary-sample"|id="sample-state"|class="sample-summary"/);
  await el.start.click();
  assert.equal(el["acquisition-state"].textContent, "IDLE");
  device.gatt.disconnect();
});
test("view markup separates setup and debug and removes the measurement heading", () => {
  const measurement = html.slice(html.indexOf('<div id="measurement-view"'), html.indexOf('<div id="setup-view"'));
  assert.doesNotMatch(measurement, /id="(?:gain|integration|led-current|apply|setup)"/);
  assert.match(html, /href="#\/measurement"/);
  assert.match(html, /href="#\/setup"/);
  assert.match(html, /href="#\/debug"/);
  assert.match(html, /id="debug-view" hidden/);
  assert.doesNotMatch(measurement, /measurement-title|debug-log|Technical status|Raw SMUX data/);
  assert.doesNotMatch(html, /<details/);
  assert.doesNotMatch(html, /HITO\s*4/i);
  assert.doesNotMatch(html, /Chart\.js|<canvas|<svg/);
});

test("NVS failure reports applied but not saved, retains new readback and clears old sample", async () => {
  const { el, device, state, sample } = setup({failSave:true});
  await el.connect.click(); await el.start.click();
  for(const packet of samplePackets(18,1)) sample.notify(packet);
  el.gain.value="8";
  await el.apply.click();
  assert.equal(state.config.gainCode,8);
  assert.match(el.config.textContent,/128x/);
  assert.match(el["config-state"].textContent,/APPLIED \/ NOT SAVED/);
  assert.match(el.message.textContent,/could not be saved for restart/);
  assert.equal(el.start.disabled,false);
  assert.equal(el.apply.disabled,false);
  device.gatt.disconnect();
});

test("Data navigation preserves the same BLE session and continues receiving samples", async () => {
  const { el, device, state, writes, sample } = setup({hash:"#/data"});
  assert.equal(el["data-view"].hidden,false);
  await el.connect.click(); await el.start.click();
  const before=writes.length;
  await el["nav-measurement"].click(); await el["nav-data"].click();
  assert.equal(el["nav-data"].getAttribute("aria-current"),"page");
  assert.equal(device.gatt.connected,true); assert.equal(state.acquiring,true); assert.equal(writes.length,before);
  for(const packet of samplePackets(18,1)) sample.notify(packet);
  assert.match(el["summary-gain"].textContent,/64x/);
  // This fixture exposes the original four characteristics, without CAPTURES.
  assert.equal(el["log-capture"].disabled,true);
  device.gatt.disconnect(); assert.equal(el["log-capture"].disabled,true);
});

test("Debug navigation and direct links preserve acquisition and keep receiving samples", async () => {
  const { el, device, writes, sample, window } = setup({ hash: "#/debug" });
  assert.equal(el["debug-view"].hidden, false);
  assert.equal(el["measurement-view"].hidden, true);
  assert.equal(el["setup-view"].hidden, true);
  await el.connect.click();
  await el["nav-measurement"].click();
  await el.start.click();
  const before = writes.length;
  await el["nav-debug"].click();
  assert.equal(window.location.hash, "#/debug");
  assert.equal(el["nav-debug"].getAttribute("aria-current"), "page");
  assert.equal(el["measurement-view"].hidden, true);
  assert.equal(el["setup-view"].hidden, true);
  for (const packet of samplePackets(18)) sample.notify(packet);
  assert.match(el.sample.textContent, /VIS_TL_2/);
  assert.match(el.sample.textContent, /#305419896/);
  await el["nav-setup"].click();
  assert.equal(el["debug-view"].hidden, true);
  window.location.hash = "#/debug";
  window.dispatchEvent(new Event("hashchange"));
  assert.equal(el["debug-view"].hidden, false);
  assert.equal(writes.length, before);
  assert.equal(device.gatt.connected, true);
  assert.equal(el.start.textContent, "STOP");
  device.gatt.disconnect();
});

test("direct Setup links and hash history preserve the same session and skip links do not change view", async () => {
  const { el, device, writes, window } = setup({ hash: "#/setup" });
  assert.equal(el["setup-view"].hidden, false);
  assert.equal(el["measurement-view"].hidden, true);
  assert.equal(writes.length, 0);
  await el["setup-connect-link"].click();
  await el.connect.click();
  const before = writes.length;
  window.location.hash = "#/setup";
  window.dispatchEvent(new Event("hashchange"));
  assert.equal(el["setup-view"].hidden, false);
  window.location.hash = "#main-content";
  window.dispatchEvent(new Event("hashchange"));
  assert.equal(el["setup-view"].hidden, false);
  assert.equal(device.gatt.connected, true);
  assert.equal(writes.length, before);
  assert.doesNotMatch(html, /\uFEFF:root/);
  assert.doesNotMatch(html, /__fixtureEmit/);
  device.gatt.disconnect();
});
