# Herramientas de la aplicación

[Aplicación](../README_APP.md) · [Web](../web/README.md).

No requieren paquetes npm. Node se necesita para desarrollar/verificar; no para
abrir el HTML generado. Ejecutar desde la raíz del proyecto.

## build-web.mjs

```powershell
node scripts/build-web.mjs
```

Lee plantilla, CSS, protocolo, SpectralChart, UI y aplicación de `web/src/`; integra los cuatro módulos
en un ámbito privado como script clásico y escribe `web/dist/index.html` UTF-8.
Resuelve rutas respecto al propio script. El HTML resultante es autocontenido.
Retira BOM UTF-8 antes de incrustar fuentes, para que no invalide selectores CSS.
Incrusta `web/images/logo-signal.png` como imagen de cabecera y favicon mediante
una URI de datos, sin necesitar archivos auxiliares al abrir el HTML.

Es un generador específico para estos módulos, no un bundler general. Rechaza
imports/exports no resueltos y marcadores pendientes. Si se añaden módulos,
adaptarlo y volver a verificar que no hay dependencias externas en el HTML.
No compila firmware, no publica en GitHub y sobrescribe cambios manuales del HTML.

## verify-setup.mjs

```powershell
node scripts/verify-setup.mjs
```

Usa `node:test`, asserts, VM y archivos estándar de Node, sin dependencias externas.
Antes hay que regenerar el HTML. Verifica el protocolo desde las fuentes y ejecuta
el script incluido en el HTML final con DOM/GATT simulados.

Cubre límites de validación cliente, LED 258 mA, tamaño/orden de bytes,
CONFIG v2, muestras v1 de 6/12/18, fragmentos incompletos, conexión,
ACK diferido sin éxito optimista, lectura real cuantizada simulada, rechazo,
rollback no confirmado simulado, reconexión y sensor ausente.
También cubre navegación Measurement/Setup sin desconexión ni órdenes adicionales,
recepción en la vista oculta, campos editados, canales físicos/VIS y estados visuales.

`web-test-dom.mjs` comparte un DOM mínimo entre verificadores, construido desde los
IDs del HTML real. Solo sirve a pruebas: no se integra en el HTML del producto.

El dispositivo es simulado: no ejecuta la máquina de estados C++ ni los registros
reales. Para esas partes se requiere compilación y
[aceptación en hardware](../docs/HITO_3_BLE.md#pasos-de-aceptación-en-hardware).
El script retorna fallo si una aserción no se cumple. Mantenerlo junto al protocolo
al cambiar mensajes. Las regresiones de `test/` se documentan en su
[README](../test/README.md); son independientes de este verificador.
