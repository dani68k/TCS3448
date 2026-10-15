import test from "node:test";
import { readFileSync } from "node:fs";
import { runInThisContext } from "node:vm";
import assert from "node:assert/strict";
import { UUID, COMMAND } from "../web/src/protocol.mjs";

class Element {
  disabled = false;
  textContent = "";
  handlers = new Map();
  addEventListener(name, callback) { this.handlers.set(name, callback); }
  async click() {
    if (!this.disabled) await this.handlers.get("click")?.();
  }
}
class Characteristic extends EventTarget {
  constructor(value) { super(); this.value = value; }
  async readValue() { return this.value; }
  async startNotifications() { return this; }
  notify(value) { this.value = value; this.dispatchEvent(new Event("characteristicvaluechanged")); }
}
const html = readFileSync(new URL("../web/dist/index.html", import.meta.url), "utf8");
const bundledScript = html.match(/<script>([\s\S]*?)<\/script>/)[1];
async function setup({ ready = true, failSubscription = false, unsupported = false } = {}) {
  const ids = ["connect", "disconnect", "start", "stop", "refresh", "connection", "support", "status", "config", "sample", "received", "message"];
  const elements = Object.fromEntries(ids.map(id => [id, new Element()]));
  const state = { ready, acquiring: false, counter: 0, command: 0, result: 0 };
  function statusValue() {
    const bytes = new Uint8Array(20);
    bytes.set([1, Number(state.ready), Number(state.acquiring), state.ready ? 0 : 1, state.command, state.result]);
    const value = new DataView(bytes.buffer);
    value.setUint16(6, state.counter, true);
    value.setUint32(16, 500, true);
    return value;
  }
  const status = new Characteristic(statusValue());
  status.readValue = async () => statusValue();
  const config = new Characteristic(new DataView(Uint8Array.from([1, 7, 29, 10, 0x57, 2, 0xf4, 1, 0x78, 0xc3, 0, 0, 0x80, 0x1a, 6, 0, 4, 5, 0, 0]).buffer));
  const sample = new Characteristic();
  if (failSubscription) sample.startNotifications = async () => { throw new Error("subscription failed"); };
  const writes = [];
  const command = new Characteristic();
  command.writeValueWithResponse = async bytes => {
    const text = new TextDecoder().decode(bytes);
    writes.push(text);
    state.command = COMMAND[text];
    ++state.counter;
    state.result = 0;
    if (text === "START") {
      if (state.ready) state.acquiring = true;
      else state.result = 2;
    }
    if (text === "STOP") state.acquiring = false;
    status.notify(statusValue());
  };
  const chars = { [UUID.status]: status, [UUID.command]: command, [UUID.sample]: sample, [UUID.config]: config };
  const device = new EventTarget();
  device.name = "ATLANTES-TCS3448";
  device.gatt = {
    connected: false,
    async connect() { this.connected = true; return this; },
    async getPrimaryService(uuid) {
      assert.equal(uuid, UUID.service);
      return { getCharacteristic: async uuid => chars[uuid] };
    },
    disconnect() {
      if (!this.connected) return;
      this.connected = false;
      state.acquiring = false;
      device.dispatchEvent(new Event("gattserverdisconnected"));
    },
  };
  Object.defineProperty(globalThis, "navigator", { configurable: true, value: unsupported ? {} : {
    bluetooth: { requestDevice: async options => {
      assert.deepEqual(options.filters, [{ services: [UUID.service] }]);
      return device;
    } },
  } });
  globalThis.window = { isSecureContext: true };
  globalThis.document = { getElementById: id => elements[id] };
  runInThisContext(bundledScript, { filename: "index.html" });
  return { el: elements, device, state, writes, status, statusValue };
}

test("connect, configuration, START, STOP and reconnect work through application acknowledgments", async () => {
  const { el, device, state, writes } = await setup();
  assert.equal(el.start.disabled, true);
  await el.connect.click();
  assert.deepEqual(writes, ["GET_STATUS", "GET_CONFIG"]);
  assert.match(el.connection.textContent, /Conectado/);
  assert.match(el.config.textContent, /Ganancia: 64x/);
  assert.match(el.config.textContent, /50.04 ms/);
  assert.equal(el.start.disabled, false);
  await el.start.click();
  assert.equal(state.acquiring, true);
  assert.equal(el.start.disabled, true);
  assert.match(el.status.textContent, /Activa/);
  await el.stop.click();
  assert.equal(state.acquiring, false);
  await el.start.click();
  await el.disconnect.click();
  assert.equal(writes.at(-1), "STOP");
  assert.equal(device.gatt.connected, false);
  assert.equal(el.start.disabled, true);
  assert.equal(el.connect.disabled, false);
  await el.connect.click();
  assert.equal(el.start.disabled, false);
  assert.equal(state.acquiring, false);
  device.gatt.disconnect();
});
test("unexpected disconnect clears controls and old configuration", async () => {
  const { el, device } = await setup();
  await el.connect.click();
  await el.start.click();
  device.gatt.disconnect();
  assert.equal(el.start.disabled, true);
  assert.equal(el.stop.disabled, true);
  assert.equal(el.connect.disabled, false);
  assert.match(el.config.textContent, /Conecta/);
});
test("sensor initialization failure still permits status/config queries", async () => {
  const { el, device, writes } = await setup({ ready: false });
  await el.connect.click();
  assert.deepEqual(writes, ["GET_STATUS", "GET_CONFIG"]);
  assert.equal(device.gatt.connected, true);
  assert.equal(el.start.disabled, true);
  assert.match(el.config.textContent, /sensor sin inicializar/);
  assert.equal(el.refresh.disabled, false);
  device.gatt.disconnect();
});
test("partial connection failure disconnects and permits retry", async () => {
  const { el, device } = await setup({ failSubscription: true });
  await el.connect.click();
  assert.equal(device.gatt.connected, false);
  assert.equal(el.connect.disabled, false);
  assert.equal(el.start.disabled, true);
  assert.match(el.message.textContent, /subscription failed/);
});
test("sensor errors are shown and do not leave buttons locked", async () => {
  const { el, device, state } = await setup();
  await el.connect.click();
  state.ready = false;
  await el.start.click();
  assert.match(el.message.textContent, /Error del sensor/);
  assert.equal(el.disconnect.disabled, false);
  assert.equal(el.start.disabled, true);
  device.gatt.disconnect();
});
test("unsupported browser disables Connect", async () => {
  const { el } = await setup({ unsupported: true });
  assert.equal(el.connect.disabled, true);
  assert.match(el.support.textContent, /no est/);
});

test("local HTML is self-contained and uses a classic script", () => {
  assert.doesNotMatch(html, /<script[^>]+(?:src=|type=["']module)/i);
  assert.doesNotMatch(html, /<link[^>]+href=/i);
  assert.doesNotMatch(bundledScript, /^\s*(?:import|export)\s/m);
  assert.match(html, /<style>/);
  assert.equal((html.match(/<script>/g) || []).length, 1);
});
