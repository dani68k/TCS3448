# Cliente Web del TCS3448

[Aplicación FW/BLE](../README_APP.md) · [Arquitectura de UI](../ARCHITECTURE.md) ·
[Protocolo BLE](../docs/HITO_3_BLE.md).

Interfaz de instrumento en dark mode. Una sola aplicación con dos vistas:
**Measurement** para conexión/adquisición y **Setup** para configuración.
Los UUID, mensajes y lógica del sensor no cambian en este rediseño.

## Abrir y navegar

Abrir `dist/index.html` con doble clic en un navegador que permita Web Bluetooth
en archivos locales y activar Bluetooth. El HTML es autocontenido: no requiere
servidor, Node, Internet ni recursos externos para utilizarlo. Puede alojarse después
mediante HTTPS, por ejemplo en GitHub Pages.

- `index.html#/measurement`: vista principal, predeterminada.
- `index.html#/setup`: vista de configuración.

No existe un segundo documento `setup.html`: la navegación cambia el fragmento
URL y muestra/oculta vistas sin recargar. Así se conservan la sesión GATT,
suscripciones, adquisición, última muestra y formulario editado. Atrás/adelante
funcionan mediante hashchange. Recargar o cerrar sí destruye la sesión de página.
La [decisión SPA](../ARCHITECTURE.md#un-documento-dos-vistas) evita duplicar BLE.

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

La tarjeta Device contiene Connect/Disconnect, Start/Stop y adquisición IDLE o
RUNNING, procedentes del firmware. La cabecera muestra CONNECTED, CONNECTING o
DISCONNECTED. Un fallo de sensor se identifica como ERROR.

Latest Sample muestra F1, F2, FZ, F3, F4, F5, FY, FXL, F6, F7, F8, NIR, VIS TL y
VIS BR como readouts de counts. VIS usa el primer ciclo SMUX; no hay promedio ni
cambio de mapping. Los otros resultados VIS quedan en Raw Debug. Un canal no
adquirido aparece como `—`; un ADC cero válido se sigue mostrando como cero.

La banda inferior muestra número de muestra, gain real del frame, integración
asociada a la configuración confirmada y estado VALID/SATURATED. Sin muestra se
muestra IDLE o MEASURING. La integración no está incluida en el paquete SAMPLE:
si no es conocida o APPLY está en curso, no se inventa. Después de APPLY correcto
se espera una nueva muestra antes de mostrar el resumen.

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
del error están en Advanced Debug. START/STOP no alteran el LED; desconexión lo apaga
según el comportamiento ya implementado en firmware. No hay persistencia tras reinicio.

Si se abre directamente Setup sin conectar, se ofrece volver a Measurement para
conectar. Cambiar de vista durante RUNNING no envía STOP ni inicia otra conexión.

## Advanced Debug y estilos

Sección colapsada por defecto, compartida entre vistas. Conserva resultados SMUX
originales válidos (18 en modo 18), secuencia, timestamp, saturación, estado técnico,
contadores de recepción y los últimos 40 eventos BLE. No añade mensajes en cada
sample ni expone logs técnicos largos en la interfaz principal.

La paleta está centralizada en variables CSS: marino/charcoal, paneles azul grisáceo,
cian/azul técnico, verde válido, ámbar pendiente y rojo para error/saturación. Usa
fuentes del sistema, cifras tabulares, focus visible y texto junto a cada color.
No hay gráficos, histogramas, barras, normalización ni dependencias visuales externas.

## Desarrollar y verificar

Editar fuentes, no el HTML generado. Desde la raíz:

```powershell
node --check web/src/app.mjs
node --check web/src/ui.mjs
node --check web/src/protocol.mjs
node scripts/build-web.mjs
node scripts/verify-setup.mjs
node test/web_app.test.mjs
node test/web_protocol.test.mjs
```

No se necesitan paquetes npm. El generador integra protocolo, UI y app como un
script clásico y retira BOM de las fuentes antes de incrustarlas. El CSS se mantiene
en un solo archivo fuente; no se duplica entre páginas.

Las pruebas simuladas comprueban navegación sin órdenes adicionales, conexión,
START/STOP, GET/APPLY, recepción mientras Setup está visible, estados, readouts
físicos y debug original. No sustituyen radio ni sensor reales. En hardware:

1. Connect desde Measurement, Start y comprobar lecturas.
2. Abrir Setup durante RUNNING: CONNECTED debe mantenerse y no cesar la adquisición.
3. Editar/APPLY, esperar configuración válida y volver a Measurement para ver muestras nuevas.
4. Comprobar Read from device, Stop y reconexión; distinguir VALID de SATURATED.
5. Abrir Advanced Debug para comparar los 18 resultados con la vista física.
6. Comprobar escritorio y tablet, teclado/focus y ausencia de desbordamiento horizontal.
