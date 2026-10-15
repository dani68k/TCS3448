# Gestor BLE Web de ATLANTES

Este módulo ocupa `src/ble/`; su namespace es `AtlantesBLE`. No existe una carpeta
`BLEWebManager/` separada. [Visión general](../../README_APP.md).

## Responsabilidad y archivos

Expone el servicio GATT, recibe comandos, coordina su ejecución con
`TCS3448Manager` y publica estado, configuración y muestras. Depende de la
biblioteca BLE incluida en Arduino-ESP32 y de la cola FreeRTOS; no accede
directamente a registros I2C.

| Archivo | Función |
| --- | --- |
| `ble.h` | API `AtlantesBLE::Init()` y `AtlantesBLE::Update()` |
| `ble.cpp` | Servicio, callbacks, sesiones, cola y envío periódico |
| `BLEProtocol.h` | UUID, versión, comandos/resultados y codificación little-endian |

Llamar Init una vez después de inicializar el sensor y Update de forma frecuente
desde `loop()`. BLE permanece consultable si el sensor no pudo inicializarse.

## Servicio y órdenes

Nombre anunciado: `ATLANTES-TCS3448`.
Servicio: `a71a0001-3448-4c53-8a7e-41544c414e54`.
Las características comparten el sufijo `-3448-4c53-8a7e-41544c414e54`:

| Prefijo UUID | Característica | Propiedades |
| --- | --- | --- |
| `a71a0002` | Estado, 20 bytes | Read + Notify |
| `a71a0003` | Comandos ASCII | Write con respuesta |
| `a71a0004` | Fragmentos de muestra, 20 bytes | Notify |
| `a71a0005` | Configuración, 20 bytes | Read |

Comandos exactos, sin CR/LF: START 1, STOP 2, GET_STATUS 3 y GET_CONFIG 4.
Ninguno es 0 e inválido es 255. Resultados: Ok 0, UnknownCommand 1,
SensorError 2 y QueueFull 3. GET_CONFIG actualiza la copia serializada de la
configuración del gestor; no cambia parámetros del sensor.

El callback de escritura interpreta el texto y lo encola con su sesión. La cola
tiene ocho elementos y no bloquea al callback; si se llena, el bucle publica
QueueFull con comando 255. Update procesa hasta ocho órdenes por llamada.

Cada orden procesada incrementa un contador uint16 de respuestas y publica el
estado. El cliente debe esperar una confirmación antes de enviar la siguiente;
la respuesta de escritura GATT solo confirma la aceptación de la escritura.

## Estado y transmisión

Los paquetes de estado y configuración tienen 20 bytes. La muestra reconstruida
tiene 48 bytes: versión, ganancia, saturación, número de canales, secuencia,
timestamp y 18 ADC. Los formatos completos y offsets están en el
[protocolo del hito 3](../../docs/HITO_3_BLE.md#servicio-gatt-v1).

Una muestra se divide en cuatro notificaciones de 20 bytes:

| Bytes | Contenido |
| --- | --- |
| 0 | Versión 1 |
| 1 | Índice 0..3 |
| 2 | Total de fragmentos: 4 |
| 3 | Reservado: cero |
| 4..7 | Secuencia uint32 little-endian |
| 8..19 | 12 bytes de muestra desde índice × 12 |

Se transmite un fragmento cada al menos 15 ms, solo con conexión y suscripción
activa. Si una muestra llega mientras aún se transmite otra, la nueva no se envía.
No hay cola de histórico ni retransmisión. El contador de adquisición puede ser
mayor que el número de muestras recibidas por el navegador.

El estado se publica tras órdenes y cambios de sesión, ante cambios de error de
adquisición y al menos cada segundo durante conexión. Read devuelve la última
instantánea publicada; GET_STATUS fuerza su actualización.

## Conexión, tareas y errores

Los callbacks actualizan conexión/sesión o encolan comandos. Update detecta
cambios de sesión, detiene adquisición, cancela fragmentos pendientes y descarta
comandos antiguos. Tras desconexión reinicia el anuncio después de 200 ms.
Las suscripciones se deshabilitan al desconectar; el navegador debe renovarlas
al reconectar. No se reanuda adquisición automáticamente.

El bucle realiza la adquisición y serialización. Las operaciones del sensor no
se ejecutan en la tarea Bluetooth. Los errores del sensor se comunican en estado
y como SensorError cuando falla una orden de inicio/parada.

Para cambiar el protocolo, actualizar `BLEProtocol.h`, los serializadores de
`ble.cpp`, `web/src/protocol.mjs` y la documentación de offsets, y regenerar el
HTML. Validar reconexiones, START/STOP, cola y muestras incompletas siguiendo la
[guía de prueba](../../docs/HITO_3_BLE.md#pasos-de-aceptación-en-hardware).
