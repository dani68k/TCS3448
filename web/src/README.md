# Fuentes de la Web

[Cliente Web](../README.md) · [Arquitectura de presentación](../../ARCHITECTURE.md).

## Estructura

| Archivo | Cometido |
| --- | --- |
| `index.template.html` | Un documento con Measurement y Setup; estado común y Advanced Debug |
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
la reconstrucción de cuatro fragmentos y la limpieza en STOP/APPLY/desconexión.
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

El generador elimina imports/exports conocidos, integra los tres módulos y CSS
en `web/dist/index.html` y rechaza dependencias no resueltas. No hay paquetes npm,
fuentes remotas, segundo documento Setup ni navegación que recargue la aplicación.
La salida abre directamente desde disco. Las comprobaciones usan DOM/GATT
simulados; revisar también el navegador real y la continuidad con el ESP32.
