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

START, STOP, GET_STATUS y GET_CONFIG siguen siendo ASCII sin terminador. APPLY_CONFIG
usa un paquete de 20 bytes etiquetado 0xA4 con parámetros completos. El protocolo
ya era binario: no se añade JSON ni una nueva fragmentación. Los
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
