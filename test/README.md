# Comprobaciones de regresión

[Aplicación](../README_APP.md) · [Verificador de setup](../scripts/README.md).

Esta carpeta no se compila dentro del firmware ni se distribuye con el HTML.
Los archivos JS existentes se han adaptado a CONFIG v2 y la interfaz de setup.

| Archivo | Cobertura |
| --- | --- |
| `web_protocol.test.mjs` | Estado, configuración, orden de bytes, mapping de canales, fragmentos y desbordamiento |
| `web_app.test.mjs` | Conexión, Start/Stop, reconexión, sensor ausente y HTML local con GATT simulado |
| `captures.test.mjs` | CSV UTF-8, pull/CRC/MTU, freeze, cancelación, ACK diferido y doble clic |
| `storage_test.cpp` | DataStore real: FIFO, snapshots, CSV, IDs, CRUD, fallos y recuperación |
| `capture_ble_test.cpp` | CaptureBLE real: caché ACK, PIN histórico, tokens, idempotencia y sesiones |

Desde la raíz, con Node y sin paquetes externos:

```powershell
node scripts/build-web.mjs
node test/web_protocol.test.mjs
node test/web_app.test.mjs
node test/spectral_chart.test.mjs
node test/captures.test.mjs
node scripts/verify-setup.mjs
scripts\test-storage.cmd
```

