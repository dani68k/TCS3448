# Fuentes de la Web

[Cliente Web](../README.md) · [Aplicación](../../README_APP.md).

| Archivo | Responsabilidad |
| --- | --- |
| `index.template.html` | Estructura HTML con marcadores STYLE y SCRIPT |
| `style.css` | Estilos de la interfaz |
| `app.mjs` | Conexión GATT, controles, confirmaciones y presentación |
| `protocol.mjs` | UUID, códigos, decodificación y `SampleAssembler` |

`app.mjs` utiliza Web Bluetooth y actualiza el DOM. Evita operaciones de usuario
simultáneas con `busy` y secuencia las operaciones GATT. Para confirmar una orden
compara el contador y el código de respuesta del estado, consultándolo hasta
3,5 s con pausas de 100 ms. GET_CONFIG termina leyendo su característica.

`protocol.mjs` valida tamaño y versión y convierte campos little-endian. La
ganancia se convierte de código a multiplicador. El orden de los 18 canales debe
coincidir con el driver del firmware.

`SampleAssembler` recibe cuatro fragmentos consecutivos de una misma secuencia.
Un índice 0 inicia/reemplaza la reconstrucción. Descarta conjuntos con secuencia
distinta, huecos, duplicados intermedios o duración superior a 2 s. Al completar,
valida también la cabecera de muestra. `app.mjs` filtra secuencias repetidas o
antiguas y contabiliza omisiones entre muestras aceptadas, considerando el
desbordamiento uint32. La desconexión limpia la reconstrucción pendiente.

Estos archivos son módulos para desarrollar el código; no abrirlos como entrada
de la aplicación. El generador los convierte en un script clásico integrado en
`web/dist/index.html`, que sí se abre directamente desde disco.

Desde la raíz:

```powershell
node --check web/src/app.mjs
node --check web/src/protocol.mjs
node scripts/build-web.mjs
```

Al cambiar el protocolo, mantener sincronizados `src/ble/BLEProtocol.h`,
`src/ble/ble.cpp`, este decodificador y los formatos documentados. Probar el HTML
generado tras regenerarlo. No hay pruebas JS activas actualmente en `test/`.
