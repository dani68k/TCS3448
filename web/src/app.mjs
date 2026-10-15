import { UUID, COMMAND, ERRORS, GAINS, decodeStatus, decodeConfig, encodeApply, SampleAssembler } from "./protocol.mjs";
import { createInstrumentUI } from "./ui.mjs";

const ids = ["connect", "disconnect", "start", "stop", "refresh", "apply", "setup", "gain", "integration", "led-current", "led-enabled", "smux", "autozero", "period", "connection", "support", "status", "config", "config-state", "sample", "received", "message", "nav-measurement", "nav-setup", "measurement-view", "setup-view", "setup-connect-link", "setup-disconnected", "acquisition-state", "device-name", "sample-readout", "summary-sample", "summary-gain", "summary-integration", "sample-state", "frame-label", "debug-log", "config-detail", "led-label"];
const el = Object.fromEntries(ids.map(id => [id, document.getElementById(id)]));
const assembler = new SampleAssembler();
const ui = createInstrumentUI(el);
let applyingConfig = false;
let device = null;
let characteristics = null;
let latestStatus = null;
let activeConfig = null;
let busy = false;
let received = 0;
let lastSequence = null;
let missed = 0;
const supported = !!navigator.bluetooth && window.isSecureContext;
el.gain.replaceChildren(...GAINS.map((gain, code) => new Option(`${gain}x`, String(code))));

function controls() {
  const linked = !!characteristics && !!device?.gatt?.connected;
  const ready = linked && latestStatus?.ready && activeConfig?.valid;
  el.connect.disabled = busy || linked || !supported;
  el.disconnect.disabled = busy || !linked;
  el.start.disabled = busy || !ready || !!latestStatus?.acquiring;
  el.stop.disabled = busy || !linked || !latestStatus?.ready;
  el.refresh.disabled = busy || !linked;
  el.setup.disabled = busy || !ready;
  el.apply.disabled = busy || !ready;
}
function showError(error) {
  const detail = error.message || String(error);
  ui.debug(`ERROR ${detail}`);
  const messages = {
    INVALID_GAIN: "Select a supported gain.", INVALID_INTEGRATION: "Enter a supported integration time greater than zero.",
    INVALID_LED_CURRENT: "LED current must be an even value between 4 and 258 mA.",
    INVALID_SMUX_MODE: "Select 6, 12 or 18 channels.", INVALID_AUTOZERO: "Select a supported autozero mode.",
    INVALID_SAMPLE_PERIOD: "Enter a positive whole-number sample period.",
    SAMPLE_PERIOD_TOO_SHORT: "Sample period is too short for these sensor settings. Increase the period.",
    CONFIG_APPLY_FAILED: "The device could not apply the settings. Read the active configuration before continuing.",
    ACK_TIMEOUT: "The device has not confirmed the change. Read the active configuration before retrying.",
    INITIALIZATION_FAILED: "The sensor is unavailable. Check the device and reconnect.",
  };
  const key = Object.keys(messages).find(key => detail.includes(key));
  el.message.textContent = messages[key] || (error.name === "NotFoundError" ? "No device selected." : "The operation could not be completed. Check the connection and try again. Details are available in Advanced Debug.");
}
function showStatus(value) {
  const previousError = latestStatus?.error;
  latestStatus = decodeStatus(value);
  const s = latestStatus;
  el.status.textContent = [
    `Sensor / configuracion conocida: ${s.ready ? "SI" : "NO"}`,
    `Acquisition: ${s.acquiring ? "RUNNING" : "STOPPED"}`,
    `Error / ultima respuesta: ${ERRORS[s.error] ?? s.error}`,
    `Muestras adquiridas: ${s.sampleCount}`, `Sample period: ${s.intervalMs} ms`,
    `Tiempo desde arranque: ${(s.uptimeMs / 1000).toFixed(1)} s`,
  ].join("\n");
  if (!s.ready || (s.error > 0 && s.error < 16 && s.error !== previousError)) {
    activeConfig = null;
    ui.config(null);
    ui.configurationState("CHECK DEVICE", "warning", "Read the active configuration before continuing.");
  }
  ui.status(s);
  controls();
  return s;
}
function fillRequested(c) {
  el.gain.value = String(c.gainCode);
  el.integration.value = String(c.integrationMs);
  el["led-current"].value = String(c.ledCurrentMa);
  el["led-enabled"].checked = c.ledEnabled;
  el.smux.value = String(c.smux);
  el.autozero.value = String(c.autozero);
  el.period.value = String(c.intervalMs);
  ui.led();
}
function showConfig(value, fill = false) {
  activeConfig = decodeConfig(value);
  const c = activeConfig;
  ui.config(c);
  if (fill && c.valid) fillRequested(c);
  controls();
}
function requestedConfig() {
  return {
    gainCode: Number(el.gain.value), integrationMs: Number(el.integration.value),
    ledCurrentMa: Number(el["led-current"].value), ledEnabled: el["led-enabled"].checked,
    smux: Number(el.smux.value), autozero: Number(el.autozero.value), intervalMs: Number(el.period.value),
  };
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
    ui.sample(sample, !applyingConfig && activeConfig?.valid ? activeConfig.integrationMs : undefined);
    ++received;
    el.sample.textContent = [
      `Muestra #${sample.sequence} - ESP32 ${sample.timestampMs} ms - ${sample.channelCount} channels`,
      `Gain: ${sample.gain}x - Saturacion: ${sample.saturated ? "SI" : "No"}`, "",
      ...sample.channels.map(([name, value]) => `${name.padEnd(10)} ${String(value).padStart(5)}`),
    ].join("\n");
    el.received.textContent = `${received} muestras completas recibidas - ${missed} muestras omitidas entre las recibidas`;
  } catch (error) { showError(error); }
}
function clearConnection() {
  if (characteristics) {
    characteristics.status.removeEventListener("characteristicvaluechanged", onStatus);
    characteristics.sample.removeEventListener("characteristicvaluechanged", onSample);
  }
  characteristics = null;
  latestStatus = activeConfig = null;
  assembler.reset();
  lastSequence = null;
  ui.connection("disconnected");
  ui.status(null);
  ui.clearSample();
  el.status.textContent = "Sin estado de la conexion actual.";
  ui.config(null);
  el.sample.textContent = "Sin muestra de la conexion actual.";
  for (const id of ["gain", "integration", "led-current", "smux", "autozero", "period"]) el[id].value = "";
  el["led-enabled"].checked = false;
  ui.led();
  controls();
}

