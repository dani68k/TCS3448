import test from 'node:test';
import assert from 'node:assert/strict';
import { SPECTRAL_CHANNELS, spectralReadings, spectralExtrema, createSpectralChart } from '../web/src/spectral-chart.mjs';
import { createTestDOM } from '../scripts/web-test-dom.mjs';

test('spectral mapping uses names, correct wavelengths, excludes VIS and preserves zero/missing', () => {
  assert.deepEqual(SPECTRAL_CHANNELS.map(c => c.wavelength), [407,424,450,473,516,546,560,596,636,687,748,855]);
  const channels = SPECTRAL_CHANNELS.map((c,i) => [c.key,i]).reverse();
  const data = spectralReadings({channels: [...channels, ['VIS_TL_0',65535]]}, 18000);
  assert.deepEqual(data.map(c => c.value), Array.from({length:12},(_,i) => i));
  assert.equal(spectralReadings(null)[0].value, null);
});

test('SVG uses fixed full scale, marks sample extrema and reuses bars without hover or selection', () => {
  const dom = createTestDOM('<div id="chart"></div><div id="extrema"></div>');
  globalThis.document = dom.document;
  const host = dom.el.chart;
  const chart = createSpectralChart(host, dom.el.extrema);
  const svg = host.children[0];
  const groups = svg.children.filter(c => c.getAttribute('class') === 'chart-channel');
  assert.equal(groups.length,12);
  const first = groups[0].children[0];
  chart.update({channels:[['F1',1000], ['F2',0], ['FZ',200], ['VIS_TL_0',65535]]}, 18000);
  assert.equal(Number(first.getAttribute('height')),1000 / 18000 * 250);
  assert.match(dom.el.extrema.textContent, /MAX F1 407 nm - 1000 counts/);
  assert.match(dom.el.extrema.textContent, /MIN F2 424 nm - 0 counts/);
  assert.match(groups[0].children[2].getAttribute('class'), /chart-max/);
  assert.match(groups[1].children[2].getAttribute('class'), /chart-min/);
  assert.equal(groups[2].children[2].getAttribute('class'), 'chart-label');
  assert.equal(groups[3].children[4].textContent, '');
  groups.forEach(g => { assert.equal(g.handlers.size,0); assert.equal(g.getAttribute('tabindex'),null); });
  assert.equal(host.children.length,1);
  assert.ok(svg.children.some(c => c.textContent === 'ADC Counts' && c.getAttribute('transform') === 'rotate(-90 12 175)'));
  for(let i=0;i<30;i++) chart.update({channels:[['F1',10]]});
  assert.equal(Number(first.getAttribute('height')),10 / 18000 * 250);
  assert.equal(groups[0].children[4].textContent,'MAX/MIN');
  chart.update({channels:[['F1',65535]]});
  assert.ok(Number(first.getAttribute('height')) <= 250);
  assert.equal(groups[0].children[0],first);
  assert.equal(groups[1].children[1].textContent,'\u2014');
  chart.update({channels:[['F1',1000]]}, 10000);
  assert.equal(Number(first.getAttribute('height')),25);
  chart.update(null);
  assert.equal(first.getAttribute('height'),'0');
  assert.equal(dom.el.extrema.textContent,'MAX \u2014MIN \u2014');
});

test('extrema includes ties and true zero, excludes missing and VIS, resets per sample', () => {
  const result = spectralExtrema(spectralReadings({ channels: [['F1',5], ['F2',5], ['F3',0], ['F4',0], ['VIS_BR_0',65000]] }));
  assert.deepEqual(result.maximum.map(c => c.key), ['F1','F2']);
  assert.deepEqual(result.minimum.map(c => c.key), ['F3','F4']);
  assert.deepEqual(spectralExtrema(spectralReadings(null)), { maximum: [], minimum: [] });
});

test('SAT takes priority over MAX only for spectral channels with confirmed full scale', () => {
  const dom = createTestDOM('<div id="chart"></div><div id="extrema"></div>');
  globalThis.document = dom.document;
  const chart = createSpectralChart(dom.el.chart, dom.el.extrema);
  const groups = dom.el.chart.children[0].children.filter(c => c.getAttribute('class') === 'chart-channel');
  chart.update({ saturated: true, channels: [['F1',18000], ['F2',20], ['FZ',0], ['VIS_TL_0',65535], ['VIS_BR_0',65535]] },18000);
  assert.equal(groups[0].children[2].getAttribute('class'), 'chart-label chart-sat');
  assert.equal(groups[0].children[4].textContent, 'SAT');
  assert.equal(groups[1].children[2].getAttribute('class'), 'chart-label');
  assert.equal(groups[2].children[2].getAttribute('class'), 'chart-label chart-min');
  assert.equal(groups.length,12);
  chart.update({ saturated: true, channels: [['F1',100], ['F2',20], ['VIS_TL_0',65535]] },18000);
  assert.equal(groups[0].children[2].getAttribute('class'), 'chart-label chart-max');
  assert.equal(groups[0].children[4].textContent, 'MAX');
  assert.equal(groups[1].children[4].textContent, 'MIN');
  chart.update({ saturated: true, channels: [['F1',65535]] });
  assert.equal(groups[0].children[4].textContent, 'MAX/MIN');
});

test('SAT and SAT naranja use rounded 95/90 percent boundaries and override extrema', () => {
  const dom = createTestDOM('<div id="chart"></div><div id="extrema"></div>');
  globalThis.document = dom.document;
  const chart = createSpectralChart(dom.el.chart, dom.el.extrema);
  const groups = dom.el.chart.children[0].children.filter(c => c.getAttribute('class') === 'chart-channel');
  for (const [raw, marker, state] of [
    [48560, 'MAX', 'chart-max'],
    [48561, 'SAT', 'chart-near-sat'],
    [51258, 'SAT', 'chart-near-sat'],
    [51259, 'SAT', 'chart-sat'],
    [53957, 'SAT', 'chart-sat'],
  ]) {
    chart.update({ channels: [['F1',raw], ['F2',0], ['VIS_TL_0',65535]] },53957);
    assert.equal(groups[0].children[4].textContent,marker);
    assert.equal(groups[0].children[2].getAttribute('class'),`chart-label ${state}`);
    assert.equal(groups[1].children[4].textContent,'MIN');
    assert.equal(groups[0].children[1].textContent,String(raw));
  }
  chart.update({ channels: [['F1',51259]] }, 65535);
  assert.equal(groups[0].children[4].textContent,'MAX/MIN');
});
