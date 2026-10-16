# Arquitectura de la interfaz del TCS3448

La [documentación global de la aplicación](README_APP.md) describe firmware,
sensor, configuración, adquisición y protocolo. Este documento concreta la
arquitectura de presentación y navegación, sin duplicar la referencia BLE.

## Un documento, tres vistas

La aplicación es una SPA ligera, sin framework ni dependencias externas:

```text
web/dist/index.html
    ├── #/measurement  → conexión, adquisición y última muestra
    ├── #/setup        → configuración solicitada y configuración activa
    └── #/debug        → resultados SMUX, estado tecnico y eventos BLE

app.mjs       → una sola sesión GATT y lógica de comandos existente
protocol.mjs  → formatos y decodificador existentes, sin cambios
ui.mjs        → navegación, estados y presentación de datos
style.css     → sistema visual compartido
```

No se crea `setup.html`. Navegar a otro archivo carga otro documento y no se
pueden conservar de forma fiable las instancias GATT y suscripciones del actual.
La [especificación Web Bluetooth](https://bluetooth.spec.whatwg.org/) vincula los
objetos Bluetooth al contexto global. Cambiar el fragmento de URL conserva ese
documento; la aplicación solo oculta/muestra vistas, sin reconectar ni enviar
START/STOP. También permite enlaces directos y navegación atrás/adelante.

Abrir `index.html#/setup` inicia la aplicación en Setup, todavía sin conexión.
El usuario puede ir a Measurement para conectar y regresar. Cambiar de vista no
borra el formulario, la última muestra ni los objetos BLE. Recargar, cerrar la
pestaña o abrir otra copia del HTML sí crea otra sesión.

## Responsabilidad de cada vista

Measurement muestra el dispositivo, controles Connect/Disconnect/Start/Stop,
estado de adquisición, canales físicos y resumen de la última muestra. Setup
contiene los únicos campos editables y la configuración activa como filas de
solo lectura, junto al control Read from device. Ambos comparten cabecera y
estado de conexión, errores comprensibles y Advanced / Raw Debug en una tercera vista del menu lateral.

El resumen principal usa F1, F2, FZ, F3, F4, F5, FY, FXL, F6, F7, F8, NIR,
VIS TL y VIS BR. Es una proyección de los nombres del protocolo, sin cambiar el
mapping del firmware. VIS TL/BR representan el primer ciclo SMUX; no se promedian
ni combinan los otros ciclos. Los canales no adquiridos aparecen como `—`, nunca
como un cero inventado. Advanced Debug conserva los resultados originales válidos
del modo activo (18 en SMUX18), estado técnico y los últimos 40 eventos BLE.

Gain y saturación del resumen vienen de la muestra. Integration se asocia a la
configuración confirmada cuando se recibe; no es un campo adicional de SAMPLE.
Si la configuración no está confirmada o APPLY está pendiente, se muestra `—`.
Después de un APPLY correcto se limpia la muestra anterior y se espera una nueva,
para no presentar una lectura antigua junto a los nuevos ajustes.

## Estado y sistema visual

Los estados del dispositivo siguen llegando del ESP32; la navegación no los
simula ni los altera. STOPPED se presenta como IDLE y RUNNING conserva su nombre.
VALID/SATURATED se calculan desde la muestra completa; errores de dispositivo
prevalecen en el indicador de estado. Los códigos de error detallados permanecen
en Advanced Debug y el mensaje principal explica la acción útil para el usuario.

`web/src/style.css` centraliza variables para fondo marino, paneles, bordes,
tipografía y colores semánticos: azul para actividad, verde para válido/conectado,
ámbar para espera/advertencia, rojo para error/saturación y gris para inactivo.
Siempre hay texto junto al color. Se usan fuentes del sistema, cifras tabulares,
focus visible y el input real del switch LED. Se incluye un histograma SVG de counts RAW.

El diseño se adapta con grid/flex para escritorio, portátil y tablet; en pantallas
estrechas apila Setup y reduce las columnas del readout. `hidden` elimina la vista
inactiva del layout y de la navegación por teclado.

## Generación y verificación

El [generador](scripts/README.md) integra los cuatro módulos y los estilos en un
único HTML local. No se duplican estilos entre páginas. Las fuentes permanecen
separadas y los BOM de UTF-8 se retiran antes de insertarlas en el documento.

Las comprobaciones de protocolo y configuración se conservan; las nuevas de UI
verifican navegación durante RUNNING sin órdenes ni desconexión, edición conservada,
recepción en la vista oculta, selección VIS y estados de muestra. La revisión visual
en navegador puede usar datos simulados, pero la conexión y continuidad con el
ESP32 deben comprobarse en hardware. Véase [Web README](web/README.md).

## Spectral Measurement

`app` entrega la muestra reconstruida a `ui.sample`, que actualiza el componente
`web/src/spectral-chart.mjs`. El componente no accede a BLE. Su definición
SPECTRAL_CHANNELS centraliza el orden F1, F2, FZ, F3, F4, F5, FY, FXL, F6, F7,
F8, NIR y sus centros 407, 424, 450, 473, 516, 546, 560, 596, 636, 687, 748,
855 nm. Busca por nombre y dibuja barras equidistantes. VIS TL/BR usan el primer
ciclo y aparecen en una tarjeta separada; no afectan a la escala espectral.

El SVG se crea una vez. Cada muestra actualiza alturas, valores y nombres
accesibles; actualiza los extremos MAX/MIN de la muestra actual. El viewBox
responde al ancho disponible sin reconstrucciones. La escala lineal va de cero a ADC Full Scale confirmado por firmware,
sin autoescala. Cada muestra conserva su full scale asociado. Sin muestra no hay barras inventadas. STOP y desconexión
conservan la última muestra; APPLY confirmado la limpia. Advanced Debug conserva
los resultados originales. Véase el README web para API y pruebas.

Setup ofrece LED AUTO ligado a RUNNING desde TCS3448Manager; no hay comandos
LED adicionales desde el navegador. START enciende y verifica el LED; STOP,
desconexion y fallo lo apagan. APPLY conserva la adquisicion previa y adapta
el LED al modo solicitado. CONFIG v3 comunica OFF/ON/AUTO con el mismo payload
de 20 bytes. Se mantienen lectura v2 y APPLY manual v2 por compatibilidad.
VIS usa dos barras horizontales apiladas TL/BR, referidas a ADC Full Scale.

## Navegacion lateral y controles compactos

La cabecera conserva titulo, estado BLE y un unico CONNECT/DISCONNECT para ambas
vistas. Measurement/Setup estan en un menu lateral. Se elimina la tarjeta Device;
el histograma contiene adquisicion IDLE/RUNNING y un unico START/STOP. Los botones
se actualizan en app.controls a partir de GATT y STATUS confirmados, y se bloquean
con busy para impedir operaciones solapadas. Desconectar intenta STOP y siempre
cierra GATT. Navegar conserva sesion, adquisicion, grafica y formulario.

El panel espectral elimina tooltip y seleccion. spectralExtrema calcula extremos
entre canales adquiridos de la muestra actual (sin VIS ni historico). La cabecera
muestra canal, nm y counts de MAX/MIN; los nombres del eje X se colorean verde/azul
y llevan una marca textual. Todos los empates se conservan; MAX tiene prioridad
de color cuando tambien es MIN. ADC Counts se dibuja vertical junto al eje Y.

Advanced / Raw Debug es una vista independiente (#/debug), fuera de Measurement
y Setup. La navegacion comparte una tabla de tres rutas y conserva BLE y muestras;
los datos de diagnostico se actualizan aunque Debug este oculto. Measurement
comienza directamente en la tarjeta del histograma, sin titulo de vista previo.

La informacion de medida esta centrada en la cabecera del histograma: Integration
time, Gain y Current LED. Se elimina el resumen inferior y su badge global;
secuencia y flag de saturacion original permanecen en Debug. SAT en rojo desde round(ADC Full Scale * 0.95) prevalece sobre SAT naranja naranja
desde round(ADC Full Scale * 0.90), MAX verde y MIN azul.
VIS no participa en SAT. El flag agregado de SAMPLE no identifica el canal:
no se usa para colorear todos los espectrales ni para inferir saturacion analogica.
