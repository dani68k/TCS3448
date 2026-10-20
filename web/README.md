# Cliente Web del TCS3448

[Aplicación FW/BLE](../README_APP.md) · [Arquitectura de UI](../ARCHITECTURE.md) ·
[Protocolo BLE](../docs/HITO_3_BLE.md).

Interfaz de instrumento en dark mode. Una sola aplicación con cuatro vistas:
**Measurement** para adquisición, **Setup** para configuración y
**Data** para capturas y **Advanced / Raw Debug** para diagnostico.
Los UUID y mensajes originales del sensor se conservan; CAPTURES amplía el servicio.

## Abrir y navegar

Abrir `dist/index.html` con doble clic en un navegador que permita Web Bluetooth
en archivos locales y activar Bluetooth. El HTML es autocontenido: no requiere
servidor, Node, Internet ni recursos externos para utilizarlo. Puede alojarse después
mediante HTTPS, por ejemplo en GitHub Pages.

- `index.html#/measurement`: vista principal, predeterminada.
- `index.html#/setup`: vista de configuración.
- `index.html#/data`: capturas persistidas en el ESP32.
- `index.html#/debug`: vista de diagnostico RAW y BLE.

No existe un segundo documento `setup.html`: la navegación cambia el fragmento
URL y muestra/oculta vistas sin recargar. Así se conservan la sesión GATT,
suscripciones, adquisición, última muestra y formulario editado. Atrás/adelante
funcionan mediante hashchange. Recargar o cerrar sí destruye la sesión de página.
La [decisión SPA](../ARCHITECTURE.md#spa-y-estados) evita duplicar BLE.

## Archivos y responsabilidades

| Archivo | Responsabilidad |
| --- | --- |
| `src/index.template.html` | Cabecera común, vistas Measurement/Setup y Advanced Debug |
| `src/style.css` | Paleta, componentes, estados, tipografía y responsive compartidos |
| `src/ui.mjs` | Navegación, readouts físicos, configuración legible y badges |
| `src/app.mjs` | Implementación única de conexión, comandos, ACK y recepción |
| `src/protocol.mjs` | Formatos existentes, validación y reconstrucción de muestras |
| `dist/index.html` | Documento generado listo para abrir/publicar |
| `images/logo-signal.png` | Logo de cabecera e icono de pestaña, incrustado en el HTML generado |

## Measurement

El titulo permanece en la cabecera y Measurement/Setup forman un menu lateral.
La cabecera comparte estado BLE y un unico boton CONNECT/DISCONNECT accesible
desde ambas vistas. En el histograma hay un unico boton START/STOP junto al
estado IDLE/RUNNING. Los textos siguen el estado confirmado del dispositivo;
ambos controles se bloquean mientras hay una operacion pendiente.
Un fallo de sensor se identifica como ERROR. La tarjeta Device se ha retirado.

Spectral Measurement representa 12 canales RAW mediante SVG propio, sin dependencias.
BROADBAND VIS muestra VIS TL y VIS BR fuera del histograma, usando exclusivamente
VIS_TL_0 y VIS_BR_0. Los otros ciclos permanecen en Advanced Debug.
Los canales no adquiridos aparecen como `—`; cero es un valor válido.

La cabecera del histograma centra Integration time, Gain y Current LED. Integration
y corriente proceden de la configuracion confirmada; Gain usa el frame al recibir
una muestra. Antes de recibir muestras se muestran los ajustes confirmados.
STOP y desconexion conservan la informacion de la ultima muestra. Durante APPLY
no se atribuyen ajustes sin confirmar a una muestra; tras exito se limpia la anterior.
El numero de muestra y la saturacion global quedan en Advanced / Raw Debug.

## Setup

El formulario REQUESTED contiene gain, integración ms, corriente LED, switch LED,
SMUX, autozero y periodo. El input checkbox del LED sigue siendo real y accesible;
el switch es únicamente su presentación. No se escribe al cambiar un campo.

APPLY CONFIGURATION conserva la validación y ACK existentes. ACTIVE muestra filas
`dt/dd` de solo lectura, no inputs deshabilitados: gain, integración real,
corriente/estado LED, SMUX, autozero, periodo y ADC Full Scale.

Read from device ejecuta GET_STATUS/GET_CONFIG. Al conectar/reconectar se rellena
todo desde firmware. Un rechazo conserva REQUESTED y la configuración activa
anterior; se muestra un error útil sin dar éxito anticipado. Los detalles internos
del error están en Advanced Debug. En modo manual START/STOP conservan el LED; en AUTO START lo enciende y STOP lo apaga; desconexión lo apaga
según el comportamiento ya implementado en firmware. No hay persistencia tras reinicio.

Si se abre directamente Setup sin conectar, se ofrece volver a Measurement para
conectar. Cambiar de vista durante RUNNING no envía STOP ni inicia otra conexión.

## Advanced Debug y estilos

Vista independiente accesible desde el menu lateral, oculta al iniciar en Measurement/Setup. Conserva resultados SMUX
originales válidos (18 en modo 18), secuencia, timestamp, saturación, estado técnico,
contadores de recepción y los últimos 40 eventos BLE. No añade mensajes en cada
sample ni expone logs técnicos largos en la interfaz principal.

La paleta está centralizada en variables CSS: marino/charcoal, paneles azul grisáceo,
cian/azul técnico, verde válido, ámbar pendiente y rojo para error/saturación. Usa
fuentes del sistema, cifras tabulares, focus visible y texto junto a cada color.
El histograma presenta counts RAW sin normalización ni dependencias visuales externas.

## Desarrollar y verificar

Editar fuentes, no el HTML generado. Desde la raíz:

```powershell
node --check web/src/app.mjs
node --check web/src/ui.mjs
node --check web/src/spectral-chart.mjs
node --check web/src/protocol.mjs
node scripts/build-web.mjs
node scripts/verify-setup.mjs
node test/web_app.test.mjs
node test/web_protocol.test.mjs
node test/spectral_chart.test.mjs
```

No se necesitan paquetes npm. El generador integra protocolo, UI y app como un
script clásico y retira BOM de las fuentes antes de incrustarlas. El CSS se mantiene
en un solo archivo fuente; no se duplica entre páginas.

Las pruebas simuladas comprueban navegación sin órdenes adicionales, conexión,
START/STOP, GET/APPLY, recepción mientras Setup está visible, estados, readouts
físicos y debug original. No sustituyen radio ni sensor reales. En hardware:

1. CONNECT en la cabecera, Start y comprobar lecturas.
2. Abrir Setup durante RUNNING: CONNECTED debe mantenerse y no cesar la adquisición.
3. Editar/APPLY, esperar configuración válida y volver a Measurement para ver muestras nuevas.
4. Comprobar Read from device, Stop y reconexión; distinguir VALID de SATURATED.
5. Abrir la pestaña Advanced / Raw Debug para comparar los 18 resultados con la vista física.
6. Comprobar escritorio y tablet, teclado/focus y ausencia de desbordamiento horizontal.

## Componente espectral (Hito 5A.1)

`src/spectral-chart.mjs` centraliza nombres, longitudes de onda y colores:

| Canal | nm | Canal | nm |
| --- | --- | --- | --- |
| F1 | 407 | F2 | 424 |
| FZ | 450 | F3 | 473 |
| F4 | 516 | F5 | 546 |
| FY | 560 | FXL | 596 |
| F6 | 636 | F7 | 687 |
| F8 | 748 | NIR | 855 |

El orden es el de la tabla por filas. `spectralReadings(sample)` busca nombres
en `sample.channels` (pares nombre/counts); no depende del orden de recepción.
`createSpectralChart(host)` inicializa una vez y devuelve `update(sample)`.
El flujo es SampleAssembler → app → ui.sample → chart.update, sin segunda suscripción.
VIS no interviene en el máximo ni en la escala del histograma.

El eje Y es fijo entre 0 y ADC Full Scale de la configuracion confirmada por el
ESP32; no depende del maximo de la muestra. Se actualiza al confirmar nuevos
ajustes y se asocia a la muestra para conservar su escala al desconectar.
Las barras son mas anchas, con separacion estrecha. VIS TL aparece encima de
VIS BR, como niveles horizontales de izquierda a derecha, con el mismo full scale.
Los valores numericos siguen mostrando los counts recibidos incluso en saturacion.
STOP y desconexion conservan la ultima lectura; APPLY confirmado la limpia.

El panel muestra MAX y MIN de la muestra actual, con canal, longitud de onda y
counts. Los nombres de canal MAX se muestran en verde, MIN en azul y los demas
en blanco. Tambien se dibujan marcas MAX/MIN para no depender solo del color.
Los canales ausentes y VIS no participan; cero es valido. Se marcan todos los
empates; si maximo y minimo coinciden se indica MAX/MIN con prioridad verde.
No se guarda un historico de extremos. No hay tooltip, hover ni seleccion de barra.
ADC Counts aparece vertical junto al eje Y. Measurement no tiene titulo superior: el contenido comienza directamente en el histograma.
El SVG escala por viewBox y ancho CSS, conservando los nodos entre muestras.

Validar en hardware: Connect → Start, comparar cada barra con Raw Debug, probar
STOP/desconexión/reconexión, cambiar Gain/Integration mediante APPLY y comprobar
nuevas lecturas. Revisar los extremos y el layout en tablet.
Las pruebas automatizadas usan muestras aisladas de producción; no simulan datos
en la aplicación distribuida. El HTML se abre localmente: no está embebido en firmware.

## LED AUTO con Measurement

En Setup activar LED AUTO y pulsar APPLY CONFIGURATION. El switch manual queda
inhabilitado mientras AUTO esta seleccionado. El ESP32 enciende el LED durante
RUNNING y lo apaga con STOP, desconexion o fallo. La preferencia AUTO permanece
hasta otro APPLY o reinicio; reconectar recupera el modo sin iniciar adquisicion.
APPLY durante RUNNING adapta el estado del LED al nuevo modo.

AUTO requiere cargar el firmware actualizado: CONFIG usa version 3, con modo LED
0 OFF / 1 ON / 2 AUTO. El cliente lee CONFIG v2/v3; las escrituras manuales
siguen usando v2 y AUTO usa v3. Los paquetes siguen ocupando 20 bytes.

La conexion tambien puede iniciarse directamente desde Setup. DISCONNECT intenta
STOP antes de cerrar GATT, incluso si Measurement esta oculta. Un cambio de vista
no altera el estado ni envia comandos. START cambia a STOP solo tras confirmar
RUNNING; un STOP confirmado o una desconexion lo devuelve a START.

El layout ocupa todo el ancho de la ventana, sin margen exterior antes del menu.
La cabecera y el menu permanecen visibles: solo el contenido principal tiene
scroll vertical; el lateral dispone de scroll propio si falta altura. La altura
usa el viewport dinamico para adaptarse tambien a la cabecera en movil.
Measurement comienza directamente en la tarjeta del histograma.

SAT se muestra en rojo desde round(ADC Full Scale * 0.95). SAT naranja se muestra
en naranja desde round(ADC Full Scale * 0.90) hasta antes del umbral SAT.
La prioridad de color y marca es SAT > SAT naranja > MAX (verde) > MIN (azul).
Con ADC Full Scale 53957: SAT desde 51259, SAT naranja desde 48561 hasta 51258.
Los umbrales usan la configuracion confirmada para la muestra. Solo participan
F1-F8, FZ, FY, FXL y NIR; VIS TL/BR nunca reciben marca SAT. El flag global BLE
no se propaga a todas las barras porque no identifica canales ni distingue VIS.
Puede indicar saturacion analogica sin un RAW a full scale; ese flag original se
conserva en Debug, sin atribuirlo a un canal. Los counts numericos no se alteran.

## Registro manual y Data

Si el servicio no anuncia CAPTURES (`34480006`), la conexión conserva Measurement
y Setup, deshabilita LOG/Data y muestra el diagnóstico. Cargar el firmware 5B;
si ya está cargado, renovar el emparejamiento Bluetooth antes de reconectar.

LOG está junto a START/STOP; requiere BLE y muestra válida. Se congela la
secuencia al pulsar y firmware confirma PIN antes de mostrar el modal oscuro.
Admite descripción opcional, máximo 80 bytes UTF-8. CANCELAR no modifica flash;
GUARDAR espera ACK real, bloquea doble clic y muestra ID asignado por ESP32.
Si está STOPPED se indica que se guarda la última muestra visible. Notificaciones
posteriores no cambian la captura. Error/desconexión no reintenta LOG.

Data conserva SPA/GATT, muestra contador N / 100 y estado de almacenamiento.
La tabla es descendente por ID; REFRESH pide solo los seis campos del listado
y valida su CRC. Reconectar refresca automáticamente. VIEW pide una fila al
ESP32 la primera vez y después reutiliza la copia verificada en memoria. Usa el mismo `createRawHistogram` de Measurement (incluidos VIS, MAX/MIN,
90/95 % SAT), con ADC Full Scale histórico. El texto distingue Saved record y
Last loaded record desconectado. Counts ausentes no se sustituyen por cero.

Los checkbox permiten borrar varias filas; DELETE SELECTED confirma cantidad e
IDs y espera ACK/recibo de los IDs exactos. Una selección borrada limpia replay.
DOWNLOAD CSV usa bytes originales del archivo, longitud y CRC32 verificados,
Blob text/csv UTF-8, nombre data.csv, sin reformateo de números. El pull por
bloques de hasta 504 bytes útiles, con lecturas GATT de hasta 512 bytes; muestra
progreso y permite cancelar. El firmware ofrece MTU 517, negociado por el host.
Acquisición sigue funcionando; mutaciones quedan BUSY durante la transferencia.

`captures.mjs` contiene UI/transport/parser; `app.mjs` comparte su bloqueo de
acciones con Setup/START/STOP. `ui.mjs` extrae el wrapper de histograma y VIS
para reutilizarlo en ambas vistas; el renderer espectral conserva su lógica.
El CSV es UTF-8 y el timestamp es uptime ESP32, no fecha UTC.
[Esquema y fallos](../src/DataStore/README.md) · [protocolo](../src/ble/README.md#capturas-manuales).

Pruebas: `node test/captures.test.mjs` y regresiones de test/ más verify-setup.
Son pruebas con DOM/GATT simulados; navegador/radio/flash reales pendientes.
Compare with live measurement no se implementa hasta esa validación.

La caché de VIEW se invalida con REFRESH, LOG/DELETE y desconexión. La primera
consulta del registro rápido no repite la cabecera CSV ni descarga otras filas.
Con firmware anterior, las filas completas ya descargadas en REFRESH también se
reutilizan para VIEW. Hay detección automática del transporte legacy y fallback
si el host no entrega las lecturas largas completas. El CSV persistente no cambia:
actualizar firmware y abrir el HTML regenerado, sin volver a subir `data/`.

## Vista ajustada a la ventana

El escritorio adapta automáticamente las vistas al alto y ancho disponibles
(desde 960 × 560 CSS px). Histograma flexible, VIS y controles visibles juntos;
Setup compacto; Data muestra tabla y gráfico lado a lado. Las filas de Data y
los paneles Debug desplazan su contenido dentro del panel, sin alargar la página.
En ventanas pequeñas se conserva scroll para no ocultar funciones ni reducir
el texto con zoom. Basta abrir el HTML regenerado; no requiere actualizar FW.

## Guardar Setup para el siguiente arranque

APPLY CONFIGURATION ahora confirma aplicación y guardado NVS con el firmware
actualizado. Tras reiniciar y reconectar, los campos se rellenan desde el setup
recuperado y verificado por ESP32, incluido LED AUTO. Si solo falla guardar,
CONFIG muestra los ajustes nuevos y el estado APPLIED / NOT SAVED pide reintentar
APPLY; no afirma que se conservara la configuración anterior. Error 24 =
CONFIG_SAVE_FAILED. No hay guardado en localStorage ni valores JS como fuente
de verdad. [Detalles](../src/nevs_manager/README.md).
