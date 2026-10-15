# Cliente Web BLE

[Aplicación](../README_APP.md) · [Servicio BLE](../src/ble/README.md).

Permite seleccionar el ESP32, conectar/desconectar, enviar START/STOP, consultar
estado y configuración y mostrar los 18 canales de la última muestra completa.
La configuración es de solo lectura. No almacena un histórico ni modifica
parámetros del sensor.

## Usar la página

Abrir **`dist/index.html` con doble clic**, con Bluetooth activo y un navegador
compatible con Web Bluetooth. La página verifica `navigator.bluetooth` y
`window.isSecureContext`; avisa y deshabilita Connect si no puede usar la API.
El soporte depende del navegador y de sus restricciones para archivos locales.

El HTML contiene todos los estilos y scripts. No necesita servidor, Node,
conexión a Internet ni archivos auxiliares para abrirse. El firmware BLE debe
estar cargado en el ESP32. El mismo archivo puede alojarse después en GitHub
Pages mediante HTTPS.

## Flujo de la Web

1. Connect solicita selección de un dispositivo que anuncie el servicio ATLANTES.
2. Conecta a GATT y obtiene las cuatro características.
3. Activa notificaciones de estado y muestras.
4. Ejecuta GET_STATUS y GET_CONFIG de forma secuencial.
5. Start/Stop envían comandos y esperan confirmación en el estado.
6. Las notificaciones de muestra se reconstruyen y solo se muestran si están completas.
7. Disconnect intenta STOP y desconecta incluso si la orden falla. Una pérdida de
   conexión limpia suscripciones locales, configuración visible y muestra anterior.

El firmware detiene la adquisición al detectar la desconexión. Al reconectar la
Web obtiene nuevas características y renueva suscripciones; hay que volver a pulsar Start.

## Desarrollar

- [src/README.md](src/README.md): fuentes editables, protocolo y validaciones.
- [dist/README.md](dist/README.md): archivo generado que se utiliza/publica.
- [Generador](../scripts/README.md): cómo construir la página autocontenida.

Desde la raíz ejecutar `node scripts/build-web.mjs` tras editar `web/src/`.
No editar el HTML generado a mano. No hay dependencias npm que instalar.
Los pasos de aceptación en hardware están en la
[guía del hito 3](../docs/HITO_3_BLE.md#pasos-de-aceptación-en-hardware).