El último script añade los casos del hito 4: APPLY con ACK diferido, validación,
readback cuantizado simulado, rechazos y configuración desconocida.
Incluye también navegación Measurement/Setup durante adquisición, conservación de
sesión/formulario, lectura física de VIS y estados VALID/SATURATED. El DOM de prueba
común se implementa en `scripts/web-test-dom.mjs`, fuera del HTML de producción.
Todos son ensayos sin hardware; no prueban las escrituras I2C, el rollback real,
la radio ni el navegador visual. Seguir la
[matriz de aceptación](../docs/HITO_3_BLE.md#pasos-de-aceptación-en-hardware).

## Capturas: software y límites de los ensayos

Los tests C++ incluyen directamente los `.cpp` de producción; mocks en `host/`
sustituyen solo Arduino/String, LittleFS/File, Preferences/NVS, BLE y colas.
El script usa MSVC C++17 local y deja artefactos en `.pio/host`. No se compilan
estos mocks dentro del firmware. Inyecta fallo de escritura, fallo de cada
rename y reset entre las fases de reemplazo, además de corruptos y mount fallido.
Valida FIFO exactamente 100 tras captura 101, preservación de filas no borradas,
contador tras borrar todo/reiniciar, idempotencia y CRC/bytes descargados.

`capture_ble_test.cpp` comprueba que leer inmediatamente después de escribir
**antes de Update** no produce un falso ACK; después comprueba PIN/config
histórica aunque se reciban muestras/config nuevas, deduplicación de LOG,
BUSY, token incorrecto, liberación tras 15 s y recibo exacto del borrado.
`captures.test.mjs` ejecuta el modal/transport reales con ACK retenido, nuevos
samples durante el modal, CANCELAR, doble clic y desconexión; comprueba un CSV
vacío y uno de 100 filas, UTF-8/comillas, fragmentación y rechazo por CRC.

No se ha validado alimentación, integridad física de flash, I2C ni radio real.
El filesystem falso no simula desgaste, sectores parcialmente programados ni
fallos de NVS eléctricos. Tampoco certifica apariencia visual en navegador.

## Lista de aceptación en ESP32 real (pendiente)

1. Aprovisionar LittleFS explícitamente solo si procede; usar filesystem existente
   cuando haya datos. Confirmar que mount fallido no autoformatea ni bloquea medida.
2. Con sensor, LOG con descripción `prueba, "café"`: una fila, canales, setup,
   full scale y uptime exactos. Dejar entrar nuevas muestras mientras el modal
   está abierto; cambiar setup antes de confirmar y comprobar captura histórica.
3. Cancelar: ninguna fila. STOP: LOG guarda la última muestra e indica parada.
   Sin conexión/muestra: botón deshabilitado; firmware rechaza PIN inválido.
4. Probar 1/99/100/101 capturas: en la 101 quedan exactamente 100, falta solo la
   más antigua. Borrar varias, insertar, comprobar que se aprovechan las plazas.
5. Reset con datos, luego borrar todos y reset: IDs nuevos siempre mayores.
   Ensayar cortes controlados antes/entre/después de los renames, conservar copia
   de datos y verificar recuperación; archivo corrupto debe bloquear reescritura.
6. VIEW: revisar los 18 canales por nombres, MAX/MIN, SAT 90/95 %, VIS TL/BR,
   modos SMUX6/12 con celdas ausentes y full scale histórico diferente del actual.
7. DOWNLOAD CSV: comparar bytes/tamaño/CRC contra `/data.csv` real, cabecera única,
   36 columnas por fila, comillas/UTF-8, CSV solo cabecera, máximo 100 filas.
   Interrumpir/cancelar transferencia y comprobar que LOG/DELETE se liberan.
8. Cortar BLE durante LOG antes/después de commit: reconectar, refrescar lista,
   verificar si se guardó sin retry automático ni filas duplicadas. Cortar durante
   descarga y borrado; comprobar selección y estado coherentes al reconectar.
9. Verificar Measurement/Setup/Debug, START/STOP, configuración y LED AUTO durante
   las operaciones. Medir tiempos `[DATA] LOG/DELETE duration=...` con 100 filas,
   continuidad de notificaciones y tiempo de descarga con el navegador real.

La comparación superpuesta queda pendiente hasta completar esta lista. Ningún
ensayo físico se da por pasado solo por compilar o ejecutar estas simulaciones.

## Transferencias rápidas y gestión CSV

Pruebas nuevas sobre implementación real:

- DataStore: proyección de 100 resúmenes sin canales, CRC/tamaño exactos, bloques
  de 504 cruzando filas, repetición de offset y GET de una fila sin cabecera.
- CaptureBLE: FAST_BEGIN/FAST_NEXT con respuesta variable de hasta 512 bytes,
  token/correlación, tres tipos de consulta y convivencia con legacy.
- Web: bytes idénticos del CSV, reducción de comandos NEXT (hasta 42 veces),
  resúmenes sin muestras, VIEW de una fila y reutilización sin nuevos comandos,
  fallback con firmware anterior o Read largo truncado, CRC y cancelación.

En hardware: actualizar firmware/HTML sin uploadfs; medir REFRESH, primer VIEW,
segundo VIEW y GET CSV con 1/100 filas, durante adquisición y con el navegador
habitual. Confirmar bytes/CRC y comportamiento con MTU bajo y alto. Los tests no
certifican una aceleración de 42 veces en segundos ni negociación MTU real.

## Persistencia NVS del setup

`nvs_manager_test.cpp` ejecuta Save/Load y ConfigMath de producción con Preferences
simulado. Verifica todos los ajustes editables, LED OFF/ON/AUTO, integración
binary64 exacta, pines/frecuencia preservados, deduplicación de escrituras y
aislamento del namespace de IDs. Cubre ausencia, blob truncado, versión/CRC/LED/
gain/periodo inválidos y errores de lectura, escritura y readback.
`verify-setup.mjs` cubre el resultado de persistencia fallido: configuración nueva
visible y APPLIED / NOT SAVED, sin dejar bloqueados START/APPLY.

Pendiente hardware: aplicar setup/AUTO, reset/ciclo de alimentación, reconectar
y comprobar cada campo y LED AUTO en START/STOP. Rechazar una petición inválida
y confirmar que el siguiente arranque recupera el último setup válido. Verificar
primera ejecución sin clave y conservación de CSV/IDs. No se han simulado los
efectos eléctricos de un corte durante nvs_commit ni se da ese ensayo por pasado.
