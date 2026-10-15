# Herramientas de generación

[Aplicación](../README_APP.md) · [Web](../web/README.md).

`build-web.mjs` genera `web/dist/index.html`. Requiere Node con soporte para módulos
ES y las API estándar de archivos; no necesita instalar paquetes npm.

Desde la raíz:

```powershell
node scripts/build-web.mjs
```

Lee `web/src/index.template.html`, `style.css`, `protocol.mjs` y `app.mjs`.
Integra los estilos y convierte los dos módulos conocidos en un script clásico
dentro de un ámbito privado. Sustituye los marcadores de la plantilla y escribe
el HTML UTF-8. Las rutas se resuelven respecto al propio script.

Es un generador específico para estos archivos, no un empaquetador JavaScript
general: rechaza imports/exports que queden sin resolver. Si se añaden módulos,
adaptar el generador y comprobar de nuevo que el HTML funciona de manera autónoma.
Los errores de lectura, dependencias inesperadas o marcadores sin resolver
interrumpen la generación.

Solo afecta a la Web: no compila ni carga el firmware, y no publica en GitHub.
Node no se necesita para abrir el HTML ya generado. Verificar sintaxis de las
fuentes y probar conexión/controles con el HTML generado después de cada cambio.
