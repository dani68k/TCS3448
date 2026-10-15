# Arquitectura de la interfaz del TCS3448

La [documentación global de la aplicación](README_APP.md) describe firmware,
sensor, configuración, adquisición y protocolo. Este documento concreta la
arquitectura de presentación y navegación, sin duplicar la referencia BLE.

## Un documento, dos vistas

La aplicación es una SPA ligera, sin framework ni dependencias externas:

```text
web/dist/index.html
    ├── #/measurement  → conexión, adquisición y última muestra
    └── #/setup        → configuración solicitada y configuración activa

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
estado de conexión, errores comprensibles y Advanced Debug colapsado por defecto.

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
focus visible y el input real del switch LED. No se incluyen gráficas ni histogramas.

El diseño se adapta con grid/flex para escritorio, portátil y tablet; en pantallas
estrechas apila Setup y reduce las columnas del readout. `hidden` elimina la vista
inactiva del layout y de la navegación por teclado.

## Generación y verificación

El [generador](scripts/README.md) integra los tres módulos y los estilos en un
único HTML local. No se duplican estilos entre páginas. Las fuentes permanecen
separadas y los BOM de UTF-8 se retiran antes de insertarlas en el documento.

Las comprobaciones de protocolo y configuración se conservan; las nuevas de UI
verifican navegación durante RUNNING sin órdenes ni desconexión, edición conservada,
recepción en la vista oculta, selección VIS y estados de muestra. La revisión visual
en navegador puede usar datos simulados, pero la conexión y continuidad con el
ESP32 deben comprobarse en hardware. Véase [Web README](web/README.md).
