# TCS3448 — Protocolo BLE y aceptación (hitos 3 y 4)

[Arquitectura global](../README_APP.md) · [Gestor BLE](../src/ble/README.md) ·
[Gestor del sensor](../src/TCS3448Manager/README.md).

Esta guía conserva su ruta original, ampliada con setup remoto del hito 4.
La referencia de campos de transporte está aquí; las responsabilidades internas
están en los README de cada módulo.

## Servicio GATT v1

Nombre anunciado: **TCS3448**. Servicio:
`34480001-7f2a-4f6c-9e31-8dbe0c2f5a70`.
El framing previo STATUS/SAMPLE conserva versión 1; CONFIG usa versión 2.
Actualizar firmware y HTML juntos. Se mantiene binario, sin mensajes JSON.
Todos los enteros multibyte son little-endian, sin enviar structs C++ en memoria.

| Característica | UUID | Propiedades |
| --- | --- | --- |
| STATUS | `34480002-7f2a-4f6c-9e31-8dbe0c2f5a70` | Read, Notify |
| COMMAND | `34480003-7f2a-4f6c-9e31-8dbe0c2f5a70` | Write con respuesta |
| SAMPLE | `34480004-7f2a-4f6c-9e31-8dbe0c2f5a70` | Notify |
| CONFIG | `34480005-7f2a-4f6c-9e31-8dbe0c2f5a70` | Read |

### Comandos

ASCII exacto sin CR/LF: START, STOP, GET_STATUS y GET_CONFIG. Sus identificadores
en STATUS son 1, 2, 3 y 4. APPLY_CONFIG es el identificador 5 y utiliza el frame
siguiente; no se divide en escritura de parámetros y comando separado.

### APPLY_CONFIG — una escritura de 20 bytes

| Offset | Tipo | Contenido |
| --- | --- | --- |
| 0 | u8 | Etiqueta 0xA4 |
| 1 | u8 | Versión de configuración: 2 |
| 2 | u8 | Gain, código propio 0..12 |
| 3 | u8 | SMUX: 6, 12 o 18 |
| 4 | u8 | Autozero propio: Disabled 0, First 1, Every 2 |
| 5 | u8 | LED enabled: 0/1 |
| 6 | u16 | Corriente LED en mA (incluye 258 sin truncar) |
| 8 | u32 | Integración solicitada por ciclo, en microsegundos |
| 12 | u32 | Periodo solicitado en ms |
| 16 | 4 bytes | Reservados, obligatoriamente cero |

Una sola escritura cabe en el payload de 20 bytes del MTU mínimo. Payload con
longitud/versión/booleano/reservados inválidos devuelve INVALID_CONFIG_PAYLOAD.
El gestor valida el resto antes de cualquier escritura. No se exponen registros,
ATIME/ASTEP, bits SMUX ni direcciones I2C.

### STATUS — 20 bytes, versión 1

| Offset | Tipo | Contenido |
| --- | --- | --- |
| 0 | u8 | Versión: 1 |
| 1 | u8 | Ready: configuración hardware conocida, 0/1 |
| 2 | u8 | RUNNING 1 / STOPPED 0 |
| 3 | u8 | Error de la última orden o del sensor |
| 4 | u8 | Comando: ninguno 0, START 1, STOP 2, GET_STATUS 3, GET_CONFIG 4, APPLY_CONFIG 5, inválido/cola 255 |
| 5 | u8 | Resultado |
| 6 | u16 | Contador de respuestas, con desbordamiento |
| 8 | u32 | Total de muestras correctas desde arranque |
| 12 | u32 | Uptime en ms |
| 16 | u32 | Sample period en ms |

Resultados: OK 0, UnknownCommand 1, SensorError 2, QueueFull 3, InvalidConfig 4,
ConfigApplyFailed 5. Errores de sensor: OK 0, Initialization 1, Configuration 2,
LED 3, I2C 4, Timeout 5, ConfigApplyFailed 6.

| Error de validación | Código |
| --- | --- |
| INVALID_GAIN | 16 |
| INVALID_INTEGRATION | 17 |
| INVALID_LED_CURRENT | 18 |
| INVALID_SMUX_MODE | 19 |
| INVALID_AUTOZERO | 20 |
| INVALID_SAMPLE_PERIOD | 21 |
| SAMPLE_PERIOD_TOO_SHORT | 22 |
| INVALID_CONFIG_PAYLOAD | 23 |

No confundir error de la última petición con adquisición activa: un rechazo de
configuración puede coexistir con RUNNING sin alterar el sensor. Una escritura
GATT confirmada no es un ACK de aplicación. Esperar contador nuevo y comando
correspondiente. Cola de ocho mensajes; el cliente envía una petición cada vez.

