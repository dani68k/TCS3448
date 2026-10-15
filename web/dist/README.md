# Interfaz local lista para utilizar

[Cliente Web](../README.md) · [Decisión de navegación](../../ARCHITECTURE.md).

Abrir `index.html` con doble clic. Incluye estilos y scripts, sin archivos
auxiliares ni servidor. Solo este HTML se necesita para distribuir o publicar.
Debe utilizarse un navegador que permita Web Bluetooth en el contexto abierto.

La navegación interna tiene dos vistas:

- Measurement: `index.html#/measurement`.
- Setup: `index.html#/setup`.

No abrir otro HTML para cambiar de vista: este documento conserva la misma sesión
BLE al navegar. Recargar, cerrar o abrir otra copia crea una sesión distinta.

No editarlo a mano: modificar `web/src/` y ejecutar desde la raíz
`node scripts/build-web.mjs`. La siguiente generación sobrescribe la salida.
El resultado no contiene datos de demostración ni simuladores: conecta al ESP32.
