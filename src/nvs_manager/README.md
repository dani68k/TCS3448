# Persistencia del setup TCS3448

`nvs_manager.h/.cpp`, namespace `NVSManager`, gestiona el setup con
`Preferences.h` en la carpeta `src/nevs_manager` solicitada.

## Flujo

1. En arranque, `main.cpp` crea `TCS3448Config` con sus valores predeterminados.
2. `Load(config)` recupera y valida los ajustes antes de `TCS3448Manager::Init(config)`.
   Sin datos, NVS no disponible o contenido inválido, deja esos valores intactos.
3. APPLY_CONFIG primero aplica y verifica el sensor. Solo después de éxito, BLE
   llama a `Save(GetConfig())`: se guarda el setup real, incluida integración
   efectiva y modo LED OFF/ON/AUTO. Una petición rechazada nunca sobrescribe NVS.
4. El ACK de APPLY llega después del commit y su verificación. Si falla guardar,
   STATUS devuelve result=6, error=24 (`CONFIG_SAVE_FAILED`); CONFIG conserva el
   readback nuevo. La web indica **APPLIED / NOT SAVED**, sin fingir rollback.

No hay guardado periódico ni escrituras al iniciar/parar adquisición o al
desconectar. Reaplicar el mismo setup evita escribir de nuevo. El estado RUNNING,
la secuencia, los canales y los ajustes de bus I2C no se persisten aquí.

## Datos y validación

Namespace NVS `tcs3448_setup`, clave única `config`, blob de 32 bytes:

| Offset | Contenido |
| --- | --- |
| 0..3 | Firma ASCII `TCS8` |
| 4 | Versión 1 |
| 5 | Gain, código del modelo |
| 6 | Auto SMUX: 6/12/18 |
| 7 | Autozero: 0/1/2 |
| 8 | LED: 0=OFF, 1=ON, 2=AUTO |
| 9 | Reservado, cero |
| 10..11 | Corriente LED uint16 |
| 12..19 | Integración ms, IEEE754 binary64 |
| 20..23 | Periodo ms uint32 |
| 24..27 | Reservado, cero |
| 28..31 | CRC32 IEEE de los primeros 28 bytes |

Enteros y bits del double se codifican explícitamente en little endian, sin
guardar padding de structs. Se validan firma, tamaño, versión, reservados, CRC,
modo LED y todos los rangos/timing mediante `TCS3448ConfigMath::Validate`.
Load solo modifica el argumento cuando toda la configuración es válida; conserva
los pines y frecuencia I2C establecidos por el firmware.

Save usa un único `putBytes`, que en el framework instalado llama a
`nvs_set_blob` y `nvs_commit`. Comprueba longitud devuelta y lee/compara los 32
bytes antes de confirmar. No mezcla claves independientes de distintas versiones
del setup. No formatea ni borra NVS ante un error. Los IDs de capturas usan otro
namespace (`captures`) y el CSV LittleFS no se modifica.

## LED y arranque

Se recupera el modo AUTO, no el estado transitorio del LED durante una captura.
La adquisición arranca detenida, por lo que AUTO queda apagado hasta START.
OFF/ON manual también se guardan; se mantienen las reglas existentes del manager:
la apertura/cierre de sesión BLE fuerza LED apagado. Ese apagado no sobrescribe
la última configuración guardada con APPLY. El resto del setup se conserva.

## API y pruebas

`Load`, `Save` y `ResultName`; resultados `Ok`, `NotFound`, `InvalidData`,
`Unavailable`, `WriteFailed`. No hay dependencia de BLE ni acceso al sensor.

`test/nvs_manager_test.cpp` ejecuta el módulo y el validador reales con Preferences
simulado: OFF/ON/AUTO, precisión, bus preservado, evitar escrituras repetidas,
aislamiento del contador de capturas, datos truncados/corruptos/inválidos y fallos
de lectura/escritura/verificación. `scripts/test-storage.cmd` incluye este test.
La web prueba el ACK de persistencia fallido manteniendo la configuración aplicada.

Pendiente en ESP32: APPLY con valores diferentes y AUTO, reiniciar alimentación,
conectar y comprobar el readback; START/STOP debe conservar AUTO. Una petición
inválida no debe cambiar el setup recuperado tras reinicio. Comprobar también
primera ejecución y que las capturas previas sigan disponibles. No requiere
uploadfs, cambios de partición ni borrado de datos.
