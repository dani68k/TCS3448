# Arquitectura TCS3448: firmware, BLE, web y capturas

```text
TCS3448Manager -> TCS3448Sample -> BLE SAMPLE -> Measurement
                    |                            | LOG
                    v                            v
       CaptureBLE: historial + PIN congelado <- modal
                    |
                 DataStore -> LittleFS /data.csv
                    |              |
              GET / DELETE     CSV pull + CRC32
                    |              |
                    +-------> Data (SPA)
```

## Firmware y fuente de verdad

`main.cpp` coordina el manager, BLE y entrada serie. El manager conserva los
ajustes reales confirmados por hardware y gobierna LED AUTO. SAMPLE v1 no cambia:
48 bytes reconstruidos desde cuatro notificaciones de 20 bytes. STATUS v1 y
CONFIG v3 conservan UUIDs, campos y comandos anteriores.

Antes de enviar una muestra, `CaptureBLE::Remember` copia muestra, configuración
y ADC Full Scale reales en un anillo de 32 entradas. No consulta Adafruit desde
DataStore ni toma una segunda lectura. La web congela secuencia/uptime al pulsar
LOG y solicita PIN de esa secuencia antes de abrir el modal. Si la muestra ya
no está retenida, NO_SAMPLE; nunca se sustituye por otra. PIN conserva una copia
independiente del anillo: las notificaciones y cambios de setup posteriores no
alteran la captura. STOP conserva la última muestra válida; desconexión invalida
la elegibilidad para LOG. Un nuevo APPLY confirmado limpia la muestra visible.

Callbacks BLE solo encolan bytes. Las operaciones de datos, archivos y sensor
se ejecutan en loop. El ACK de datos se guarda en una caché protegida: la lectura
GATT no devuelve como ACK el eco de una escritura aún no procesada. Sesiones
invalidan comandos, PIN, transferencia y respuestas de la conexión anterior.

## SPA y estados

Un HTML autocontenido generado desde `web/src`, sin dependencias de red. Rutas:

| Ruta | Vista |
| --- | --- |
| `#/measurement` | Histograma RAW, START/STOP, LOG |
| `#/setup` | Configuración solicitada y confirmada |
| `#/data` | Saved captures, tabla, selección, replay, borrar, descargar |
| `#/debug` | RAW SMUX, estado y mensajes BLE |

La navegación cambia hash/hidden, conserva una sola sesión Web Bluetooth,
suscripciones, adquisición y formulario. Cabecera y menú sticky compartidos.
`app.mjs` coordina conexión y busy; `captures.mjs` implementa el modal/tabla y
transporte de datos. Configuración confirmada del ESP32 y CSV remoto son fuentes
de verdad; el navegador no asigna IDs ni mantiene una base local de capturas.

LOG está habilitado con conexión y muestra válida, RUNNING o STOPPED. El modal
muestra secuencia/uptime congelados, avisa de adquisición parada y admite
80 bytes UTF-8 opcionales. CANCELAR no escribe. GUARDAR bloquea doble clic,
espera el ACK persistente y solo entonces muestra ID y contador. Si se pierde
el ACK, no se reintenta automáticamente: reconectar/refrescar Data para conocer
el resultado. El token de PIN queda en la fila para deduplicar el commit.

Data lista IDs de más reciente a más antiguo, aunque el archivo sea ascendente.
Se refresca desde ESP32 al reconectar. VIEW ejecuta GET_RECORD remoto y reproduce
esa fila con su escala/ajustes históricos. Checkbox selecciona una o varias filas;
la confirmación enumera IDs. Borrado valida todos antes de escribir y el cliente
verifica la lista de IDs del recibo de ACK. Si desaparece la fila elegida se
limpia el gráfico; desconectado queda identificado como Last loaded record.

## Histograma compartido

`ui.mjs::createRawHistogram` reutiliza `createSpectralChart` junto a las barras VIS
para Measurement y Data. Se crean dos instancias del mismo componente, sin
muestras ficticias. Mantiene orden, colores, MAX/MIN con empates, counts originales
y ADC Full Scale fijo correspondiente a la muestra. SAT amarillo desde
round(0.90 * escala), rojo desde round(0.95 * escala); VIS queda excluido.
Los canales no adquiridos muestran raya, nunca cero inventado. VIS TL/BR son
el ciclo 0; los ciclos adicionales siguen en RAW y en el CSV. El flag agregado de
saturación no identifica canales ni permite inferir flags analógicos/digitales.

## Persistencia e integridad

`DataStore` recorre y valida el único `/data.csv`; máximo 100 filas. IDs uint32
reservados en Preferences/NVS antes de escribir: crecen tras reinicio y borrado
total, con huecos admisibles ante fallo. Inserción 101 descarta solo la más antigua.
Descripciones escapadas, UTF-8, cabecera única de 36 campos, uptime de adquisición,
setup y full scale históricos; flags no disponibles = `unknown`.

Partición existente `huge_app.csv`, 4 MiB, aplicación de 3 MiB y LittleFS de
896 KiB, configurado con `board_build.filesystem`. Mount sin autoformat; si falla,
se informa STORAGE_UNAVAILABLE y la adquisición sigue disponible. Se reescribe
por streaming a temporal, valida tamaño/CRC/tipos/recuento, renombra original a
respaldo, promueve temporal y verifica antes de ACK. Reinicio restaura respaldo
si falta original; corrupción presente impide mutaciones destructivas.

