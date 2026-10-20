# Interfaz local lista para utilizar

[Cliente Web](../README.md) · [Decisión de navegación](../../ARCHITECTURE.md).

Abrir `index.html` con doble clic. Incluye estilos y scripts, sin archivos
auxiliares ni servidor. Solo este HTML se necesita para distribuir o publicar.
Debe utilizarse un navegador que permita Web Bluetooth en el contexto abierto.

La navegación interna tiene cuatro vistas:

- Measurement: `index.html#/measurement`.
- Setup: `index.html#/setup`.
- Data: `index.html#/data` (capturas manuales, VIEW, borrado y CSV).
- Advanced / Raw Debug: `index.html#/debug`.

No abrir otro HTML para cambiar de vista: este documento conserva la misma sesión
BLE al navegar. Recargar, cerrar o abrir otra copia crea una sesión distinta.

No editarlo a mano: modificar `web/src/` y ejecutar desde la raíz
`node scripts/build-web.mjs`. La siguiente generación sobrescribe la salida.
El resultado no contiene datos de demostración ni simuladores: conecta al ESP32.

LOG fija la muestra al abrir el modal y espera ACK del ESP32 para confirmar.
Data se refresca al reconectar. Descargar conserva bytes y verifica CRC32;
CANCEL TRANSFER libera la transferencia. No hay datos demo ni grabación periódica.
La comparación live/guardado está pendiente de validación física.
[Persistencia e inicialización](../../src/DataStore/README.md).

La versión optimizada pide resúmenes en REFRESH y una sola fila en VIEW; conserva
las filas consultadas en memoria por conexión. GET CSV usa hasta 504 bytes útiles
por lectura, con CRC32 y fallback legacy. Para la mejora completa, cargar firmware
nuevo y abrir este HTML regenerado. El esquema CSV no cambia: no subir filesystem.

Las vistas se ajustan automáticamente al espacio de escritorio disponible. Data
muestra tabla y replay en paralelo, con scroll solo en la lista; Debug usa
scroll en sus paneles. En ventanas pequeñas se permite scroll para no ocultar
controles. Este ajuste es solo web: abrir el HTML regenerado, sin cargar firmware.

Con el firmware actualizado, APPLY CONFIGURATION guarda también en NVS. Tras
reiniciar, Setup lee los valores recuperados, incluido LED AUTO. Si falla solo
la persistencia, APPLIED / NOT SAVED distingue los valores aplicados de los
guardados. Actualizar firmware y este HTML; no requiere uploadfs.
