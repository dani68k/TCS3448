import test from "node:test";
import assert from "node:assert/strict";
import { decodeStatus, decodeConfig, SampleAssembler, CHANNELS } from "../web/src/protocol.mjs";

const view = bytes => new DataView(Uint8Array.from(bytes).buffer);
// Fixed wire fixture with distinct 16-bit values and nontrivial 32-bit fields.
const rawSample = Uint8Array.from([
  1, 7, 1, 18, 0x78, 0x56, 0x34, 0x12, 1, 0xef, 0xcd, 0xab,
  0, 0, 1, 0, 255, 0, 0, 1, 255, 255, 1, 2,
  2, 2, 3, 2, 4, 2, 5, 2, 6, 2, 7, 2,
  8, 2, 9, 2, 10, 2, 11, 2, 12, 2, 13, 2,
]);
function fragments(sequence = 0x12345678, raw = rawSample) {
  const sample = Uint8Array.from(raw);
  new DataView(sample.buffer).setUint32(4, sequence, true);
  return Array.from({ length: 4 }, (_, i) => {
    const packet = new Uint8Array(20);
    packet.set([1, i, 4, 0]);
    new DataView(packet.buffer).setUint32(4, sequence, true);
    packet.set(sample.subarray(i * 12, i * 12 + 12), 8);
    return new DataView(packet.buffer);
  });
}
function complete(assembler, packets) {
  let sample;
  packets.forEach((packet, i) => { sample = assembler.push(packet, i * 15); });
  return sample;
}

test("decodes status and acknowledgment in little-endian order", () => {
  const status = decodeStatus(view([1, 1, 0, 4, 2, 2, 0xff, 0xff, 0x78, 0x56, 0x34, 0x12, 1, 0xef, 0xcd, 0xab, 0xf4, 1, 0, 0]));
  assert.deepEqual(status, { ready: true, acquiring: false, error: 4, command: 2, result: 2, counter: 65535, sampleCount: 0x12345678, uptimeMs: 0xabcdef01, intervalMs: 500 });
});
test("decodes default configuration and integration in milliseconds", () => {
  const config = decodeConfig(view([1, 7, 29, 10, 0x57, 2, 0xf4, 1, 0x78, 0xc3, 0, 0, 0x80, 0x1a, 6, 0, 4, 5, 0, 0]));
  assert.deepEqual(config, { gain: 64, atime: 29, ledCurrentMa: 10, astep: 599, intervalMs: 500, integrationMs: 50.04, i2cHz: 400000, sda: 4, scl: 5 });
});
test("reassembles exactly one coherent sample and preserves all channel indices", () => {
  const assembler = new SampleAssembler();
  const packets = fragments();
  for (let i = 0; i < 3; i++) assert.equal(assembler.push(packets[i], i), null);
  const sample = assembler.push(packets[3], 3);
  assert.equal(sample.sequence, 0x12345678);
  assert.equal(sample.timestampMs, 0xabcdef01);
  assert.equal(sample.gain, 64);
  assert.equal(sample.saturated, true);
  assert.deepEqual(sample.channels.map(([name]) => name), CHANNELS);
  assert.deepEqual(sample.channels.map(([, value]) => value), [0, 1, 255, 256, 65535, 513, 514, 515, 516, 517, 518, 519, 520, 521, 522, 523, 524, 525]);
  assert.equal(sample.channels.find(([name]) => name === "F1")[1], 520);
});
test("discards gaps, duplicate middle packets and orphan fragments, recovers on next frame", () => {
  for (const order of [[0, 2, 3], [0, 1, 1, 2, 3], [1, 2, 3], [0, 3, 1, 2]]) {
    const assembler = new SampleAssembler();
    const packets = fragments();
    for (const i of order) assert.equal(assembler.push(packets[i]), null);
    assert.equal(complete(assembler, fragments(23)).sequence, 23);
  }
});
test("never mixes sequences, replaces interrupted frames, accepts uint32 rollover", () => {
  const assembler = new SampleAssembler();
  assembler.push(fragments(9)[0]);
  assert.equal(assembler.push(fragments(10)[1]), null);
  assert.equal(complete(assembler, fragments(0xffffffff)).sequence, 0xffffffff);
  assert.equal(complete(assembler, fragments(0)).sequence, 0);
  assembler.push(fragments(11)[0]);
  assert.equal(complete(assembler, fragments(12)).sequence, 12);
});
test("expires old fragments and clears partial data on disconnect", () => {
  const assembler = new SampleAssembler();
  const packets = fragments();
  assembler.push(packets[0], 0);
  assert.equal(assembler.push(packets[1], 2001), null);
  assembler.push(packets[0], 3000);
  assembler.reset();
  assert.equal(assembler.push(packets[1], 3001), null);
});
test("handles DataViews with nonzero offsets", () => {
  const assembler = new SampleAssembler();
  let result;
  for (const packet of fragments()) {
    const padded = new Uint8Array(32);
    padded.set(new Uint8Array(packet.buffer), 7);
    result = assembler.push(new DataView(padded.buffer, 7, 20));
  }
  assert.equal(result.sequence, 0x12345678);
});
test("rejects malformed lengths, versions, flags and channel counts", () => {
  assert.throws(() => decodeStatus(view([1])), /Longitud/);
  assert.throws(() => decodeConfig(view([2, ...Array(19).fill(0)])), /Version/);
  assert.throws(() => decodeConfig(view([1, 13, ...Array(18).fill(0)])), /Ganancia/);
  const assembler = new SampleAssembler();
  const badFragment = fragments()[0];
  badFragment.setUint8(2, 5);
  assert.throws(() => assembler.push(badFragment), /Fragmento/);
  for (const [offset, value] of [[0, 2], [1, 13], [2, 2], [3, 17]]) {
    const malformed = Uint8Array.from(rawSample);
    malformed[offset] = value;
    assert.throws(() => complete(assembler, fragments(1, malformed)));
  }
  assert.equal(complete(assembler, fragments(2)).sequence, 2);
});
