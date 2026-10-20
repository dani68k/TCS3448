# Firmware de la aplicación

[Documentación general](../README_APP.md).

`main.cpp` conserva únicamente la coordinación de arranque y ejecución periódica.

| Etapa | Llamadas | Efecto |
| --- | --- | --- |
| `setup()` | Serial a 115200, espera inicial de 250 ms | Diagnóstico de arranque |
| `setup()` | `NVSManager::Load(config)` | Recuperar setup válido o conservar defaults |
| `setup()` | `TCS3448Manager::Init(config)` | Aplicar configuración recuperada; sensor detenido |
| `setup()` | `TCS3448BLE::Init()` | Crear servicio GATT y anunciar dispositivo |
| `loop()` | `TCS3448BLE::Update()` | Atender sesiones, comandos, adquisición y envío |
| `loop()` | `processSerialCommands()` y `delay(1)` | Descartar entrada serie y ceder tiempo |

BLE se inicializa aunque falle el sensor, para poder consultar el error desde la
Web. El inicio de adquisición depende de START; no se leen muestras continuamente
desde `main.cpp`.

Módulos:

- [TCS3448Manager](TCS3448Manager/README.md): control del sensor.
- [nevs_manager](nevs_manager/README.md): persistencia del setup con Preferences/NVS.
- [ble](ble/README.md): transporte y coordinación de comandos.
- [SerialCommand](SerialCommand/README.md): punto de entrada serie reservado.

El entorno y la distribución de flash se definen en [platformio.ini](../platformio.ini).
Los pasos de compilación, carga USB y prueba están en [README_APP.md](../README_APP.md#arrancar-y-probar).

## Capturas manuales

[DataStore](DataStore/README.md) administra LittleFS, CSV, FIFO de 100, IDs NVS y
recuperación. `ble/CaptureBLE` recibe snapshots al transmitir una muestra,
fija una copia al solicitar PIN y coordina LOG/CRUD/descarga desde loop.
`main.cpp` conserva su tamaño y flujo. No se crea una tarea de logging.
La inicialización de datos no impide consultar el sensor si falla el almacenamiento.
Pruebas de C++ real con dependencias simuladas: `scripts/test-storage.cmd`.

El transporte optimizado de datos mantiene las solicitudes BLE de 20 bytes y
usa respuestas de hasta 512: CSV completo, resúmenes o una fila sin cabecera.
El manager y la adquisición conservan su flujo; no hay cambios del archivo CSV.
