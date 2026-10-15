# Plantilla de documentación de módulo

Copiar esta estructura a `<carpeta-del-modulo>/README.md` al crear un nuevo
módulo y añadir su enlace a [README_APP.md](../README_APP.md).
Sustituir los textos orientativos por el comportamiento implementado.

## Nombre y propósito

Qué problema resuelve y cómo participa en el flujo de la aplicación.

## Archivos y responsabilidades

Tabla con cada archivo relevante y su cometido. Indicar qué parte de la
aplicación utiliza el módulo y de qué otros módulos depende.

## Interfaz, entradas y salidas

Funciones públicas, tipos de datos, unidades, propiedad/lifetime de datos,
retornos y condiciones para invocarlas. Indicar desde qué tarea o bucle se ejecuta.

## Configuración

Parámetros, valores predeterminados, límites, cuándo se aplican y si persisten.
Si no existe configuración, indicarlo.

## Flujo de funcionamiento

Explicar inicio, procesamiento y parada. Incluir un diagrama Mermaid cuando
aclare estados, dependencias o flujo de datos.

## Errores y recuperación

Cómo se notifican, qué se conserva/descarta y cómo recuperar el funcionamiento.

## Uso y verificación

Ejemplo mínimo, comandos existentes y pruebas manuales o automáticas aplicables.
Separar pruebas realizadas de comprobaciones todavía pendientes.

## Cambios que requieren actualizar otros módulos

Formatos de protocolo, muestras, calibraciones, almacenamiento o interfaces
compartidas. Enlazar las otras fuentes de documentación, sin afirmar que las
funciones futuras ya existen.
