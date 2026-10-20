# DataStore: capturas manuales

`DataStore` conserva un solo archivo lógico `/data.csv` en LittleFS. No toma
muestras: recibe una copia `Snapshot` con `TCS3448Sample`, configuración confirmada
y ADC Full Scale correspondiente a esa adquisición. [Arquitectura](../../ARCHITECTURE.md).

## API y límites

`Init`, `Insert`, `Delete`, `Open`, `Read`, `Close`, `Count`; firmas en `DataStore.h`.
Todas las llamadas se ejecutan desde loop, nunca desde callbacks Bluetooth.
Máximo 100 filas de datos. La inserción 101 elimina exclusivamente la primera
(menor ID); borrar filas libera plazas. CSV ascendente; tabla web descendente.
La memoria está acotada: recorrido secuencial, una línea de hasta 700 bytes y
validación de hasta 36 campos; no se carga el CSV completo en RAM ni se usa PSRAM.
El recorrido de validación usa además 100 request IDs (400 bytes) y buffer CRC de
128 bytes. GET_RECORD materializa solamente una fila (sin cabecera en el protocolo rápido).

## CSV exacto

UTF-8 sin BOM, separador coma, decimales con punto y terminador LF en todas las
líneas, incluida la última. `.gitattributes` fija LF para el CSV de provisión
también en Windows. Una cabecera, sin metadatos adicionales:

```csv
record_id,description,timestamp_ms,gain,integration_ms,led_current_ma,led_mode,auto_smux,autozero,sample_period_ms,adc_full_scale,f1,f2,fz,f3,f4,f5,fy,fxl,f6,f7,f8,nir,vis_tl,vis_br,valid,analog_saturation,digital_saturation,sequence,channel_count,request_id,saturated,vis_tl_1,vis_br_1,vis_tl_2,vis_br_2
```

- `record_id`: uint32 positivo, estable; no representa posición de fila.
- `description`: opcional, hasta **80 bytes UTF-8**, sin controles ni saltos de
  línea. Se entrecomilla siempre y las comillas internas se duplican. Firmware
  rechaza UTF-8 mal formado; la web comprueba bytes, no solamente maxlength.
- `timestamp_ms`: uptime uint32 del ESP32 al adquirir, no UTC; envuelve después
  de unos 49,7 días. `sequence` es la secuencia original uint32 de adquisición.
- `gain`: factor real de la muestra (0.5, 1.0, ..., 2048.0). `integration_ms`:
  integración efectiva del manager, con cinco decimales. `led_current_ma` es el
  valor confirmado; `led_mode` = OFF/ON/AUTO de esa adquisición. AUTO identifica
  el modo gobernado por firmware, aunque el LED está apagado posteriormente.
- `auto_smux` y `channel_count`: 6, 12 o 18; `autozero`: 0/1/2, mismos códigos del
  modelo. `sample_period_ms`: periodo confirmado; `adc_full_scale`: uint16 histórico.
- Canales RAW uint16. Un canal no adquirido tiene celda **vacía**, nunca cero
  inventado. F1/F2/FZ/F3/F4/F5/FY/FXL/F6/F7/F8/NIR siguen el mapping del protocolo;
  VIS TL/BR son el ciclo 0. Los cuatro campos VIS adicionales preservan ciclos
  1/2 para reproducir todos los canales originales sin promediar.
- `valid` = 1 para estas capturas verificadas. `analog_saturation` y
  `digital_saturation` = `unknown`: el driver solo entrega `saturated` agregado,
  guardado separadamente como 0/1. No se deducen flags analógicos/digitales.
- `request_id`: token uint32 de PIN, persistido para impedir duplicar un LOG.
  Repetirlo devuelve el mismo ID si toda la fila histórica coincide;
  una colisión con datos diferentes da INVALID_PAYLOAD. La web no reintenta LOG
  automáticamente. La deduplicación persiste mientras se conserve esa fila.

Los ajustes de bus I2C no son parámetros de adquisición; permanecen en el manager.
No se inventan registros ATIME/ASTEP ni estados de saturación separados.

## Partición y primer uso

`esp32dev`, flash 4 MiB, `huge_app.csv` existente: aplicación 0x300000 bytes,
partición de datos `spiffs` en 0x310000, tamaño **0xE0000 = 896 KiB**. El nombre
histórico de la partición no impide usar LittleFS. Se configura
`board_build.filesystem = littlefs`; `LittleFS.begin(false)` **no autoformatea**.
Si no monta, STORAGE_UNAVAILABLE; adquisición/BLE siguen disponibles.

`data/data.csv` local contiene solo cabecera para preparar una imagen inicial.
`pio run -t buildfs` crea esa imagen sin grabarla. Una partición virgen necesita
aprovisionamiento explícito; `uploadfs` sustituye el filesystem entero, por lo
que no se debe usar sobre capturas que deban conservarse. Esta implementación
no ejecuta uploadfs ni formatea la placa. Un archivo existente vacío es
CSV_CORRUPT: no se reinterpreta como un archivo nuevo.

