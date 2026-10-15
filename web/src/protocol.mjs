export const UUID = Object.freeze({
  service: "a71a0001-3448-4c53-8a7e-41544c414e54",
  status: "a71a0002-3448-4c53-8a7e-41544c414e54",
  command: "a71a0003-3448-4c53-8a7e-41544c414e54",
  sample: "a71a0004-3448-4c53-8a7e-41544c414e54",
  config: "a71a0005-3448-4c53-8a7e-41544c414e54",
});
export const COMMAND = Object.freeze({ START: 1, STOP: 2, GET_STATUS: 3, GET_CONFIG: 4 });
export const CHANNELS = Object.freeze([
  "FZ", "FY", "FXL", "NIR", "VIS_TL_0", "VIS_BR_0", "F2", "F3", "F4",
  "F6", "VIS_TL_1", "VIS_BR_1", "F1", "F7", "F8", "F5", "VIS_TL_2", "VIS_BR_2",
]);
export const GAINS = Object.freeze([0.5, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048]);
export const ERRORS = Object.freeze(["Ninguno", "Inicializacion", "Configuracion", "LED", "Comunicacion I2C", "Tiempo de espera agotado"]);

function validate(value, size) {
  if (!(value instanceof DataView) || value.byteLength !== size) throw new Error("Longitud de paquete incorrecta");
  if (value.getUint8(0) !== 1) throw new Error("Version de protocolo incompatible");
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
  validate(value, 20);
  if (value.getUint8(1) >= GAINS.length) throw new Error("Ganancia invalida");
  return {
    gain: GAINS[value.getUint8(1)], atime: value.getUint8(2), ledCurrentMa: value.getUint8(3),
    astep: value.getUint16(4, true), intervalMs: value.getUint16(6, true),
    integrationMs: value.getUint32(8, true) / 1000, i2cHz: value.getUint32(12, true),
    sda: value.getUint8(16), scl: value.getUint8(17),
  };
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
      if (data.getUint32(4, true) !== sequence || data.getUint8(3) !== 18 || data.getUint8(2) > 1 || data.getUint8(1) >= GAINS.length) throw new Error("Muestra invalida");
      return {
        sequence, timestampMs: data.getUint32(8, true), gain: GAINS[data.getUint8(1)],
        saturated: !!data.getUint8(2), channels: CHANNELS.map((name, i) => [name, data.getUint16(12 + i * 2, true)]),
      };
    } catch (error) {
      this.reset();
      throw error;
    }
  }
}