async function sendCommand(name, { payload, fill = false, allowUnavailable = false } = {}) {
  const current = characteristics;
  if (!current || !device?.gatt?.connected) throw new Error("Dispositivo desconectado");
  ui.debug(`COMMAND ${name}`);
  const before = decodeStatus(await current.status.readValue()).counter;
  await current.command.writeValueWithResponse(payload || new TextEncoder().encode(name));
  const deadline = Date.now() + 5000;
  while (Date.now() < deadline) {
    if (characteristics !== current || !device?.gatt?.connected) throw new Error("Conexion perdida");
    const status = showStatus(await current.status.readValue());
    if (status.counter !== before && (status.command === COMMAND[name] || status.command === 255)) {
      ui.debug(`ACK ${name} counter=${status.counter} result=${status.result} error=${status.error}`);
      if (name === "GET_CONFIG" || name === "APPLY_CONFIG") {
        // Even failed APPLY returns the old verified config, or an explicit unknown flag.
        showConfig(await current.config.readValue(), fill && status.result === 0);
      }
      if (name === "STOP" || name === "APPLY_CONFIG") assembler.reset();
      if (status.result !== 0) {
        if (allowUnavailable && name === "GET_CONFIG" && !status.ready && status.result === 2) return;
        const resultNames = ["OK", "UNKNOWN_COMMAND", "SENSOR_ERROR", "COMMAND_QUEUE_FULL", "INVALID_CONFIG", "CONFIG_APPLY_FAILED"];
        throw new Error(`${name}: ${status.error ? ERRORS[status.error] ?? status.error : resultNames[status.result] ?? "COMMAND_FAILED"} (result=${status.result})`);
      }
      return;
    }
    await new Promise(resolve => setTimeout(resolve, 100));
  }
  throw new Error(`${name}: ACK_TIMEOUT; consultar GET_CONFIG antes de reintentar`);
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
    ui.connection("connecting");
    const server = await device.gatt.connect();
    ui.connection("connected", device.name || "TCS3448");
    ui.debug("GATT connected");
    const service = await server.getPrimaryService(UUID.service);
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
    el.sample.textContent = "Esperando adquisicion...";
    el.received.textContent = "0 muestras completas recibidas";
    await sendCommand("GET_STATUS");
    await sendCommand("GET_CONFIG", { fill: true, allowUnavailable: true });
    if (!device.gatt.connected) throw new Error("Conexion perdida durante la configuracion");
    ui.connection("connected", device.name || "TCS3448");
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
  await sendCommand("GET_CONFIG", { fill: true, allowUnavailable: true });
}));
el.apply.addEventListener("click", () => action(async () => {
  const payload = encodeApply(requestedConfig());
  applyingConfig = true;
  ui.configurationState("APPLYING", "warning", "Waiting for device confirmation.");
  try {
    await sendCommand("APPLY_CONFIG", { payload, fill: true });
    ui.configurationState("CONFIGURATION VALID", "success", "Settings applied to the device.");
    ui.clearSample();
  } catch (error) {
    if (error.message.includes("ACK_TIMEOUT")) {
      activeConfig = null;
      ui.config(null);
    }
    ui.configurationState(activeConfig?.valid ? "CONFIGURATION UNCHANGED" : "CHECK DEVICE", "warning", "Requested settings were not confirmed. Read from device to refresh.");
    throw error;
  } finally { applyingConfig = false; }
}));
if (!supported) el.support.textContent = "Web Bluetooth is unavailable in this browser. Use a compatible browser with Bluetooth enabled.";
clearConnection();
