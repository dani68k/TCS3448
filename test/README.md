# Comprobaciones de regresión

[Aplicación](../README_APP.md) · [Verificador de setup](../scripts/README.md).

Esta carpeta no se compila dentro del firmware ni se distribuye con el HTML.
Los archivos JS existentes se han adaptado a CONFIG v2 y la interfaz de setup.

| Archivo | Cobertura |
| --- | --- |
| `web_protocol.test.mjs` | Estado, configuración, orden de bytes, mapping de canales, fragmentos y desbordamiento |
| `web_app.test.mjs` | Conexión, Start/Stop, reconexión, sensor ausente y HTML local con GATT simulado |

Desde la raíz, con Node y sin paquetes externos:

```powershell
node scripts/build-web.mjs
node test/web_protocol.test.mjs
node test/web_app.test.mjs
node scripts/verify-setup.mjs
```

El último script añade los casos del hito 4: APPLY con ACK diferido, validación,
readback cuantizado simulado, rechazos y configuración desconocida.
Incluye también navegación Measurement/Setup durante adquisición, conservación de
sesión/formulario, lectura física de VIS y estados VALID/SATURATED. El DOM de prueba
común se implementa en `scripts/web-test-dom.mjs`, fuera del HTML de producción.
Todos son ensayos sin hardware; no prueban las escrituras I2C, el rollback real,
la radio ni el navegador visual. Seguir la
[matriz de aceptación](../docs/HITO_3_BLE.md#pasos-de-aceptación-en-hardware).
