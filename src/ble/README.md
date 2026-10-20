# Gestor BLE Web del TCS3448

[Aplicación](../../README_APP.md) · [Gestor del sensor](../TCS3448Manager/README.md).

El módulo real sigue en `src/ble/`, namespace `TCS3448BLE`. No se ha movido a
otra carpeta. Expone GATT, encola comandos, coordina el gestor y publica ACK,
estado, configuración y muestras. No incluye ni llama a Adafruit_TCS3448.

## Archivos y API

| Archivo | Contenido |
| --- | --- |
| `ble.h` | `TCS3448BLE::Init()` y `Update()` |
| `ble.cpp` | Callbacks, cola, sesiones, ejecución y publicación |
| `BLEProtocol.h` | Identificadores, códigos estables, encoder y decoder de APPLY |

Init se llama una vez después del gestor, incluso si el sensor falló. Update se
llama frecuentemente desde loop. Depende de Arduino BLE, FreeRTOS y tipos propios
del gestor. Las tareas Bluetooth no ejecutan operaciones I2C.

## Servicio y mensajes

Nombre: `TCS3448`. Servicio: `34480001-7f2a-4f6c-9e31-8dbe0c2f5a70`.
Características con sufijo `-7f2a-4f6c-9e31-8dbe0c2f5a70`:

| Prefijo | Característica | Propiedades / formato |
| --- | --- | --- |
| `34480002` | STATUS | Read + Notify, 20 bytes, versión 1 |
| `34480003` | COMMAND | Write con respuesta; ASCII o APPLY binario |
| `34480004` | SAMPLE | Notify, cuatro fragmentos de 20 bytes, versión 1 |
| `34480005` | CONFIG | Read, 20 bytes, versión 3 |
| `34480006` | CAPTURES | Read + Write con respuesta, 20 bytes, protocolo de datos |

