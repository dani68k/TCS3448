# Firmware de la aplicación

[Documentación general](../README_APP.md).

`main.cpp` conserva únicamente la coordinación de arranque y ejecución periódica.

| Etapa | Llamadas | Efecto |
| --- | --- | --- |
| `setup()` | Serial a 115200, espera inicial de 250 ms | Diagnóstico de arranque |
| `setup()` | `TCS3448Manager::Init()` | Aplicar configuración; sensor detenido |
| `setup()` | `TCS3448BLE::Init()` | Crear servicio GATT y anunciar dispositivo |
| `loop()` | `TCS3448BLE::Update()` | Atender sesiones, comandos, adquisición y envío |
| `loop()` | `processSerialCommands()` y `delay(1)` | Descartar entrada serie y ceder tiempo |

BLE se inicializa aunque falle el sensor, para poder consultar el error desde la
Web. El inicio de adquisición depende de START; no se leen muestras continuamente
desde `main.cpp`.

Módulos:

- [TCS3448Manager](TCS3448Manager/README.md): control del sensor.
- [ble](ble/README.md): transporte y coordinación de comandos.
- [SerialCommand](SerialCommand/README.md): punto de entrada serie reservado.

El entorno y la distribución de flash se definen en [platformio.ini](../platformio.ini).
Los pasos de compilación, carga USB y prueba están en [README_APP.md](../README_APP.md#arrancar-y-probar).