## IDs y confirmación persistente

Preferences/NVS, namespace `captures`, clave `last`, reserva el siguiente ID
**antes** de reescribir. Fallos/reset pueden dejar huecos; no se reutilizan IDs
tras borrar todas las filas o reiniciar. Al montar, se eleva el contador al mayor
ID existente si fuera necesario; nunca se reduce. Agotamiento uint32 devuelve
STORAGE_FULL. NVS y filesystem son necesarios para mutaciones; un borrado externo
de NVS no es parte del mecanismo de reinicio soportado.

## Escritura segura y recuperación

1. Validar cabecera, 36 columnas, tipos, UTF-8, presencia de canales según SMUX,
   IDs estrictamente ascendentes, request IDs únicos y límite de 100. Cualquier
   corrupción impide reescribir; se conserva el original para recuperación manual.
2. Recorrer `/data.csv` y escribir `/data.tmp`, filtrando solo IDs pedidos o la
   primera fila en FIFO. Verificar cada escritura, flush, cierre, tamaño, CRC32,
   esquema y recuento del temporal frente al contenido que se pretendía escribir.
3. Con el original todavía válido, retirar un respaldo residual, renombrar
   `/data.csv` a `/data.bak` y `/data.tmp` a `/data.csv`. Los destinos están ausentes:
   no se depende de sobrescritura de rename. Se inspeccionó el framework instalado:
   `FS/src/vfs_api.cpp::VFSImpl::rename` delega a `::rename` y retorna su resultado;
   `LittleFS.cpp` usa esp_littlefs. No se presupone una transacción entre renames.
4. Revalidar tamaño/CRC del nuevo archivo antes del ACK. Solo entonces retirar
   el respaldo. Si falla el segundo rename, intentar restaurar el original.
5. Tras reset: un `data.csv` válido es autoritativo; si falta, restaurar un
   `data.bak` validado. Nunca promover un temporal incompleto. Un CSV presente
   corrupto se conserva y se reporta, incluso si hay respaldo; no se sustituye
   silenciosamente. Temporal/respaldo son transitorios, no sesiones ni otros CSV.

Se exige margen libre conservador de 78 192 bytes antes de insertar. Fallos de
escritura preservan el original y devuelven WRITE_FAILED; espacio insuficiente,
STORAGE_FULL. La integridad física frente a cortes depende de LittleFS y flash:
los tests de fallos simulados no sustituyen ensayos de alimentación en la placa.

## Transferencias y ocupación

`Open(0)` valida y abre el CSV íntegro; el modo legacy `Open(id)` devuelve
cabecera + fila exacta. `Open(..., Transfer::Record)` sirve solo la fila, con LF,
y recorre/valida hasta el ID solicitado, sin leer después ni recalcular el CRC
de todo el archivo. Las mutaciones siempre validan el archivo entero.

`Transfer::Summaries` proyecta seis columnas:
`record_id,description,timestamp_ms,gain,integration_ms,led_current_ma`.
Calcula tamaño/CRC por recorrido y después transmite filas proyectadas en orden
ascendente usando un buffer de una fila, sin cargar el listado entero ni escribir
otro archivo. Permite repetir offsets reconstruyendo el cursor virtual.

`Read` acepta capacidad 12 (legacy) o 504 (rápida), offsets múltiplos de esa
capacidad y devuelve la longitud real del último bloque. La transferencia bloquea
Insert/Delete con BUSY hasta Close, desconexión o 15 segundos de inactividad.
No detiene adquisición. [Wire BLE](../ble/README.md#capturas-manuales).
CRC32 IEEE (polinomio reflejado 0xEDB88320, inicio/final XOR 0xFFFFFFFF).
El esquema persistente de 36 columnas permanece igual: no requiere uploadfs ni
migración para actualizar firmware/web a la transferencia rápida.

Las mutaciones son síncronas y ceden CPU durante recorridos. Pueden retrasar
una lectura/notificación de adquisición; no se garantiza tiempo real durante
compactación. Cada LOG/DELETE informa duración por serie, sin logs periódicos.
Medir en placa con 100 filas antes de establecer una garantía de latencia.

## Pruebas

`test/storage_test.cpp` compila el DataStore real con FS/NVS simulados:
FIFO 1/99/100/101, CSV/UTF-8/comillas, setup histórico, canales ausentes, borrado,
IDs/reinicio/vaciado, duplicados, CRC, locks, fallos de escritura/rename y reset.
`test/capture_ble_test.cpp` ejecuta CaptureBLE real con colas/GATT simulados.
Ver [ejecución y límites](../../test/README.md). Comparación live/guardado pendiente
hasta completar validación física; no está implementada.
