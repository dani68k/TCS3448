import { readFile, writeFile, mkdir } from "node:fs/promises";

const source = new URL("../web/src/", import.meta.url);
const read = name => readFile(new URL(name, source), "utf8");
const [template, css, protocol, app] = await Promise.all([
  read("index.template.html"), read("style.css"), read("protocol.mjs"), read("app.mjs"),
]);
// These two dependency-free modules share one private scope in the local HTML.
const protocolScript = protocol.replace(/^export /gm, "");
const appScript = app.replace(/^import \{[^\n]+\} from "\.\/protocol\.mjs";\r?\n/, "");
if (/^\s*(?:import|export)\s/m.test(protocolScript + appScript)) {
  throw new Error("Unexpected module dependency in the standalone page");
}
const script = `(() => {\n"use strict";\n${protocolScript}\n${appScript}\n})();`
  .replace(/<\/script/gi, "<\\/script");
const html = template.replace("{{STYLE}}", () => css).replace("{{SCRIPT}}", () => script);
if (/\{\{(?:STYLE|SCRIPT)\}\}/.test(html)) throw new Error("Unresolved HTML placeholder");
await mkdir(new URL("../web/dist/", import.meta.url), { recursive: true });
await writeFile(new URL("../web/dist/index.html", import.meta.url), html, "utf8");
console.log("Generated web/dist/index.html (standalone)");
