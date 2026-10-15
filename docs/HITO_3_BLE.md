# Hito 3 — ATLANTES / TCS3448 BLE Web

Implementación local de firmware y cliente Web Bluetooth. La compilación y las
pruebas automáticas no verifican por sí mismas la estabilidad de radio ni el
funcionamiento del sensor físico: completar los pasos de aceptación de abajo.

## Módulos

- `src/main.cpp`: inicialización y llamadas de servicio.
- `src/TCS3448Manager/`: configuración, inicio/parada y sondeo de medidas.
- `src/ble/`: servicio GATT, cola de comandos, sesiones y transmisión.
- `src/ble/BLEProtocol.h`: UUID, identificadores y codificación del protocolo v1.
- `web/dist/index.html`: página autocontenida para abrir con doble clic o publicar.
- `web/src/`: fuentes de la página; `scripts/build-web.mjs` genera el HTML final.
- `test/web_protocol.test.mjs`: decodificación y recuperación de fragmentos.
- `test/web_app.test.mjs`: flujo de la aplicación con un dispositivo simulado.

No hay comandos serie activos. El puerto serie conserva los mensajes de diagnóstico.

## Comportamiento

El dispositivo se anuncia como **ATLANTES-TCS3448**. Arranca detenido, con el LED
apagado. `START` enciende el LED e inicia la adquisición; `STOP` cancela la medida
en curso, descarta la transmisión pendiente y apaga el LED. Ambos son idempotentes.
La desconexión detiene la adquisición y el LED en el siguiente ciclo de `loop()`;
el anuncio se reactiva tras 200 ms. Reconectar requiere pulsar Start de nuevo.

Los callbacks BLE solo encolan órdenes. El bucle principal realiza las operaciones
I2C, comprueba si hay un resultado y atiende las notificaciones. No espera a que
termine la integración. Ante error I2C o timeout, detiene la adquisición y publica
el error. START puede reintentar un fallo de adquisición; si falló la inicialización,
revisar el sensor y reiniciar el ESP32. BLE permite consultar el fallo sin sensor.

Configuración inicial: SDA 4, SCL 5, I2C 400 kHz, ganancia 64x, ATIME 29,
ASTEP 599, LED 10 mA y periodo solicitado 500 ms. La integración calculada es
50,04 ms **por ciclo**; se usan tres ciclos SMUX. La cadencia real depende de
la integración y del transporte; no es una garantía de tiempo real.

`TCS3448Sample` contiene secuencia, timestamp del ESP32, ganancia y saturación
del mismo frame, y los 18 resultados ADC. Secuencia y timestamp son uint32 y
pueden desbordarse. La secuencia empieza en 1 y solo se reinicia al reiniciar el
ESP32; no al hacer STOP o reconectar. No son valores calibrados ni lux.

## Servicio GATT v1

UUID de servicio: `a71a0001-3448-4c53-8a7e-41544c414e54`.

| Característica | UUID | Propiedades |
| --- | --- | --- |
| Estado | `a71a0002-3448-4c53-8a7e-41544c414e54` | Read, Notify |
| Comandos | `a71a0003-3448-4c53-8a7e-41544c414e54` | Write con respuesta |
| Muestras | `a71a0004-3448-4c53-8a7e-41544c414e54` | Notify |
| Configuración | `a71a0005-3448-4c53-8a7e-41544c414e54` | Read |

Cada comando es texto ASCII exacto, sin terminador: `START`, `STOP`, `GET_STATUS`,
`GET_CONFIG`. La escritura GATT acepta la orden; el estado confirma su ejecución
con contador de respuesta y resultado. El cliente envía una orden cada vez y espera
su confirmación antes de continuar. Cola de ocho órdenes; las de sesiones anteriores
se descartan. Un desbordamiento se comunica como resultado 3, comando 255.

