# Fuentes de la Web

[Cliente Web](../README.md) · [Arquitectura de presentación](../../ARCHITECTURE.md).

## Estructura

| Archivo | Cometido |
| --- | --- |
| `index.template.html` | Un documento con Measurement, Setup, Data y Debug; estado comun |
| `style.css` | Variables y componentes dark, grid/flex y focus/contraste |
| `ui.mjs` | Presentación, rutas hash y readouts; sin acceso Bluetooth |
| `app.mjs` | Única sesión GATT, comandos y enlaces con la presentación |
| `protocol.mjs` | Protocolo existente, decodificación y validación; no cambia con el diseño |

`ui.mjs` exporta `physicalReadings(sample)` y `createInstrumentUI(elements)`.
El primero proyecta nombres del protocolo en los 14 canales físicos, usando
VIS_TL_0/VIS_BR_0 y null para canales no adquiridos. No transforma los counts ni
promedia ciclos. El segundo renderiza estados, configuración y muestras, y conecta
Measurement/Setup a `#/measurement`/`#/setup`. La vista inactiva usa hidden y la
navegación marca aria-current. No desconecta ni envía comandos al cambiar de vista.

## Sesión y configuración

app conserva el flujo GATT anterior: selección → conexión → características →
suscripción → GET_STATUS/GET_CONFIG. busy impide órdenes de usuario solapadas y
las operaciones GATT siguen siendo secuenciales. APPLY espera contador/comando de
ACK y luego lee CONFIG, sin éxito optimista. Toda la validación definitiva sigue
en firmware; la presentación no incorpora registros ni conversiones hardware.

El formulario y ACTIVE están separados. Los valores editables no se sustituyen al
cambiar de vista. ACTIVE usa filas de texto desde una respuesta verificada del
ESP32. El LED mantiene el checkbox y su valor booleano; cambiar su aspecto a switch
no altera el payload. El texto ON/OFF sigue la posición solicitada, no afirma que
ya esté aplicada: el estado aplicado se consulta en ACTIVE.

## Muestras y estados

SampleAssembler permanece en protocol.mjs. app mantiene la secuencia, las omisiones,
la reconstrucción de fragmentos y la limpieza del ensamblador en desconexión.
La lectura visual permanece en STOP/desconexión y se limpia tras APPLY.
La notificación se procesa aunque Measurement esté oculta. ui dibuja nombres
físicos, mientras `sample` conserva en Advanced Debug el texto SMUX original.

Gain y saturación vienen del frame. Integration se toma de la configuración
confirmada asociada a la recepción, no de un nuevo campo de protocolo. Durante
APPLY o configuración desconocida no se atribuye una integración. Tras éxito se
limpia la muestra previa para no mezclarla con nuevos ajustes.

Badges incluyen texto y color: CONNECTED/VALID verde, RUNNING/MEASURING azul,
IDLE/DISCONNECTED gris, APPLYING/advertencia ámbar, SATURATED/ERROR rojo. La pérdida
de conexión actualiza cabecera y limpia valores de esa sesión. Los errores se
traducen a mensajes orientados al usuario; el código completo se conserva en el
log acotado de Advanced Debug.

## Compilar la Web y verificar

```powershell
node scripts/build-web.mjs
node scripts/verify-setup.mjs
node test/web_app.test.mjs
node test/web_protocol.test.mjs
```

El generador elimina imports/exports conocidos, integra los cinco módulos y CSS
en `web/dist/index.html` y rechaza dependencias no resueltas. No hay paquetes npm,
fuentes remotas, segundo documento Setup ni navegación que recargue la aplicación.
La salida abre directamente desde disco. Las comprobaciones usan DOM/GATT
simulados; revisar también el navegador real y la continuidad con el ESP32.

`spectral-chart.mjs` implementa el SVG RAW, mapping centralizado y escala fija ADC Full Scale.
Su API, longitudes de onda, extremos MAX/MIN y estados se documentan en el
[README web](../README.md#componente-espectral-hito-5a1).

## Capturas manuales

`captures.mjs`: CaptureTransport (request/response, stage, pull, CRC32), parser
CSV y createCaptureUI (modal LOG, tabla Data, GET_RECORD, borrado/recibo, descarga).
`app.mjs` proporciona acceso a la misma conexión y action/busy; las notificaciones
siguen entrando durante el modal y las transferencias. La muestra elegida se
fija por secuencia en firmware, no se envían canales/config inventados desde JS.
`protocol.mjs` añade UUID CAPTURES sin alterar SAMPLE/CONFIG/STATUS existentes.

`ui.mjs` exporta también `createRawHistogram(host, extrema, visHost)` con
`update(sample, fullScale)`: Measurement y Data comparten renderer, VIS y escala
por muestra. Data utiliza full scale y setup obtenidos en GET_RECORD. Rutas hash
incluyen `#/data`; navegar no crea otra sesión Bluetooth. Descripción máx. 80 bytes,
no 80 caracteres Unicode. No hay comparación superpuesta todavía.

Regenerar con `node scripts/build-web.mjs`. Ver `test/captures.test.mjs` para
freeze, cancelación, ACK diferido, descarga íntegra y errores. [Detalles de UX](../README.md#registro-manual-y-data).

El transporte rápido usa FAST_BEGIN/FAST_NEXT, 504 bytes útiles por lectura GATT
larga, listado compacto por `list()` y fila sin cabecera por `record(id)`.
`download()` se reserva para CSV íntegro. `recordCache` conserva filas verificadas
hasta refresh/mutación/desconexión; no vuelve a consultar al repetir VIEW.
Firmware anterior y hosts que truncan Read largo usan fallback legacy tras END.

El bloque final de style.css ajusta el escritorio al viewport mediante flex/grid
y min-height:0; SVG usa el alto disponible con su viewBox original. Las clases
data-list-card/saved-record-card delimitan tabla con scroll y replay fijo.
Un ajuste extra para alturas de hasta 640 px reduce espaciado de Setup. Bajo
960 × 560 CSS px sigue disponible el layout responsive con scroll accesible.

APPLY con éxito informa que el setup quedó aplicado y guardado para reinicio.
protocol.mjs reconoce error 24 CONFIG_SAVE_FAILED; app.mjs mantiene readback
actual, limpia la muestra anterior y muestra APPLIED / NOT SAVED sin invalidar
el sensor. El guardado se realiza en firmware, no mediante una segunda orden JS.
