export const UUID = Object.freeze({
  service: "34480001-7f2a-4f6c-9e31-8dbe0c2f5a70",
  status: "34480002-7f2a-4f6c-9e31-8dbe0c2f5a70",
  command: "34480003-7f2a-4f6c-9e31-8dbe0c2f5a70",
  sample: "34480004-7f2a-4f6c-9e31-8dbe0c2f5a70",
  config: "34480005-7f2a-4f6c-9e31-8dbe0c2f5a70",
});
export const COMMAND = Object.freeze({ START: 1, STOP: 2, GET_STATUS: 3, GET_CONFIG: 4, APPLY_CONFIG: 5 });
export const CHANNELS = Object.freeze([
  "FZ", "FY", "FXL", "NIR", "VIS_TL_0", "VIS_BR_0", "F2", "F3", "F4",
  "F6", "VIS_TL_1", "VIS_BR_1", "F1", "F7", "F8", "F5", "VIS_TL_2", "VIS_BR_2",
]);
export const GAINS = Object.freeze([0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]);
export const AUTOZERO = Object.freeze(["Disabled", "Before first measurement cycle", "Every integration cycle"]);
export const ERRORS = Object.freeze({
  0: "OK", 1: "INITIALIZATION_FAILED", 2: "CONFIGURATION_INVALID", 3: "LED_FAILED",
  4: "I2C_COMMUNICATION_FAILED", 5: "ACQUISITION_TIMEOUT", 6: "CONFIG_APPLY_FAILED",
  16: "INVALID_GAIN", 17: "INVALID_INTEGRATION", 18: "INVALID_LED_CURRENT",
  19: "INVALID_SMUX_MODE", 20: "INVALID_AUTOZERO", 21: "INVALID_SAMPLE_PERIOD",
  22: "SAMPLE_PERIOD_TOO_SHORT", 23: "INVALID_CONFIG_PAYLOAD",
});

function validate(value, size, version = 1) {
  if (!(value instanceof DataView) || value.byteLength !== size) throw new Error("Longitud de paquete incorrecta");
  if (value.getUint8(0) !== version) throw new Error("Version de protocolo incompatible");
}
export function decodeStatus(value) {
  validate(value, 20);
  if (value.getUint8(1) > 1 || value.getUint8(2) > 1) throw new Error("Estado invalido");
  return {
    ready: !!value.getUint8(1), acquiring: !!value.getUint8(2), error: value.getUint8(3),
    command: value.getUint8(4), result: value.getUint8(5), counter: value.getUint16(6, true),
    sampleCount: value.getUint32(8, true), uptimeMs: value.getUint32(12, true), intervalMs: value.getUint32(16, true),
  };
}
export function decodeConfig(value) {
  validate(value, 20, 2);
  const gainCode = value.getUint8(1);
  const smux = value.getUint8(2);
  const autozero = value.getUint8(3);
  if (gainCode >= GAINS.length || ![6, 12, 18].includes(smux) || autozero > 2 || value.getUint8(4) > 1 || value.getUint8(5) > 1) throw new Error("Configuracion recibida invalida");
  return {
    gainCode, gain: GAINS[gainCode], smux, autozero, ledEnabled: !!value.getUint8(4), valid: !!value.getUint8(5),
    ledCurrentMa: value.getUint16(6, true), integrationMs: value.getUint32(8, true) / 1000,
    intervalMs: value.getUint32(12, true), adcFullScale: value.getUint16(16, true), revision: value.getUint16(18, true),
  };
}
export function validateRequested(config) {
  if (!Number.isInteger(config.gainCode) || config.gainCode < 0 || config.gainCode >= GAINS.length) throw new Error("INVALID_GAIN");
  if (!Number.isFinite(config.integrationMs) || config.integrationMs <= 0 || Math.round(config.integrationMs * 1000) < 6 || config.integrationMs * 1000 > 0xffffffff) throw new Error("INVALID_INTEGRATION");
  if (!Number.isInteger(config.ledCurrentMa) || config.ledCurrentMa < 4 || config.ledCurrentMa > 258 || config.ledCurrentMa % 2) throw new Error("INVALID_LED_CURRENT");
  if (typeof config.ledEnabled !== "boolean") throw new Error("INVALID_CONFIG_PAYLOAD");
  if (![6, 12, 18].includes(config.smux)) throw new Error("INVALID_SMUX_MODE");
  if (!Number.isInteger(config.autozero) || config.autozero < 0 || config.autozero > 2) throw new Error("INVALID_AUTOZERO");
  if (!Number.isInteger(config.intervalMs) || config.intervalMs <= 0 || config.intervalMs > 0x7fffffff) throw new Error("INVALID_SAMPLE_PERIOD");
  // This is a superficial check only. Firmware adds SMUX/autozero timing margins.
  if (config.intervalMs < config.integrationMs * (config.smux / 6)) throw new Error("SAMPLE_PERIOD_TOO_SHORT");
  return config;
}
export function encodeApply(config) {
  validateRequested(config);
  const bytes = new Uint8Array(20);
  const view = new DataView(bytes.buffer);
  bytes.set([0xa4, 2, config.gainCode, config.smux, config.autozero, Number(config.ledEnabled)]);
  view.setUint16(6, config.ledCurrentMa, true);
  view.setUint32(8, Math.round(config.integrationMs * 1000), true);
  view.setUint32(12, config.intervalMs, true);
  return bytes;
}
export class SampleAssembler {
  constructor() { this.reset(); }
  reset() { this.pending = null; }
  push(packet, now = Date.now()) {
    try {
      validate(packet, 20);
      const index = packet.getUint8(1);
      const sequence = packet.getUint32(4, true);
      if (index > 3 || packet.getUint8(2) !== 4 || packet.getUint8(3) !== 0) throw new Error("Fragmento invalido");
      if (index === 0) this.pending = { sequence, next: 0, started: now, bytes: new Uint8Array(48) };
      const pending = this.pending;
      if (!pending || pending.sequence !== sequence || pending.next !== index || now - pending.started > 2000) {
        this.reset();
        return null;
      }
      pending.bytes.set(new Uint8Array(packet.buffer, packet.byteOffset + 8, 12), index * 12);
      ++pending.next;
      if (pending.next !== 4) return null;
      this.reset();
      const data = new DataView(pending.bytes.buffer);
      validate(data, 48);
      if (data.getUint32(4, true) !== sequence || ![6, 12, 18].includes(data.getUint8(3)) || data.getUint8(2) > 1 || data.getUint8(1) >= GAINS.length) throw new Error("Muestra invalida");
      return {
        sequence, timestampMs: data.getUint32(8, true), gain: GAINS[data.getUint8(1)],
        saturated: !!data.getUint8(2), channelCount: data.getUint8(3), channels: CHANNELS.slice(0, data.getUint8(3)).map((name, i) => [name, data.getUint16(12 + i * 2, true)]),
      };
    } catch (error) {
      this.reset();
      throw error;
    }
  }
}