GET_CONFIG actualiza la característica de configuración; después de recibir la
confirmación, leerla. GET_STATUS actualiza y notifica el estado. El estado se
actualiza tambión cada segundo mientras hay conexión y ante fallos. No se puede
modificar la configuración desde la página.

Todos los enteros multibyte se codifican **little-endian**; nunca se transmite la
representación en memoria de un struct C++. La versión del protocolo es 1.

### Estado — 20 bytes

| Offset | Tipo | Contenido |
| --- | --- | --- |
| 0 | u8 | Versión |
| 1 | u8 | Sensor inicializado: 0/1 |
| 2 | u8 | Adquisición activa: 0/1 |
| 3 | u8 | Error del sensor |
| 4 | u8 | Último comando: ninguno 0, START 1, STOP 2, GET_STATUS 3, GET_CONFIG 4, inválido 255 |
| 5 | u8 | Resultado: OK 0, desconocido 1, error sensor 2, cola llena 3 |
| 6 | u16 | Contador de respuestas; incremento con cada respuesta, con desbordamiento |
| 8 | u32 | Total de muestras adquiridas desde arranque |
| 12 | u32 | Uptime del ESP32 en ms |
| 16 | u32 | Periodo solicitado en ms |

Errores: 0 ninguno, 1 inicialización, 2 configuración, 3 LED, 4 comunicación I2C,
5 timeout. `ready` indica inicialización conseguida; consultar tambión `error` para
conocer fallos posteriores. Con `ready=0`, la configuración contiene los valores
previstos, sin afirmar que se hayan aplicado al hardware.

### Configuración — 20 bytes

| Offset | Tipo | Contenido |
| --- | --- | --- |
| 0 | u8 | Versión |
| 1 | u8 | Código de ganancia |
| 2 | u8 | ATIME |
| 3 | u8 | Corriente del LED en mA |
| 4 | u16 | ASTEP |
| 6 | u16 | Periodo solicitado en ms |
| 8 | u32 | Integración por ciclo en microsegundos, redondeada |
| 12 | u32 | Frecuencia I2C en Hz |
| 16 | u8 | SDA |
| 17 | u8 | SCL |
| 18 | 2 bytes | Reservados, cero |

Códigos de ganancia 0..12: 0,5x, 1x, 2x, 4x, 8x, 16x, 32x, 64x, 128x,
256x, 512x, 1024x y 2048x.

### Muestra reconstruida — 48 bytes

| Offset | Tipo | Contenido |
| --- | --- | --- |
| 0 | u8 | Versión |
| 1 | u8 | Código de ganancia del frame |
| 2 | u8 | Saturación: 0/1 |
| 3 | u8 | Número de canales: 18 |
| 4 | u32 | Secuencia |
| 8 | u32 | Timestamp de lectura en ms desde arranque |
| 12 | 18 × u16 | Valores ADC en orden SMUX |

Orden: FZ, FY, FXL, NIR, VIS_TL_0, VIS_BR_0, F2, F3, F4, F6,
VIS_TL_1, VIS_BR_1, F1, F7, F8, F5, VIS_TL_2, VIS_BR_2.

### Fragmentación de muestras

Cada muestra se envía en cuatro notificaciones de **20 bytes**, compatibles con
el MTU ATT mónimo de 23 sin exigir negociar un MTU mayor.

| Offset | Tipo | Contenido |
| --- | --- | --- |
| 0 | u8 | Versión |
| 1 | u8 | índice del fragmento: 0..3 |
| 2 | u8 | Número de fragmentos: 4 |
| 3 | u8 | Reservado, cero |
| 4 | u32 | Secuencia de la muestra |
| 8 | 12 bytes | Tramo de la muestra: offsets índice × 12 hasta índice × 12 + 11 |

Se deja un mónimo de 15 ms entre fragmentos. No se reemplaza una transmisión a
medias por una muestra nueva. Si el transporte está ocupado, se omite el envío de
la muestra nueva. No hay retransmisión ni almacenamiento histórico: la secuencia
permite detectar omisiones. El cliente descarta muestras incompletas, fuera de
orden, con secuencia inconsistente o con más de 2 s entre inicio y fin. Al conectar
se activan de nuevo las notificaciones; no se reutilizan suscripciones anteriores.

