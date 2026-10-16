// Protocol names are the keys; wavelengths describe bands, not bar spacing.
export const SPECTRAL_CHANNELS = [
  ["F1", 407, "#9181bd"], ["F2", 424, "#7d89c9"], ["FZ", 450, "#6298cc"],
  ["F3", 473, "#66b3c6"], ["F4", 516, "#64b69c"], ["F5", 546, "#84b779"],
  ["FY", 560, "#afbd72"], ["FXL", 596, "#cbbb72"], ["F6", 636, "#cc986c"],
  ["F7", 687, "#c57978"], ["F8", 748, "#aa697b"], ["NIR", 855, "#a48a9f"],
].map(([key, wavelength, color]) => ({ key, wavelength, color }));

export function spectralReadings(sample) {
  const values = new Map(sample?.channels || []);
  return SPECTRAL_CHANNELS.map(channel => ({ ...channel, value: values.get(channel.key) ?? null }));
}

export function spectralExtrema(readings) {
  const acquired = readings.filter(c => Number.isFinite(c.value));
  if (!acquired.length) return { maximum: [], minimum: [] };
  const max = Math.max(...acquired.map(c => c.value));
  const min = Math.min(...acquired.map(c => c.value));
  return { maximum: acquired.filter(c => c.value === max), minimum: acquired.filter(c => c.value === min) };
}

export function createSpectralChart(host, summaryHost = host) {
  const svgNode = (tag, attributes = {}, text) => {
    const element = document.createElementNS("http://www.w3.org/2000/svg", tag);
    for (const [key, value] of Object.entries(attributes)) element.setAttribute(key, value);
    if (text !== undefined) element.textContent = text;
    return element;
  };
  const summary = ["max", "min"].map(kind => {
    const element = document.createElement("p");
    element.className = `chart-extreme chart-${kind}`;
    summaryHost.append(element);
    return element;
  });
  const svg = svgNode("svg", { viewBox: "0 0 960 380", role: "group", "aria-label": "Spectral RAW ADC counts" });
  const ticks = Array.from({ length: 6 }, (_, i) => {
    const y = 300 - i * 50;
    const label = svgNode("text", { x: 65, y: y + 4, "text-anchor": "end", class: "chart-axis" });
    svg.append(svgNode("line", { x1: 76, x2: 952, y1: y, y2: y, class: "chart-grid" }), label);
    return label;
  });
  svg.append(svgNode("text", { x: 12, y: 175, transform: "rotate(-90 12 175)", "text-anchor": "middle", class: "chart-axis" }, "ADC Counts"));
  let range = null;
  const bars = SPECTRAL_CHANNELS.map((c, i) => {
    const x = 82 + i * 73;
    const group = svgNode("g", { role: "img", class: "chart-channel" });
    const bar = svgNode("rect", { x: x + 3, y: 300, width: 59, height: 0, rx: 3, fill: c.color, class: "chart-bar" });
    const value = svgNode("text", { x: x + 32, y: 290, "text-anchor": "middle", class: "chart-value" });
    const label = svgNode("text", { x: x + 32, y: 325, "text-anchor": "middle", class: "chart-label" }, c.key);
    const marker = svgNode("text", { x: x + 32, y: 369, "text-anchor": "middle", class: "chart-marker" });
    group.append(bar, value, label,
      svgNode("text", { x: x + 32, y: 347, "text-anchor": "middle", class: "chart-axis" }, `${c.wavelength} nm`), marker);
    svg.append(group);
    return { group, bar, value, label, marker };
  });
  const empty = svgNode("text", { x: 514, y: 170, "text-anchor": "middle", class: "chart-empty" }, "Waiting for data...");
  svg.append(empty);
  host.append(svg);
  function update(sample, adcFullScale) {
    const data = spectralReadings(sample);
    const { maximum, minimum } = spectralExtrema(data);
    [maximum, minimum].forEach((channels, index) => {
      const kind = index === 0 ? "MAX" : "MIN";
      summary[index].textContent = channels.length
        ? `${kind} ${channels.map(c => `${c.key} ${c.wavelength} nm`).join(", ")} - ${channels[0].value} counts`
        : `${kind} \u2014`;
    });
    if (Number.isFinite(adcFullScale) && adcFullScale > 0) range = adcFullScale;
    ticks.forEach((tick, i) => { tick.textContent = range ? String(Math.round(range * i / 5)) : "\u2014"; });
    empty.setAttribute("visibility", sample ? "hidden" : "visible");
    bars.forEach(({ group, bar, value, label, marker }, i) => {
      const c = data[i], height = range ? Math.min(c.value ?? 0, range) / range * 250 : 0;
      const isMax = maximum.some(channel => channel.key === c.key);
      const isMin = minimum.some(channel => channel.key === c.key);
      bar.setAttribute("height", height);
      bar.setAttribute("y", 300 - height);
      value.setAttribute("y", 290 - height);
      value.textContent = sample ? c.value ?? "\u2014" : "";
      // Integer-count thresholds match the displayed ADC margins (e.g. 53957 -> 51259).
      const knownScale = c.value !== null && Number.isFinite(adcFullScale) && adcFullScale > 0;
      const isSat = knownScale && c.value >= Math.round(adcFullScale * .95);
      const isNearSat = knownScale && !isSat && c.value >= Math.round(adcFullScale * .90);
      const stateClass = isSat ? " chart-sat" : isNearSat ? " chart-near-sat" : isMax ? " chart-max" : isMin ? " chart-min" : "";
      label.setAttribute("class", `chart-label${stateClass}`);
      marker.textContent = isSat ? "SAT" : isNearSat ? "SAT" : isMax && isMin ? "MAX/MIN" : isMax ? "MAX" : isMin ? "MIN" : "";
      marker.setAttribute("class", `chart-marker${stateClass}`);
      group.setAttribute("aria-label", `${c.key}, ${c.wavelength} nm, ${c.value === null ? "Not acquired" : `${c.value} counts`}${marker.textContent ? `, ${marker.textContent}` : ""}`);
    });
  }
  update(null);
  return { update };
}
