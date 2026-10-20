// Minimal DOM support for application tests; never included in the generated Web.
class TestElement {
  disabled = false;
  hidden = false;
  value = "";
  checked = false;
  dataset = {};
  children = [];
  attributes = new Map();
  handlers = new Map();
  _text = "";
  constructor(tag = "div") { this.tagName = tag.toUpperCase(); }
  get textContent() { return this._text + this.children.map(child => child.textContent ?? String(child)).join(""); }
  set textContent(value) { this._text = String(value); this.children = []; }
  addEventListener(event, handler) { this.handlers.set(event, handler); }
  setAttribute(name, value) { this.attributes.set(name, String(value)); }
  getAttribute(name) { return this.attributes.get(name) ?? null; }
  replaceChildren(...children) { this._text = ""; this.children = children; }
  append(...children) { this.children.push(...children); }
  showModal() { this.open = true; }
  close() { this.open = false; }
  async click() { if (!this.disabled) await this.handlers.get("click")?.(new Event("click", { cancelable: true })); }
}
export function createTestDOM(html, hash = "") {
  const elements = new Map([...html.matchAll(/id="([^"]+)"/g)].map(match => [match[1], new TestElement()]));
  const el = new Proxy({}, { get: (_, id) => elements.get(id) });
  const document = { title: "", getElementById: id => elements.get(id) ?? null, createElementNS: (_, tag) => new TestElement(tag), createElement: tag => new TestElement(tag) };
  const window = new EventTarget();
  window.isSecureContext = true;
  window.location = { hash };
  const Option = class extends TestElement {
    constructor(text, value) { super("option"); this.textContent = text; this.value = value; }
  };
  return { el, document, window, Option };
}
