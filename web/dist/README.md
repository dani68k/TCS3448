# Web lista para usar

[Cliente Web](../README.md).

`index.html` es el resultado generado y autocontenido. Se puede abrir con doble
clic, copiar a otro directorio o publicar en un alojamiento HTTPS como GitHub Pages.
Para distribuir la aplicación solo se necesita ese HTML; este README no es una
dependencia de ejecución.

Incluye CSS y JavaScript clásico; no hace importaciones de módulos ni carga
recursos auxiliares. Requiere un navegador que permita Web Bluetooth en el contexto
abierto, Bluetooth activo y el firmware ATLANTES cargado en el ESP32.

No editarlo manualmente. Editar `web/src/` y ejecutar desde la raíz:

```powershell
node scripts/build-web.mjs
```
