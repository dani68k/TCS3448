import { readFile, writeFile, mkdir } from "node:fs/promises";

const source = new URL("../web/src/", import.meta.url);
const read = async name => (await readFile(new URL(name, source), "utf8")).replace(/^\uFEFF/, "");
const [template, css, protocol, chart, ui, captures, app, logo] = await Promise.all([
  read("index.template.html"), read("style.css"), read("protocol.mjs"), read("spectral-chart.mjs"), read("ui.mjs"), read("captures.mjs"), read("app.mjs"),
  readFile(new URL("../web/images/logo-signal.png", import.meta.url)),
]);
// The dependency-free protocol, presentation and BLE app share one private scope.
const scripts = [protocol, chart, ui, captures, app].map(source => source
  .replace(/^export /gm, "")
  .replace(/^import \{[^\n]+\} from "\.\/(?:protocol|ui|spectral-chart|captures)\.mjs";\r?\n/gm, ""));
if (scripts.some(source => /^\s*(?:import|export)\s/m.test(source))) {
  throw new Error("Unexpected module dependency in the standalone page");
}
const script = `(() => {\n"use strict";\n${scripts.join("\n")}\n})();`
  .replace(/<\/script/gi, "<\\/script");
const logoUri = `data:image/png;base64,${logo.toString("base64")}`;
const html = template.replace("{{STYLE}}", () => css).replace("{{SCRIPT}}", () => script)
  .replaceAll("{{LOGO}}", logoUri);
if (/\{\{(?:STYLE|SCRIPT|LOGO)\}\}/.test(html)) throw new Error("Unresolved HTML placeholder");
await mkdir(new URL("../web/dist/", import.meta.url), { recursive: true });
await writeFile(new URL("../web/dist/index.html", import.meta.url), html, "utf8");
console.log("Generated web/dist/index.html (standalone)");
