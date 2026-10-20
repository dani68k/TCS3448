import { AUTOZERO } from "./protocol.mjs";

import { SPECTRAL_CHANNELS, createSpectralChart } from "./spectral-chart.mjs";

const PHYSICAL_CHANNELS = [...SPECTRAL_CHANNELS.map(c => c.key), "VIS TL", "VIS BR"];

export function physicalReadings(sample) {
  const values = new Map(sample?.channels || []);
  // VIS is represented by the first SMUX cycle; other cycles stay in Raw Debug.
  return PHYSICAL_CHANNELS.map(name => ({
    name, value: values.get(name === "VIS TL" ? "VIS_TL_0" : name === "VIS BR" ? "VIS_BR_0" : name) ?? null,
  }));
}

export function createRawHistogram(host, extrema, visHost) {
  const node=(tag, className, text)=>{ const e=document.createElement(tag); e.className=className; e.textContent=text; return e; };
  const dash="\u2014";
  const chart=createSpectralChart(host,extrema);
  const visRows = new Map(["VIS TL", "VIS BR"].map(name => {
    const row = node("div", "vis-row");
    const value = node("dd", "reading-value", dash);
    const meter = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    meter.setAttribute("viewBox", "0 0 100 6");
    meter.setAttribute("class", "vis-meter");
    meter.setAttribute("preserveAspectRatio", "none");
    meter.setAttribute("role", "img");
    const track = document.createElementNS("http://www.w3.org/2000/svg", "rect");
    const fill = document.createElementNS("http://www.w3.org/2000/svg", "rect");
    for (const rect of [track, fill]) { rect.setAttribute("height", 6); rect.setAttribute("rx", 2); }
    track.setAttribute("width", 100); track.setAttribute("class", "vis-track");
    fill.setAttribute("width", 0); fill.setAttribute("class", "vis-fill");
    meter.append(track, fill);
    row.append(node("dt", "channel-name", name), value);
    // Keep the visual level outside dd: updating its text must not remove the SVG.
    row.append(meter);
    visHost.append(row);
    return [name, {value, meter, fill}];
  }));
  function update(sample, fullScale) {
    chart.update(sample, fullScale);
    for (const {name, value} of physicalReadings(sample).filter(c => c.name.startsWith("VIS"))) {
      const item = visRows.get(name);
      item.value.textContent = value === null ? dash : `${value} counts`;
      item.fill.setAttribute("width", fullScale ? Math.min(value ?? 0, fullScale) / fullScale * 100 : 0);
      item.meter.setAttribute("aria-label", `${name}: ${value === null ? "Not acquired" : `${value} counts`}`);
    }
  }

  return {update};
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
  const histogram = createRawHistogram(el["spectral-chart"], el["spectral-extrema"], el["sample-readout"]);
  const logs = [];
  let lastStatusKey = "";
  let lastSample = null;
  let sampleFullScale = null;
  let confirmedFullScale = null;
  let confirmedConfig = null;
  const debug = text => {
    logs.push(`${new Date().toLocaleTimeString()}  ${text}`);
    if (logs.length > 40) logs.shift();
    el["debug-log"].textContent = logs.join("\n");
  };
  const views = { measurement: "Measurement", setup: "Sensor Setup", data: "Saved captures", debug: "Advanced / Raw Debug" };
  function showView(view, updateHash = false) {
    const active = Object.hasOwn(views, view) ? view : "measurement";
    for (const key of Object.keys(views)) {
      el[`${key}-view`].hidden = key !== active;
      el[`nav-${key}`].setAttribute("aria-current", key === active ? "page" : "false");
    }
    document.title = `TCS3448 | ${views[active]}`;
    if (updateHash) window.location.hash = `#/${active}`;
  }
  for (const view of Object.keys(views)) {
    el[`nav-${view}`].addEventListener("click", event => { event.preventDefault(); showView(view, true); });
  }
  el["setup-connect-link"].addEventListener("click", event => { event.preventDefault(); showView("measurement", true); });
  window.addEventListener("hashchange", () => {
    const view = window.location.hash.slice(2);
    if (window.location.hash.startsWith("#/") && Object.hasOwn(views, view)) showView(view);
  });
  showView(window.location.hash.slice(2));

  const readings = sample => histogram.update(sample, sampleFullScale ?? confirmedFullScale);

  function measurementSettings(integrationMs, gain, ledCurrentMa) {
    el["summary-integration"].textContent = Number.isFinite(integrationMs) ? `${integrationMs.toFixed(3)} ms` : dash;
    el["summary-gain"].textContent = Number.isFinite(gain) ? `${gain}x` : dash;
    el["summary-led-current"].textContent = Number.isFinite(ledCurrentMa) ? `${ledCurrentMa} mA` : dash;
  }
  function clearSample() {
    lastSample = null;
    sampleFullScale = null;
    readings(null);
    measurementSettings(confirmedConfig?.integrationMs, confirmedConfig?.gain, confirmedConfig?.ledCurrentMa);
  }
  const configurationState = (text, state, detail = "") => {
    badge(el["config-state"], text, state);
    el["config-detail"].textContent = detail;
  };
  function config(c) {
    confirmedConfig = c?.valid ? c : null;
    if (!lastSample) measurementSettings(confirmedConfig?.integrationMs, confirmedConfig?.gain, confirmedConfig?.ledCurrentMa);
    confirmedFullScale = c?.valid ? c.adcFullScale : null;
    if (!lastSample) readings(null);
    const zero = c?.autozero === 1 ? "Before first measurement" : c?.autozero === 2 ? "Every measurement" : AUTOZERO[0];
    const rows = [
      ["Gain", c?.valid ? `${c.gain}x` : dash],
      ["Integration", c?.valid ? `${c.integrationMs.toFixed(3)} ms` : dash],
      ["LED Current", c?.valid ? `${c.ledCurrentMa} mA` : dash],
      ["LED", c?.valid ? (c.ledAuto ? "AUTO - with measurement" : c.ledEnabled ? "ON" : "OFF") : dash],
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
    if (c?.valid) debug(`CONFIG revision=${c.revision} gain=${c.gain}x integration=${c.integrationMs}ms SMUX=${c.smux} LED=${c.ledCurrentMa}mA/${c.ledAuto ? "AUTO" : c.ledEnabled ? "ON" : "OFF"}`);
  }
  function connection(state, name = "TCS3448") {
    badge(el.connection, state.toUpperCase(), state === "connected" ? "success" : state === "connecting" ? "warning" : "muted");
    el["device-name"].textContent = name;
    el["setup-disconnected"].hidden = state === "connected";
  }
  function status(s) {
    const error = s && (!s.ready || (s.error > 0 && s.error < 16));
    badge(el["acquisition-state"], error ? "ERROR" : s?.acquiring ? "RUNNING" : "IDLE", error ? "danger" : s?.acquiring ? "accent" : "muted");
    const key = s ? `${s.ready}/${s.acquiring}/${s.error}` : "disconnected";
    if (key !== lastStatusKey) { debug(s ? `STATUS ready=${Number(s.ready)} running=${Number(s.acquiring)} error=${s.error}` : "DISCONNECTED"); lastStatusKey = key; }
  }
  function sample(sample, integrationMs, adcFullScale, ledCurrentMa) {
    lastSample = sample;
    sampleFullScale = adcFullScale;
    readings(sample);
    measurementSettings(integrationMs, sample.gain, ledCurrentMa);
  }
  function led() {
    el["led-enabled"].disabled = el["led-auto"].checked;
    el["led-label"].textContent = el["led-auto"].checked ? "AUTO" : el["led-enabled"].checked ? "ON" : "OFF";
  }
  el["led-auto"].addEventListener("change", led);
  el["led-enabled"].addEventListener("change", led);
  return { debug, showView, connection, status, config, configurationState, sample, clearSample, led };
}
