# Driver Adafruit_TCS3448

[Aplicación](../../README_APP.md) · [Gestor](../../src/TCS3448Manager/README.md).

Adaptador local sobre Adafruit_AS7343. Encapsula el mapa de registros compatible
y la dirección del TCS3448. PlatformIO resuelve Adafruit AS7343 y BusIO mediante
`lib_deps`. El driver no define cadencia, protocolo BLE ni configuración de la Web.

## Archivos y API

`Adafruit_TCS3448.h` define clase, aliases, canales, frames de registro y
`tcs3448_settings_t`, un snapshot exclusivo de la capa sensor/driver.
`Adafruit_TCS3448.cpp` implementa:

| Método | Responsabilidad |
| --- | --- |
| `begin(wire)` | Inicialización heredada usando la dirección TCS3448 |
| `getDataReady(ready)` | AVALID con retorno de error de comunicación |
| `readData(data, channelCount=18)` | Lectura coherente de 6/12/18 resultados y flags |
| `readConfiguration(settings)` | Lectura comprobada de todos los ajustes de setup |
| `writeConfiguration(settings)` | Escritura mediante setters existentes; LED habilitado al final |
| `reset()` | Reset software y espera acotada de respuesta; requiere begin después |
| `getGPIOValue(value)` / `getFlickerStatus(status)` | Lecturas con error explícito |

## Lectura y configuración

readData empieza por ASTATUS, que fija el frame, y lee 1 + channelCount × 2 bytes.
El buffer máximo es 37 bytes; los canales restantes se inicializan a cero. Gain
y saturación proceden de ese mismo frame. No inicia ni espera la integración;
consultar disponibilidad antes y usar modo SMUX consistente con channelCount.

Los getters heredados devuelven un valor y no permiten al gestor distinguir un
fallo I2C de un registro que valga cero. readConfiguration usa lecturas bool para
CFG1 (0xC6), ATIME (0x81), ASTEP (0xD4/D5), CFG20 (0xD6), AZ_CONFIG (0xDE) y LED
(0xCD), con banco comprobado. Solo entrega el snapshot si todas las lecturas van bien.

writeConfiguration reutiliza setGain, setATIME, setASTEP, setSMUXMode,
setAutoZeroFrequency, setLEDCurrent y enableLED de la API instalada. No inventa
modos de autozero. El manager valida antes, detiene el motor antes de cambiar
SMUX/timing y verifica después: este método no promete transacción atómica.

El snapshot del driver contiene códigos hardware, ATIME/ASTEP y frecuencia de
autozero; nunca se expone al cliente Web. La conversión de integración en ms,
ADC full scale, validación y rollback pertenecen al gestor.

Compilar con `pio run -e esp32dev`; validar en hardware readback, saturación y
coherencia de los tres modos SMUX al modificar acceso a registros. La API heredada
readAllChannels sigue siendo bloqueante; no es el camino de adquisición BLE.