### CONFIG — 20 bytes, versión 2

| Offset | Tipo | Contenido |
| --- | --- | --- |
| 0 | u8 | Versión: 2 |
| 1 | u8 | Gain, código propio 0..12 |
| 2 | u8 | SMUX: 6, 12 o 18 |
| 3 | u8 | Autozero: Disabled 0, First 1, Every 2 |
| 4 | u8 | LED realmente aplicado: 0/1 |
| 5 | u8 | Configuración confirmada: 0/1 |
| 6 | u16 | Corriente LED en mA |
| 8 | u32 | Integración REAL por ciclo, redondeada a microsegundos |
| 12 | u32 | Periodo activo en ms |
| 16 | u16 | ADC full scale calculado desde ajustes verificados, limitado a 65535 |
| 18 | u16 | Revisión de configuración, con desbordamiento |

Gain 0..12: 0,5x, 1x, 2x, 4x, 8x, 16x, 32x, 64x, 128x, 256x, 512x, 1024x,
2048x. Los códigos son del protocolo, sin importar enums Adafruit en Web/BLE.
La revisión sube con APPLY correcto y cambios de LED por seguridad. Sample period
es del planificador; los demás campos hardware se confirman por lectura I2C.
Con confirmed=0 los valores son la última copia, no se pueden presentar como activos.

### GET_CONFIG, CONFIG_ACK y atomicidad

GET_CONFIG verifica la copia activa contra los registros y publica CONFIG antes
de confirmar la consulta. La Web lee CONFIG después de ver la respuesta. Si el
sensor no está disponible, conserva la conexión y muestra configuración desconocida.

APPLY valida todos los campos y, solo después, pausa/escribe/verifica el hardware.
En éxito, actualiza la configuración y reanuda si estaba RUNNING. Antes del ACK
prepara CONFIG; STATUS confirma comando 5, resultado 0 y contador nuevo. La Web
lee CONFIG y muestra el tiempo REAL, no el solicitado.

Ante validación fallida se publica la configuración anterior sin cambiar el sensor
ni RUNNING. Ante error de escritura se intenta restaurar todos los campos anteriores
y verificarlos. Siempre se detiene la adquisición en ese caso. Si el rollback se
verifica, CONFIG conserva la anterior con confirmed=1; si no, confirmed=0 y se
intenta apagar LED. El resultado/error son ConfigApplyFailed. No hay garantía de
transacción I2C indivisible ni persistencia tras reiniciar.

El cliente espera hasta 5 s y consulta STATUS cada 100 ms mientras espera. Si hay
timeout, no informa éxito: debe consultar GET_CONFIG antes de reintentar. No existe
un request-id adicional; se presupone una sola orden pendiente por cliente.

### SAMPLE — 48 bytes reconstruidos, versión 1

| Offset | Tipo | Contenido |
| --- | --- | --- |
| 0 | u8 | Versión: 1 |
| 1 | u8 | Gain del frame, código 0..12 |
| 2 | u8 | Saturación: 0/1 |
| 3 | u8 | Número de resultados válidos: 6, 12 o 18 |
| 4 | u32 | Secuencia de muestra |
| 8 | u32 | Timestamp de detección del resultado en ms desde arranque |
| 12 | 18 × u16 | ADC crudos; resultados no válidos rellenados con cero |

Orden de campos del protocolo, truncado a channelCount:
FZ, FY, FXL, NIR, VIS_TL_0, VIS_BR_0, F2, F3, F4, F6, VIS_TL_1, VIS_BR_1,
F1, F7, F8, F5, VIS_TL_2, VIS_BR_2. No mostrar ceros de canales inactivos como
medidas válidas. Secuencia y timestamp desbordan como uint32; no se reinician por
START/STOP, APPLY o reconexión, solo por reiniciar el ESP32.

### Fragmentación existente de muestras

Cuatro notificaciones de 20 bytes, para no depender de negociar MTU mayor:

| Offset | Tipo | Contenido |
| --- | --- | --- |
| 0 | u8 | Versión: 1 |
| 1 | u8 | Índice del fragmento: 0..3 |
| 2 | u8 | Número de fragmentos: 4 |
| 3 | u8 | Reservado: cero |
| 4 | u32 | Secuencia de muestra |
| 8 | 12 bytes | Datos de muestra desde índice × 12 |

Intervalo mínimo 15 ms entre fragmentos; solo se envían con conexión/suscripción.
Si un frame sigue pendiente, no se reemplaza: se omite transmitir el nuevo. Sin
retransmisión ni histórico. El cliente rechaza pérdidas, desorden, secuencias
mezcladas o reconstrucciones de más de 2 s. STOP, APPLY y desconexión cancelan
fragmentos pendientes; la Web también limpia su ensamblador.

