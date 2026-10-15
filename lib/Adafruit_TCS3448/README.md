# Driver Adafruit_TCS3448

[Aplicación](../../README_APP.md) · [Gestor del sensor](../../src/TCS3448Manager/README.md).

Adaptador de bajo nivel para el TCS3448 que hereda de `Adafruit_AS7343` y utiliza
su mapa de registros y configuración. PlatformIO resuelve Adafruit AS7343 y sus
dependencias mediante `lib_deps`.

## Archivos y funciones

`Adafruit_TCS3448.h` define la clase, constantes TCS3448, tipos de canal y los
formatos de frame. `Adafruit_TCS3448.cpp` implementa:

| Método | Uso |
| --- | --- |
| `begin(wire)` | Inicialización heredada en dirección I2C `0x59` |
| `getDataReady(ready)` | Consultar AVALID con retorno de éxito de la comunicación |
| `readData(data)` | Leer estado y 18 resultados en una transacción consecutiva |
| `reset()` | Reset software y espera acotada de respuesta; requiere begin después |
| `getGPIOValue(value)` | Leer GPIO con comprobación de comunicación |
| `getFlickerStatus(status)` | Leer estado de flicker con comprobación de comunicación |

`readData()` empieza por ASTATUS y lee los 36 bytes de canales siguientes. Así
obtiene ganancia, saturación y canales del mismo frame. Convierte cada par de
bytes little-endian a uint16. El frame de registros ocupa 37 bytes y el modo
esperado es SMUX de 18 resultados. Consultar disponibilidad antes de leer.

La muestra de aplicación `TCS3448Sample` y su secuencia/timestamp se construyen
en el gestor, no en el driver. Este módulo no establece la cadencia, no publica
BLE y no calibra los ADC. La lectura heredada `readAllChannels()` es bloqueante;
el gestor usa `getDataReady()` y `readData()` para su adquisición por sondeo.

Los métodos con retorno bool permiten al gestor comunicar fallos. Validar las
lecturas y su coherencia en hardware al modificar acceso a registros; compilar
desde la raíz con `pio run -e esp32dev`.