START, STOP, GET_STATUS y GET_CONFIG siguen siendo ASCII sin terminador. APPLY_CONFIG
usa un paquete de 20 bytes etiquetado 0xA4 con parámetros completos. El protocolo
de muestras sigue siendo binario y conserva sus cuatro fragmentos. Los
[offsets y códigos](../../docs/HITO_3_BLE.md#servicio-gatt-v1) son la referencia de wire.
El cliente del hito 3 no interpreta CONFIG v2; actualizar firmware y HTML juntos.

## Recepción, ACK y errores

Los callbacks interpretan el comando y encolan una copia de sus datos con la
sesión actual. Cola: ocho elementos, sin espera; hasta ocho órdenes procesadas
por Update. Un cambio de sesión descarta órdenes antiguas. Un exceso de cola
se comunica con comando 255 y resultado QueueFull 3.

GET_CONFIG llama RefreshConfig del gestor: verifica los registros antes de
publicar la instantánea. Fallo de lectura implica resultado SensorError y CONFIG
no válida. Para APPLY, BLE copia pines/frecuencia del gestor y entrega únicamente
los parámetros remotos a ApplyConfig. No contiene cálculos de registros.

El ACK lógico de APPLY consta de STATUS y su CONFIG asociada: primero se prepara
CONFIG con la configuración verificada (o la anterior tras rechazo/rollback), luego
se incrementa el contador de respuestas y se publica STATUS. El cliente espera
comando 5 y contador nuevo, comprueba resultado/error y lee CONFIG. La revisión
permite identificar aplicaciones, pero no es un identificador de petición.
Solo debe haber una orden de aplicación pendiente por cliente.

| Resultado | Código | Interpretación |
| --- | --- | --- |
| Ok | 0 | Orden ejecutada |
| UnknownCommand | 1 | Texto desconocido |
| SensorError | 2 | Falló inicio/parada o lectura de configuración |
| QueueFull | 3 | Cola llena; no asumir aceptación |
| InvalidConfig | 4 | Candidato o payload rechazado |
| ConfigApplyFailed | 5 | Fallo hardware de aplicación/recuperación |

El detalle ocupa el byte de error en STATUS: errores de sensor 1..6 o validaciones
16..23. Los rechazos no alteran el estado del gestor; el error de la última orden
se conserva en BLE hasta otra orden. Un error nuevo de adquisición lo sustituye.
No enviar ASCII APPLY_CONFIG solo: se reconoce, pero devuelve INVALID_CONFIG_PAYLOAD.

## Adquisición y sesiones

START inicia el planificador sin forzar LED. STOP cancela medida y fragmentos,
conservando el estado LED configurado. APPLY cancela la transmisión pendiente;
el gestor pausa/reanuda la adquisición solo tras aplicación correcta.

Cada muestra mantiene 48 bytes, con channelCount 6/12/18 y posiciones no válidas a
cero. Se divide con el framing existente: versión, índice 0..3, total 4, reservado,
secuencia uint32 y 12 bytes de datos. Se espacia al menos 15 ms entre fragmentos.
No se transmite sin suscripción; si aún hay un frame pendiente, la muestra nueva
se omite. No hay histórico ni retransmisión.

Al cambiar sesión se pide StopAcquisition(true), se apaga LED y se actualiza CONFIG.
Desconectar reactiva anuncio tras 200 ms y borra suscripciones; reconectar requiere
volver a suscribirse, consultar configuración y pulsar Start. CONFIG del gestor
es la única fuente de verdad, no una copia editable independiente en BLE.

El estado se publica tras órdenes, sesiones y errores, y cada segundo conectado.
Read devuelve la última instantánea; GET_STATUS fuerza actualización. Serial
registra recepción de APPLY y CONFIG_ACK con el nombre del error, sin logs por loop.

## Verificación

Compilar `pio run -e esp32dev`; generar HTML y ejecutar
`node scripts/verify-setup.mjs` para comprobar protocolo y aplicación simulada.
La simulación no ejecuta esta cola ni los callbacks reales; completar la
[prueba en hardware](../../docs/HITO_3_BLE.md#pasos-de-aceptación-en-hardware),
incluyendo cambio de parámetros durante RUNNING y desconexión durante APPLY.

## Extension LED AUTO

CONFIG v3: byte 4 es modo LED 0 OFF, 1 ON, 2 AUTO. APPLY v3: byte 5 usa esos
mismos codigos; el resto de offsets y los 20 bytes no cambian. Se acepta APPLY
v2 para manual OFF/ON. START/STOP controlan el LED en AUTO; manual conserva
su estado con STOP. Desconexion/fallo apagan ambos modos y conservan AUTO.
Actualizar firmware y HTML juntos para utilizar AUTO.

## Capturas manuales

`CaptureBLE.h/.cpp` añade una cola independiente de 8 solicitudes de 20 bytes,
procesada desde Update. `Init` monta DataStore; `Remember` asocia cada muestra
que se va a transmitir con configuración y ADC reales (32 entradas acotadas).
PIN fija una entrada; no se vuelve a medir al confirmar. Reset de sesión limpia
PIN/transferencias/recibos y descarta solicitudes de sesiones anteriores.

### Paquete CAPTURES

UUID `34480006-7f2a-4f6c-9e31-8dbe0c2f5a70`, Read + Write con respuesta. Usa
Las solicitudes tienen exactamente 20 bytes, enteros little endian. Sin notify en
esta característica: solicitudes Write With Response y polling Read; la ruta rápida
usa respuestas largas. Los
samples continúan por su característica original, sin mezclar streams.

| Bytes | Solicitud | Respuesta |
| --- | --- | --- |
| 0 | opcode | mismo opcode |
| 1 | reservado 0 | error: 0 = OK |
| 2..5 | request_id uint32 no cero, nuevo en cada operación | mismo request_id |
| 6..7 | sequence uint16 (chunk = offset / 12) | misma sequence |
| 8..19 | payload, resto cero | resultado / hasta 12 bytes |

El navegador inicia su contador de solicitudes desde un uint32 aleatorio con
crypto y lo incrementa. No acepta respuesta hasta que coincidan opcode, request
y sequence; timeout 6 s por operación. Firmware publica la respuesta **después**
de procesar la cola; onRead devuelve una caché protegida por sección crítica.
El valor escrito de la característica nunca se interpreta como ACK antes de
procesarse. Cola llena produce timeout, sin falso éxito ni retry automático.

Offsets de payload siguientes son relativos al byte 8 del paquete:

| Opcode | Operación | Solicitud | Respuesta OK |
| --- | --- | --- | --- |
| 1 | PIN_CAPTURE | sequence uint32 @0 | secuencia fijada uint32 @0; request de PIN es token de captura |
| 2 | STAGE | offset uint16 @0, longitud uint8 @2 (0..9), bytes @3 | sin datos |
| 3 | LOG_CAPTURE | token PIN uint32 @0, longitud descripción uint16 @4 | record_id uint32 @0, count uint16 @4 |
| 4 | BEGIN / LIST_RECORDS / DOWNLOAD_DATA_CSV | sin payload | longitud uint32 @0, CRC32 uint32 @4, count uint16 @8 |
| 5 | NEXT | offset uint32 @0, token BEGIN/GET uint32 @4 | hasta 12 bytes exactos; último bloque rellenado con ceros fuera de longitud |
| 6 | END | token BEGIN/GET uint32 @0 | sin datos |
| 7 | DELETE_RECORDS | cantidad uint16 @0 | count restante uint16 @0, cantidad borrada uint16 @2 |
| 8 | GET_RECORD | record_id uint32 @0, positivo | longitud/CRC/count como BEGIN, cabecera + fila exacta |
| 9 | RESET_STAGE | sin payload | sin datos |
| 10 | DELETE_RECEIPT | offset uint32 @0, request DELETE uint32 @4 | hasta 12 bytes de IDs uint32 borrados |

STAGE exige offsets consecutivos y máximo 400 bytes; PIN o RESET_STAGE lo limpia.
LOG exige longitud exacta, descripción de hasta 80 bytes y PIN de la sesión.
El request de PIN se guarda en CSV y hace idempotente repetir LOG con ese token:
misma captura no añade otra fila, incluso después de reiniciar DataStore. La web
no reintenta una escritura dudosa; refresca desde ESP32 al reconectar.

DELETE: confirmar IDs en navegador, RESET_STAGE, subir IDs uint32 little endian
mediante STAGE, ejecutar DELETE. El firmware rechaza cero, repetidos, más de 100
o cualquier ID desconocido antes de modificar el archivo. Tras ACK la web lee
DELETE_RECEIPT y compara todos los IDs exactos con la selección confirmada.
Recibo limitado a 400 bytes y a la sesión; no ofrece operaciones de paths.

La tabla anterior describe el transporte legacy, preservado para compatibilidad.
La web actual prefiere estas operaciones adicionales:

| Opcode | Operación | Payload solicitud | Respuesta |
| --- | --- | --- | --- |
| 11 | FAST_BEGIN | tipo uint8 @0 (0=CSV, 1=resúmenes, 2=registro), ID uint32 @4 (solo tipo 2) | 20 bytes: longitud uint32 @8, CRC32 @12, count uint16 @16, capacidad uint16 @18 = 504 (offsets absolutos) |
| 12 | FAST_NEXT | offset uint32 @0, token FAST_BEGIN uint32 @4; sequence = offset / 504 | 8 bytes correlación + datos, longitud **8 + min(504, total-offset)** |

FAST_NEXT guarda hasta 512 bytes en la caché de respuestas procesadas. onRead
copia la caché; no modifica el archivo ni avanza el cursor desde la tarea BLE.
Se revisó `BLECharacteristic.cpp`: onRead se invoca en la lectura inicial y el
framework continúa mediante Read Blob sobre el mismo valor para un MTU menor.
`BLEDevice::setMTU(517)` ofrece un MTU alto; no fuerza el MTU elegido por el host.
Los UUID, escrituras de 20 bytes y notificaciones SAMPLE anteriores no cambian.

CSV devuelve los bytes exactos del archivo. Resúmenes solo contiene cabecera de
seis columnas e ID/descripción/uptime/gain/integración/corriente. GET_RECORD devuelve
solo la fila con 36 columnas y LF, sin repetir cabecera. Ninguno crea archivos.
El listado se genera por streaming. Todos anuncian tamaño/CRC antes de transferir;
el último bloque no añade ceros al contenido válido. END (6) conserva su token.

El navegador prueba FAST_BEGIN una vez por conexión. INVALID_PAYLOAD indica
firmware legacy y activa 4/5/8. Si el host trunca un Read largo, cierra primero
con END y repite la consulta por legacy. CRC incorrecto no se considera éxito.
No se reintentan LOG/DELETE. El navegador también conserva una caché de registros
verificados para VIEW, invalidada al refrescar, mutar o desconectar.

Ambas rutas bloquean mutaciones hasta END, desconexión o 15 s de inactividad,
validan token/offset/sequence y mantienen adquisición. Hay progreso/cancelación.
El bloque útil aumenta 12 -> 504: hasta 42 veces menos comandos NEXT; velocidad
real pendiente de medir con MTU/radio. No cambia `/data.csv` ni requiere uploadfs.

| Código | Error |
| --- | --- |
| 0 | OK |
| 1 | NO_SAMPLE |
| 2 | INVALID_DESCRIPTION |
| 3 | STORAGE_UNAVAILABLE |
| 4 | STORAGE_FULL |
| 5 | RECORD_NOT_FOUND |
| 6 | INVALID_RECORD_IDS |
| 7 | BUSY |
| 8 | CSV_CORRUPT |
| 9 | WRITE_FAILED |
| 10 | TRANSFER_FAILED |
| 11 | INVALID_PAYLOAD |

Una caída durante LOG puede dejar la fila persistida y el ACK perdido; comprobar
Data al reconectar. No hay reenvíos automáticos. Las mutaciones son síncronas en
loop; reportan duración por serie y pueden retrasar una muestra. No se promete
latencia hasta medir con flash y radio reales. [CSV y recuperación](../DataStore/README.md).

Pruebas nativas ejecutan el CaptureBLE de producción con GATT/colas simulados:
ACK procesado, PIN histórico, LOG idempotente, BUSY, tokens, timeout, recibo de
borrado y aislamiento de sesión. No prueban comportamiento eléctrico de la radio.

## Persistir APPLY_CONFIG

Después de un ApplyConfig correcto, BLE guarda GetConfig con NVSManager antes
de publicar el ACK. Si el sensor rechaza, no toca NVS. Resultado STATUS 6 + error
24 (`CONFIG_SAVE_FAILED`) significa aplicado en hardware pero no confirmado
persistente; CONFIG contiene los valores nuevos. No invalida ready ni detiene
la adquisición por un fallo exclusivamente de NVS. Éxito 0 confirma también
persistencia con este firmware. UUIDs y tamaños de paquetes siguen iguales.
[Gestor y arranque](../nevs_manager/README.md).