## Abrir la página localmente

Abrir **`web/dist/index.html` con doble clic** en un navegador compatible con
Web Bluetooth, por ejemplo Chrome de escritorio, con Bluetooth habilitado.
No necesita Python, Node ni servidor para usarla. El ESP32 debe tener cargado el
firmware BLE; pulsar Connect permite seleccionar ATLANTES-TCS3448.

El HTML incluye estilos y JavaScript clásico. No carga módulos, archivos externos
ni recursos de red. Puede copiarse como un único archivo a otro directorio o equipo.
Se detecta la disponibilidad de Web Bluetooth y del contexto seguro: si el navegador
restringe la API para archivos locales, la página muestra el aviso correspondiente.
Los navegadores pueden aplicar restricciones adicionales a `file://`, como indica
la [especificación de contextos seguros](https://www.w3.org/TR/secure-contexts/).

Para publicarla después, subir ese mismo `index.html` a GitHub Pages u otro
alojamiento HTTPS. Aún no se ha publicado. La comunicación con el ESP32 sigue siendo
BLE desde el navegador; no necesita Wi-Fi en el ESP32.

### Modificar la página

Editar los archivos de `web/src/` y regenerar el HTML (sin instalar dependencias):

```powershell
node scripts/build-web.mjs
```

Conservar `web/dist/index.html` actualizado en el repositorio. No editarlo a mano,
porque se genera a partir de las fuentes. Las pruebas de la aplicación ejecutan el
script incluido en ese mismo HTML.

## Pasos de aceptación en hardware

1. **Servicio y conexión.** Compilar/subir el firmware. Pulsar Connect y elegir
   ATLANTES-TCS3448. Comprobar sensor inicializado, adquisición detenida y valores
   de configuración. Disconnect y Connect deben funcionar varias veces seguidas.
2. **Adquisición.** Pulsar Start; deben aparecer muestras completas, secuencias
   crecientes y los 18 canales. Variar la iluminación y comprobar la respuesta.
   Pulsar Stop: el LED debe apagarse y el total adquirido quedar fijo al actualizar
   el estado. Start debe reanudar con la secuencia siguiente, sin reiniciar el contador.
3. **Recuperación.** Desconectar durante adquisición, cerrar la pestaña y apagar el
   Bluetooth del ordenador en ensayos separados. Comprobar que el ESP32 detiene la
   adquisición al detectar la pérdida de conexión, vuelve a anunciarse y al reconectar
   sigue detenido. Repetir al menos diez ciclos Connect/Start/Stop/Disconnect.
4. **Sensor ausente.** Arrancar sin el sensor y comprobar que se puede conectar,
   consultar el error y leer los valores de configuración previstos; Start debe quedar
   deshabilitado. Restaurar el sensor y reiniciar para recuperar la inicialización.
5. **Continuidad.** Mantener adquisición durante al menos diez minutos. Registrar
   desconexiones, saltos de secuencia y errores; comprobar que el número recibido
   aumenta y no se muestran muestras mezcladas. Este paso valida la estabilidad real.

Las pruebas de fallos de conexión pueden hacerse abriendo directamente el HTML local;
no requieren mantener ningún servidor en ejecución.

## Verificación automática

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
node scripts/build-web.mjs
node --check web/src/app.mjs
node --check web/src/protocol.mjs
node --test test/web_protocol.test.mjs test/web_app.test.mjs
```

Las pruebas de la aplicación usan un GATT simulado: validan controles, confirmaciones,
errores y reconexión, pero no prueban las tareas del ESP32, la radio ni la interfaz
visual en un navegador. El soporte de reconexión exige volver a suscribirse segón la
[especificación Web Bluetooth](https://bluetooth.spec.whatwg.org/).
