import { UUID, COMMAND, ERRORS, decodeStatus, decodeConfig, SampleAssembler } from "./protocol.mjs";

const el = Object.fromEntries(["connect", "disconnect", "start", "stop", "refresh", "connection", "support", "status", "config", "sample", "received", "message"].map(id => [id, document.getElementById(id)]));
const assembler = new SampleAssembler();
let device = null;
let characteristics = null;
let latestStatus = null;
let busy = false;
let received = 0;
let lastSequence = null;
let missed = 0;
const supported = !!navigator.bluetooth && window.isSecureContext;

function controls() {
  const linked = !!characteristics && !!device?.gatt?.connected;
  el.connect.disabled = busy || linked || !supported;
  el.disconnect.disabled = busy || !linked;
  el.start.disabled = busy || !linked || !latestStatus?.ready || !!latestStatus?.acquiring;
  el.stop.disabled = busy || !linked || !latestStatus?.ready;
  el.refresh.disabled = busy || !linked;
}
function showError(error) { el.message.textContent = error.message || String(error); }
function showStatus(value) {
  latestStatus = decodeStatus(value);
  const s = latestStatus;
  el.status.textContent = [
    `Sensor: ${s.ready ? "Inicializado" : "No disponible"}`,
    `Adquisición: ${s.acquiring ? "Activa" : "Detenida"}`,
    `Error: ${ERRORS[s.error] ?? s.error}`,
    `Muestras adquiridas: ${s.sampleCount}`,
    `Periodo solicitado: ${s.intervalMs} ms`,
    `Tiempo desde arranque: ${(s.uptimeMs / 1000).toFixed(1)} s`,
  ].join("\n");
  controls();
  return s;
}
function showConfig(value) {
  const c = decodeConfig(value);
  el.config.textContent = [
    latestStatus?.ready ? "Aplicada al sensor" : "Valores previstos; sensor sin inicializar",
    `Ganancia: ${c.gain}x`, `ATIME / ASTEP: ${c.atime} / ${c.astep}`,
    `Integración por ciclo: ${c.integrationMs.toFixed(2)} ms`,
    `Periodo solicitado: ${c.intervalMs} ms`, `Corriente LED: ${c.ledCurrentMa} mA`,
    `I2C: ${c.i2cHz} Hz · SDA ${c.sda} · SCL ${c.scl}`,
  ].join("\n");
}
function onStatus(event) {
  try { showStatus(event.target.value); } catch (error) { showError(error); }
}
function onSample(event) {
  try {
    const sample = assembler.push(event.target.value);
    if (!sample) return;
    if (lastSequence !== null) {
      const delta = (sample.sequence - lastSequence) >>> 0;
      if (delta === 0 || delta > 0x7fffffff) return;
      missed += delta - 1;
    }
    lastSequence = sample.sequence;
    ++received;
    el.sample.textContent = [
      `Muestra #${sample.sequence} · ESP32 ${sample.timestampMs} ms`,
      `Ganancia: ${sample.gain}x · Saturación: ${sample.saturated ? "SÍ" : "No"}`, "",
      ...sample.channels.map(([name, value]) => `${name.padEnd(10)} ${String(value).padStart(5)}`),
    ].join("\n");
    el.received.textContent = `${received} muestras completas recibidas · ${missed} muestras omitidas entre las recibidas`;
  } catch (error) { showError(error); }
}
function clearConnection() {
  if (characteristics) {
    characteristics.status.removeEventListener("characteristicvaluechanged", onStatus);
    characteristics.sample.removeEventListener("characteristicvaluechanged", onSample);
  }
  characteristics = null;
  latestStatus = null;
  assembler.reset();
  lastSequence = null;
  el.connection.textContent = "Desconectado";
  el.status.textContent = "Sin conexión; adquisición detenida en el dispositivo al detectar la desconexión.";
  el.config.textContent = "Conecta el dispositivo para consultar la configuración.";
  el.sample.textContent = "Sin muestra de la conexión actual.";
  controls();
}

async function sendCommand(name) {
  const current = characteristics;
  if (!current || !device?.gatt?.connected) throw new Error("Dispositivo desconectado");
  // Poll for application-level completion; a GATT write acknowledgment only accepts the write.
  const before = decodeStatus(await current.status.readValue()).counter;
  await current.command.writeValueWithResponse(new TextEncoder().encode(name));
  const deadline = Date.now() + 3500;
  while (Date.now() < deadline) {
    if (characteristics !== current || !device?.gatt?.connected) throw new Error("Conexión perdida");
    const status = showStatus(await current.status.readValue());
    if (status.counter !== before && (status.command === COMMAND[name] || status.command === 255)) {
      if (status.result !== 0) {
        const results = ["OK", "Comando desconocido", "Error del sensor", "Cola de comandos llena"];
        throw new Error(`${name}: ${results[status.result] ?? status.result} (${ERRORS[status.error] ?? status.error})`);
      }
      if (name === "GET_CONFIG") showConfig(await current.config.readValue());
      if (name === "STOP") assembler.reset();
      return;
    }
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  throw new Error(`${name}: no se recibió confirmación del dispositivo`);
}
async function action(operation) {
  if (busy) return;
  busy = true;
  el.message.textContent = "";
  controls();
  try { await operation(); } catch (error) { showError(error); }
  finally { busy = false; controls(); }
}
el.connect.addEventListener("click", () => action(async () => {
  try {
    device?.removeEventListener("gattserverdisconnected", clearConnection);
    device = await navigator.bluetooth.requestDevice({ filters: [{ services: [UUID.service] }] });
    device.addEventListener("gattserverdisconnected", clearConnection);
    el.connection.textContent = "Conectando…";
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService(UUID.service);
    // Sequential GATT operations avoid overlapping requests on browser stacks.
    const status = await service.getCharacteristic(UUID.status);
    const command = await service.getCharacteristic(UUID.command);
    const sample = await service.getCharacteristic(UUID.sample);
    const config = await service.getCharacteristic(UUID.config);
    characteristics = { status, command, sample, config };
    status.addEventListener("characteristicvaluechanged", onStatus);
    sample.addEventListener("characteristicvaluechanged", onSample);
    await status.startNotifications();
    await sample.startNotifications();
    received = missed = 0;
    lastSequence = null;
    assembler.reset();
    el.sample.textContent = "Esperando adquisición…";
    el.received.textContent = "0 muestras completas recibidas";
    await sendCommand("GET_STATUS");
    await sendCommand("GET_CONFIG");
    if (!device.gatt.connected) throw new Error("Conexión perdida durante la configuración");
    el.connection.textContent = `Conectado · ${device.name || "ATLANTES-TCS3448"}`;
  } catch (error) {
    device?.gatt?.disconnect();
    clearConnection();
    throw error;
  }
}));
el.disconnect.addEventListener("click", () => action(async () => {
  try { await sendCommand("STOP"); }
  finally { device?.gatt?.disconnect(); clearConnection(); }
}));
el.start.addEventListener("click", () => action(() => sendCommand("START")));
el.stop.addEventListener("click", () => action(() => sendCommand("STOP")));
el.refresh.addEventListener("click", () => action(async () => {
  await sendCommand("GET_STATUS");
  await sendCommand("GET_CONFIG");
}));
if (!supported) el.support.textContent = "Web Bluetooth no está disponible. Abre este archivo en un navegador compatible con Web Bluetooth o utiliza HTTPS.";
controls();