## Arranque y uso local

Seguir [README_APP.md](../README_APP.md#arrancar-y-probar). El firmware usa una
aplicación de 3 MiB sin segundo slot OTA y se carga por USB. Abrir el HTML local
`web/dist/index.html` con doble clic en un navegador compatible. No requiere
servidor ni archivos auxiliares. Node solo se utiliza para regenerar/verificar.

START/STOP controlan adquisición y no fuerzan LED. Aplicar LED ON ilumina incluso
estando STOPPED. Por seguridad, desconexión/fallo intentan apagar LED y actualizar
la configuración. Reconexión recupera STOPPED y LED OFF; no reanuda por sí sola.

## Pasos de aceptación en hardware

1. **Conectar.** Cargar firmware/HTML del mismo hito. Connect → TCS3448. Confirmar
   STOPPED, ACTIVE CONFIG y REQUESTED rellenados desde firmware, LED OFF inicial.
2. **Aplicar detenido.** Gain 128x, integración 50 ms, LED 20 mA ON, SMUX 18,
   autozero First, periodo 500 ms. APPLY. Esperar ACK y comprobar el tiempo real
   cuantizado y ADC full scale; nunca mostrar éxito antes del ACK.
3. **Rechazos.** Probar LED 3/5/21/260, gain 13, integración 0, SMUX no válido,
   autozero no válido y periodo 0 con un cliente de prueba que pueda omitir validación
   JS. Configuración/revisión y RUNNING deben quedar iguales. La Web bloquea los
   errores evidentes; la validación definitiva debe comprobarse también en firmware.
4. **Periodo mínimo.** Con integración 50,04 ms, SMUX18 y First: 180 ms debe
   rechazar SAMPLE_PERIOD_TOO_SHORT; 181 ms supera la política de validación.
   Comprobar tiempos reales en la placa. Every requiere presupuesto de tres autozeros.
5. **Adquisición.** Start, variar iluminación y comprobar muestras completas.
   Modificar gain, integración, LED corriente/ON/OFF con APPLY estando RUNNING.
   Verificar cambio real en counts y que continúa RUNNING tras éxito.
6. **SMUX.** Probar 6, 12 y 18; comprobar exactamente ese número de resultados
   válidos, sin mostrar valores antiguos de ciclos inactivos. Verificar ganancia y
   saturación coherentes; no confundir ausencia de canal con lectura cero válida.
7. **Límites.** LED 4 y 258 mA deben aceptarse sin truncar. Verificar corriente
   acorde al hardware conectado. Probar ganancia 0,5x/2048x y ADC full scale limitado
   a 65535 con integración larga y periodo suficiente.
8. **Start/Stop.** Repetir ambos: sin reiniciar secuencia, sin forzar LED. Stop
   cancela la medida y notificaciones nuevas; puede quedar tráfico ya encolado.
9. **Reconexión.** Desconectar durante RUNNING/APPLY y cerrar pestaña o apagar
   Bluetooth en ensayos separados. Volver a conectar: STOPPED, LED OFF y nueva
   lectura real del resto de parámetros. Repetir diez ciclos.
10. **Fallo hardware/rollback.** Provocar un fallo I2C controlado durante APPLY;
    comprobar ConfigApplyFailed, adquisición detenida y configuración previa solo
    si el rollback se verifica. Si no, UNKNOWN y START/APPLY bloqueados. Corregir
    conexión y reiniciar. No se considera rollback probado solo con simulación JS.
11. **Sensor ausente.** Arrancar sin sensor; conectar BLE para ver error y CONFIG
    desconocida, con START/APPLY bloqueados. Restaurar sensor y reiniciar.
12. **Continuidad.** Adquirir diez minutos con cambios periódicos de setup; registrar
    pérdidas, errores y desconexiones. Reiniciar para verificar valores de arranque.

## Verificación automática y límites

```powershell
pio run -e esp32dev
node --check web/src/app.mjs
node --check web/src/protocol.mjs
node scripts/build-web.mjs
node scripts/verify-setup.mjs
node test/web_protocol.test.mjs
node test/web_app.test.mjs
```

El verificador usa GATT simulado y ejecuta el script del HTML final: comprueba
validación cliente, bytes, ACK no optimista, rechazos, reconexión y canales. No
prueba los setters, los tiempos del sensor, la recuperación I2C real ni el aspecto
visual de la Web. Completar la aceptación anterior con ESP32 y navegador reales.
