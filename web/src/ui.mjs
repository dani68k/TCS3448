import { AUTOZERO } from "./protocol.mjs";

const PHYSICAL_CHANNELS = ["F1", "F2", "FZ", "F3", "F4", "F5", "FY", "FXL", "F6", "F7", "F8", "NIR", "VIS TL", "VIS BR"];

export function physicalReadings(sample) {
  const values = new Map(sample?.channels || []);
  // VIS is represented by the first SMUX cycle; other cycles stay in Raw Debug.
  return PHYSICAL_CHANNELS.map(name => ({
    name, value: values.get(name === "VIS TL" ? "VIS_TL_0" : name === "VIS BR" ? "VIS_BR_0" : name) ?? null,
  }));
}

export function createInstrumentUI(el) {
  const node = (tag, className, text) => {
    const element = document.createElement(tag);
    element.className = className;
    if (text !== undefined) element.textContent = String(text);
    return element;
  };
  const badge = (element, text, state) => {
    element.textContent = text;
    element.dataset.state = state;
  };
  const dash = "\u2014";
  const logs = [];
  let lastStatusKey = "";
  let lastSample = null;
  let lastStatus = null;
  const debug = text => {
    logs.push(`${new Date().toLocaleTimeString()}  ${text}`);
    if (logs.length > 40) logs.shift();
    el["debug-log"].textContent = logs.join("\n");
  };
  function showView(view, updateHash = false) {
    const setup = view === "setup";
    el["measurement-view"].hidden = setup;
    el["setup-view"].hidden = !setup;
    el["nav-measurement"].setAttribute("aria-current", setup ? "false" : "page");
    el["nav-setup"].setAttribute("aria-current", setup ? "page" : "false");
    document.title = `TCS3448 | ${setup ? "Sensor Setup" : "Measurement"}`;
    if (updateHash) window.location.hash = setup ? "#/setup" : "#/measurement";
  }
  el["nav-measurement"].addEventListener("click", event => { event.preventDefault(); showView("measurement", true); });
  el["nav-setup"].addEventListener("click", event => { event.preventDefault(); showView("setup", true); });
  el["setup-connect-link"].addEventListener("click", event => { event.preventDefault(); showView("measurement", true); });
  window.addEventListener("hashchange", () => {
    const hash = window.location.hash;
    if (hash === "#/setup" || hash === "#/measurement") showView(hash.slice(2));
  });
  showView(window.location.hash === "#/setup" ? "setup" : "measurement");

  function readings(sample) {
    el["sample-readout"].replaceChildren(...physicalReadings(sample).map(({ name, value }) => {
      const row = node("div", `channel-row${name.startsWith("VIS") ? " reference-channel" : ""}`);
      row.append(node("dt", "channel-name", name), node("dd", "reading-value", value === null ? dash : value));
      if (value === null) row.dataset.empty = "true";
      return row;
    }));
  }
  function sampleState() {
    if (lastStatus && (!lastStatus.ready || (lastStatus.error > 0 && lastStatus.error < 16))) {
      badge(el["sample-state"], "ERROR", "danger");
    } else if (lastSample) {
      badge(el["sample-state"], lastSample.saturated ? "SATURATED" : "VALID", lastSample.saturated ? "danger" : "success");
    } else {
      badge(el["sample-state"], lastStatus?.acquiring ? "MEASURING" : "IDLE", lastStatus?.acquiring ? "accent" : "muted");
    }
  }
  function clearSample() {
    lastSample = null;
    readings(null);
    el["summary-sample"].textContent = dash;
    el["summary-gain"].textContent = dash;
    el["summary-integration"].textContent = dash;
    el["frame-label"].textContent = "Waiting for a sample";
    sampleState();
  }
  const configurationState = (text, state, detail = "") => {
    badge(el["config-state"], text, state);
    el["config-detail"].textContent = detail;
  };
  function config(c) {
    const zero = c?.autozero === 1 ? "Before first measurement" : c?.autozero === 2 ? "Every measurement" : AUTOZERO[0];
    const rows = [
      ["Gain", c?.valid ? `${c.gain}x` : dash],
      ["Integration", c?.valid ? `${c.integrationMs.toFixed(3)} ms` : dash],
      ["LED Current", c?.valid ? `${c.ledCurrentMa} mA` : dash],
      ["LED", c?.valid ? (c.ledEnabled ? "ON" : "OFF") : dash],
      ["Auto SMUX", c?.valid ? `${c.smux} channels` : dash],
      ["Autozero", c?.valid ? zero : dash],
      ["Sample Period", c?.valid ? `${c.intervalMs} ms` : dash],
      ["ADC Full Scale", c?.valid ? `${c.adcFullScale} counts` : dash],
    ];
    el.config.replaceChildren(...rows.map(([name, value]) => {
      const row = node("div", "config-row");
      row.append(node("dt", "", name), node("dd", "", value));
      return row;
    }));
    configurationState(c?.valid ? "CONFIGURATION VALID" : "NOT AVAILABLE", c?.valid ? "success" : "muted");
    if (c?.valid) debug(`CONFIG revision=${c.revision} gain=${c.gain}x integration=${c.integrationMs}ms SMUX=${c.smux} LED=${c.ledCurrentMa}mA/${c.ledEnabled ? "ON" : "OFF"}`);
  }
  function connection(state, name = "TCS3448") {
    badge(el.connection, state.toUpperCase(), state === "connected" ? "success" : state === "connecting" ? "warning" : "muted");
    el["device-name"].textContent = name;
    el["setup-disconnected"].hidden = state === "connected";
  }
  function status(s) {
    lastStatus = s;
    const error = s && (!s.ready || (s.error > 0 && s.error < 16));
    badge(el["acquisition-state"], error ? "ERROR" : s?.acquiring ? "RUNNING" : "IDLE", error ? "danger" : s?.acquiring ? "accent" : "muted");
    const key = s ? `${s.ready}/${s.acquiring}/${s.error}` : "disconnected";
    if (key !== lastStatusKey) { debug(s ? `STATUS ready=${Number(s.ready)} running=${Number(s.acquiring)} error=${s.error}` : "DISCONNECTED"); lastStatusKey = key; }
    sampleState();
  }
  function sample(sample, integrationMs) {
    lastSample = sample;
    readings(sample);
    el["summary-sample"].textContent = `#${sample.sequence}`;
    el["summary-gain"].textContent = `${sample.gain}x`;
    el["summary-integration"].textContent = Number.isFinite(integrationMs) ? `${integrationMs.toFixed(3)} ms` : dash;
    el["frame-label"].textContent = `Sample #${sample.sequence}`;
    sampleState();
  }
  function led() { el["led-label"].textContent = el["led-enabled"].checked ? "ON" : "OFF"; }
  el["led-enabled"].addEventListener("change", led);
  return { debug, showView, connection, status, config, configurationState, sample, clearSample, led };
}