[DataStore README](src/DataStore/README.md) define **esquema CSV exacto**, tipos,
provisión inicial, límites de RAM, IDs, fallos y recuperación. No hay calibración,
registro periódico, sesiones, SD, Wi-Fi, HTTP ni base de datos.

## Protocolo y descarga

Nueva característica `34480006-7f2a-4f6c-9e31-8dbe0c2f5a70`, Read/Write con
respuesta. Cada operación usa request32 y secuencia16; paquetes de 20 bytes
para MTU ATT 23. PIN, STAGE, LOG, BEGIN/LIST/DOWNLOAD, NEXT, END, DELETE,
GET_RECORD, RESET_STAGE y DELETE_RECEIPT. [Offsets exactos](src/ble/README.md#capturas-manuales).

La ruta rápida separa tres consultas: LIST transmite solo ID, descripción,
uptime, gain, integración y corriente LED; GET_RECORD transmite una sola fila
sin repetir la cabecera; GET CSV transmite el archivo íntegro e idéntico.
FAST_BEGIN (11) informa bytes/CRC/count y bloque de 504; FAST_NEXT (12) devuelve
8 bytes de correlación y hasta 504 bytes de datos. Una lectura GATT larga obtiene
hasta 512 bytes; el framework gestiona Read Blob si el MTU negociado es menor.
El ESP32 ofrece MTU 517; el navegador/SO decide el valor efectivo.

El listado se proyecta por streaming en RAM acotada, sin otro archivo ni escritura
de flash. GET_RECORD recorre hasta el ID solicitado, valida las filas leídas y
no recalcula el CRC de todo el CSV. Las mutaciones mantienen su validación completa.
El navegador verifica longitud/CRC antes de usar datos o crear Blob UTF-8 exacto.
Conserva registros ya obtenidos en una caché por conexión, invalidada en REFRESH,
LOG/DELETE y desconexión: repetir VIEW no transfiere nada. Si el firmware anterior
solo ofrece LIST como CSV completo, reutiliza las filas verificadas para VIEW.

El protocolo anterior 4/5/8 permanece disponible. Se detecta firmware anterior
por INVALID_PAYLOAD en FAST_BEGIN; si un host trunca lecturas largas se libera
la transferencia y repite solo la lectura con bloques de 12 bytes. No se reintenta
una mutación. END, desconexión o 15 s sin actividad liberan el bloqueo. La
adquisición sigue funcionando y las mutaciones responden BUSY durante transferencia.
El transporte rápido reduce hasta 42 veces las solicitudes de bloque, sin prometer
ese factor en tiempo real: medir MTU/radio con la placa y el navegador.

## Verificación y trabajo pendiente

`node scripts/build-web.mjs` genera `web/dist/index.html`. Tests JS de protocolo,
Setup, navegación, renderer y capturas; tests nativos ejecutan DataStore y
CaptureBLE de producción con FS/NVS/GATT/colas simulados. Incluyen FIFO,
recuperación ante fallos, freeze, ACK diferido, idempotencia, CRC y aislamiento
de sesiones. [Comandos y matriz hardware](test/README.md).

La compilación PlatformIO y estos tests de software no validan flash/radio/I2C
físicos. Pendientes: reset/cortes en cada fase, LED AUTO/adquisición concurrente,
CSV idéntico en placa, reconexión y duración con 100 filas. La superposición
Compare with live measurement queda **pendiente**, sin implementarla antes de
cerrar esa validación. No se normalizan counts ni se interpreta RAW como absorbancia.

## Ajuste automático al viewport

En escritorio (viewport desde 960 × 560 CSS px), main distribuye el alto libre
con flex/grid. Measurement comparte ese espacio entre cabecera de controles,
SVG flexible y VIS; Setup reduce espaciado y mantiene dos columnas editables.
Data coloca lista y replay lado a lado; solo la tabla desplaza sus filas, con
cabecera fija. Debug conserva scroll dentro de sus paneles. No se usa zoom ni
se transforma el documento para hacerlo caber: el SVG conserva proporciones.

En ventanas más pequeñas se conserva el diseño responsive con desplazamiento
para mantener controles y texto accesibles. Mensajes extensos y el modal tienen
scroll local cuando lo requieren. La navegación y BLE no se modifican.

## Setup persistente en NVS

`src/nevs_manager/nvs_manager.h/.cpp` encapsula Preferences en namespace
`tcs3448_setup`, clave `config`. main recupera un blob de 32 bytes validado
(versión, CRC y ConfigMath) antes de inicializar el sensor. Si falta o falla,
conserva defaults sin borrar NVS. Se almacenan los campos editables de Setup,
incluido modo LED AUTO; bus I2C y RUNNING no se restauran desde NVS.

APPLY_CONFIG aplica/verifica hardware y después guarda el GetConfig confirmado.
El ACK de éxito espera el guardado y readback NVS. Un fallo de persistencia
produce result=6/error=24 CONFIG_SAVE_FAILED: los ajustes ya están aplicados y
la web muestra APPLIED / NOT SAVED. No se guardan peticiones rechazadas, estados
transitorios LED ni muestras. Reaplicar ajustes idénticos evita reescribir.
[NVS manager](src/nevs_manager/README.md) define formato, errores y pruebas.
